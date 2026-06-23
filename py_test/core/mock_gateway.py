# core/mock_gateway.py
# -*- coding: utf-8 -*-

import threading
import time

from core import protocol
from core.behavior_profile import BehaviorProfile
from core.command_dispatcher import CommandDispatcher
from core.gateway_config import GatewayConfig, legacy_default_config
from core.gateway_state import GatewayState
from core.mqtt_bus import MqttBus
from handlers import create_handlers


class MockGateway:
    def __init__(
        self,
        gateway_config: GatewayConfig = None,
        behavior_profile: BehaviorProfile = None,
        index: int = 0,
    ):
        self.config = gateway_config or legacy_default_config()
        self.behavior = behavior_profile or BehaviorProfile.from_config(self.config.behavior)
        self.index = index
        self.running = False
        self.started = False
        self.started_at = 0.0
        self.alarm_states = {}
        self.spike_states = {}

        self.state = GatewayState(self.config.ports, self.config.default_port_id)
        self.dispatcher = CommandDispatcher(create_handlers())

        self.bus = MqttBus(
            gateway_config=self.config,
            on_message=self.on_mqtt_message,
            on_connected=self.on_mqtt_connected,
            client_suffix=str(index),
        )

    def start(self):
        self.running = True
        self.bus.connect()

    def connect_forever(self):
        self.running = True
        self.bus.connect_forever()

    def on_mqtt_connected(self):
        if self.started:
            return

        self.started = True
        self.started_at = time.time()

        self._log("mqtt connected")
        self.state.init_from_config(self.config.ports)

        self.publish_register()
        self.publish_snapshot("startup")

        threading.Thread(target=self.telemetry_loop, daemon=True).start()
        threading.Thread(target=self.heartbeat_loop, daemon=True).start()

    def publish_register(self):
        self.publish(protocol.gateway_register(self.config))

    def on_mqtt_message(self, topic, data):
        if data.get("type") == "ack":
            self._log("ignore ack message")
            return

        cmd = data.get("cmd") or data.get("command") or data.get("commandType") or data.get("action") or ""
        cmd_id = str(data.get("cmd_id") or data.get("cmdId") or data.get("commandId") or "")
        self._log(f"command received cmd={cmd} cmd_id={cmd_id}")

        if cmd in {"gateway_register", "config_snapshot", "device_config_snapshot"}:
            self._log(f"ignore pc_data ack-like command: {cmd}")
            return

        if topic == self.config.cmd_topic():
            self._log("gateway command topic")
        elif topic.startswith(self.config.cmd_topic() + "/"):
            self._log("port/device command topic")
        else:
            self._log(f"ignore command on unexpected topic: {topic}")
            return

        handled = self.dispatcher.dispatch(self, topic, data)

        if not handled:
            seq = self._safe_seq(data)
            port_id = self.port_id_from_topic(topic) or self.default_port_id()
            self.publish_ack(
                cmd="unknown_command",
                seq=seq,
                ok=False,
                reason="unknown_command",
                slave_id=None,
                cmd_id=cmd_id,
                port_id=port_id,
            )

    def telemetry_loop(self):
        while self.running:
            time.sleep(self.config.telemetry_interval_sec)

            if not self.running:
                break

            for port_id in self.state.list_ports():
                devices = self.state.list_devices(port_id)
                if not devices:
                    continue
                values_by_slave = self._telemetry_values(port_id, devices)
                self.publish(
                    protocol.telemetry_pack(
                        devices,
                        context=self.config,
                        port_id=port_id,
                        values_by_slave=values_by_slave,
                    ),
                    port_id=port_id,
                )
                self._publish_threshold_alarm_events(port_id, devices, values_by_slave)

    def heartbeat_loop(self):
        while self.running:
            time.sleep(self.config.heartbeat_interval_sec)

            if not self.running:
                break

            self.publish_heartbeat()

    def publish_heartbeat(self):
        self.publish(protocol.gateway_heartbeat(self.config))

    def publish(self, payload, port_id=None):
        self.bus.publish(payload, port_id=port_id)

    def publish_ack(
        self,
        cmd: str,
        seq: int,
        ok: bool,
        reason: str,
        slave_id,
        device_type: str = "sensor_th",
        cmd_id: str = "",
        port_id: str = None,
    ):
        self.publish(
            protocol.ack(
                cmd=cmd,
                seq=seq,
                ok=ok,
                reason=reason,
                slave_id=slave_id,
                device_type=device_type,
                cmd_id=cmd_id,
                context=self.config,
                port_id=port_id or self.config.default_port_id,
            ),
            port_id=port_id or self.config.default_port_id,
        )

    def publish_snapshot(self, reason: str, seq: int = 0):
        self.publish(
            protocol.config_snapshot(
                reason=reason,
                seq=seq,
                context=self.config,
                ports=self.state.build_snapshot_ports(),
            )
        )

    def stop(self):
        self.running = False
        self.bus.disconnect()

    def default_port_id(self):
        return self.config.default_port_id

    def port_id_from_topic(self, topic: str):
        prefix = self.config.cmd_topic() + "/"
        if not topic.startswith(prefix):
            return None
        suffix = topic[len(prefix):]
        if not suffix or "/" in suffix:
            return None
        return suffix

    def _safe_seq(self, data):
        try:
            return int(data.get("seq") or data.get("sequence") or 0)
        except Exception:
            return 0

    def _telemetry_values(self, port_id, devices):
        values_by_slave = {}

        for device in devices:
            device_type = device.get("device_type") or device.get("deviceType") or "sensor_th"
            if device_type == "relay":
                continue

            slave_id = self._device_slave_id(device)
            temperature = self._scenario_temperature(port_id, device)
            values_by_slave[slave_id] = {
                "temperature": temperature,
                "humidity": protocol.jitter_value(60.0, 0.8),
            }

        return values_by_slave

    def _publish_threshold_alarm_events(self, port_id, devices, values_by_slave):
        for device in devices:
            device_type = device.get("device_type") or device.get("deviceType") or "sensor_th"
            if device_type == "relay":
                continue

            slave_id = self._device_slave_id(device)
            values = values_by_slave.get(slave_id, {})
            if "temperature" not in values:
                continue

            thresholds = self._temperature_alarm_thresholds(device)
            if thresholds is None:
                self._clear_temperature_alarm_states(port_id, slave_id)
                continue

            value = float(values["temperature"])
            self._publish_temperature_alarm_event(
                port_id,
                device,
                slave_id,
                value,
                thresholds.get("high"),
                "threshold_high",
                value > thresholds["high"] if thresholds.get("high") is not None else False,
                suppress_recovery=slave_id == 10,
            )
            self._publish_temperature_alarm_event(
                port_id,
                device,
                slave_id,
                value,
                thresholds.get("low"),
                "threshold_low",
                value < thresholds["low"] if thresholds.get("low") is not None else False,
                suppress_recovery=slave_id == 12,
            )

    def _publish_temperature_alarm_event(
        self,
        port_id,
        device,
        slave_id,
        value,
        threshold,
        alarm_type,
        active,
        suppress_recovery=False,
    ):
        key = (self.config.gatewayId, port_id, slave_id, "temperature", alarm_type)
        if threshold is None:
            self.alarm_states.pop(key, None)
            return

        was_active = self.alarm_states.get(key) == "active"
        if active and not was_active:
            event = protocol.alarm_event(
                device,
                state="active",
                value=value,
                threshold=threshold,
                context=self.config,
                port_id=port_id,
                alarm_type=alarm_type,
            )
            self._log_alarm_event(port_id, slave_id, event, value, threshold, "active")
            self.publish(event, port_id=port_id)
            self.alarm_states[key] = "active"
        elif not active and was_active:
            if suppress_recovery:
                return
            event = protocol.alarm_event(
                device,
                state="recovered",
                value=value,
                threshold=threshold,
                context=self.config,
                port_id=port_id,
                alarm_type=alarm_type,
            )
            self._log_alarm_event(port_id, slave_id, event, value, threshold, "recovered")
            self.publish(event, port_id=port_id)
            self.alarm_states[key] = "recovered"

    def _log_alarm_event(self, port_id, slave_id, event, value, threshold, state):
        message = (
            f"[ALARM_EVENT] {state} gateway={self.config.gatewayId} port={port_id} "
            f"slave_id={slave_id} value={value} threshold={threshold} "
            f"alarm_id={event.get('alarm_id')}"
        )
        if slave_id in {9, 10, 11, 12, 13}:
            self._log_scenario_once(port_id, slave_id, f"{event.get('alarm_type')}_{state}_sent", message)
        else:
            print(message)

    def _clear_temperature_alarm_states(self, port_id, slave_id):
        for alarm_type in ("threshold_high", "threshold_low"):
            key = (self.config.gatewayId, port_id, slave_id, "temperature", alarm_type)
            self.alarm_states.pop(key, None)

    def _temperature_alarm_thresholds(self, device):
        threshold_config = device.get("threshold_config") or device.get("thresholdConfig")
        if not isinstance(threshold_config, dict):
            return None

        threshold_enabled = bool(threshold_config.get("threshold_enabled", threshold_config.get("thresholdEnabled", True)))
        if not threshold_enabled:
            return None

        thresholds = threshold_config.get("thresholds")
        if not isinstance(thresholds, dict):
            return None

        temperature = thresholds.get("temperature")
        if not isinstance(temperature, dict):
            return None

        if not bool(temperature.get("enable_alarm", temperature.get("enableAlarm", True))):
            return None

        high = self._optional_float(temperature.get("alarm_high", temperature.get("alarmHigh")))
        low = self._optional_float(temperature.get("alarm_low", temperature.get("alarmLow")))
        if high is None and low is None:
            return None
        return {"high": high, "low": low}

    def _optional_float(self, value):
        if value is None:
            return None
        try:
            return float(value)
        except Exception:
            return None

    def _device_slave_id(self, device):
        try:
            return int(device.get("slave_id", device.get("device_id", device.get("deviceId", 0))) or 0)
        except Exception:
            return 0

    def _scenario_temperature(self, port_id, device):
        slave_id = self._device_slave_id(device)
        thresholds = self._temperature_alarm_thresholds(device)
        elapsed = self._device_elapsed(device)

        if thresholds is None:
            return protocol.jitter_value(25.0, 0.3)

        high = thresholds.get("high")
        low = thresholds.get("low")
        normal = self._temperature_normal_value(low, high)

        if slave_id == 9 and high is not None:
            if elapsed < 10.0:
                return protocol.jitter_value(normal, 0.2)
            if elapsed < 20.0:
                self._log_scenario_once(
                    port_id,
                    slave_id,
                    "enter_high",
                    f"[TEMP_SCENARIO] enter_high gateway={self.config.gatewayId} "
                    f"port={port_id} slave_id={slave_id} threshold={high}",
                )
                return protocol.jitter_value(high + 2.0, 0.2)
            self._log_scenario_once(
                port_id,
                slave_id,
                "recover_normal",
                f"[TEMP_SCENARIO] recover_normal gateway={self.config.gatewayId} "
                f"port={port_id} slave_id={slave_id} threshold={high}",
            )
            return protocol.jitter_value(normal, 0.2)

        if slave_id == 10 and high is not None:
            if elapsed < 10.0:
                return protocol.jitter_value(normal, 0.2)
            self._log_scenario_once(
                port_id,
                slave_id,
                "enter_high",
                f"[TEMP_SCENARIO] enter_high gateway={self.config.gatewayId} "
                f"port={port_id} slave_id={slave_id} threshold={high}",
            )
            return protocol.jitter_value(high + 2.0, 0.2)

        if slave_id == 11 and low is not None:
            if elapsed < 10.0:
                return protocol.jitter_value(normal, 0.2)
            if elapsed < 20.0:
                self._log_scenario_once(
                    port_id,
                    slave_id,
                    "enter_low",
                    f"[TEMP_SCENARIO] enter_low gateway={self.config.gatewayId} "
                    f"port={port_id} slave_id={slave_id} threshold={low}",
                )
                return protocol.jitter_value(low - 2.0, 0.2)
            self._log_scenario_once(
                port_id,
                slave_id,
                "recover_normal",
                f"[TEMP_SCENARIO] recover_normal gateway={self.config.gatewayId} "
                f"port={port_id} slave_id={slave_id} threshold={low}",
            )
            return protocol.jitter_value(normal, 0.2)

        if slave_id == 12 and low is not None:
            if elapsed < 10.0:
                return protocol.jitter_value(normal, 0.2)
            self._log_scenario_once(
                port_id,
                slave_id,
                "enter_low",
                f"[TEMP_SCENARIO] enter_low gateway={self.config.gatewayId} "
                f"port={port_id} slave_id={slave_id} threshold={low}",
            )
            return protocol.jitter_value(low - 2.0, 0.2)

        if slave_id == 13 and high is not None and low is not None:
            if elapsed < 10.0:
                return protocol.jitter_value(normal, 0.2)
            if elapsed < 20.0:
                self._log_scenario_once(
                    port_id,
                    slave_id,
                    "enter_high",
                    f"[TEMP_SCENARIO] enter_high gateway={self.config.gatewayId} "
                    f"port={port_id} slave_id={slave_id} threshold={high}",
                )
                return protocol.jitter_value(high + 2.0, 0.2)
            if elapsed < 30.0:
                return protocol.jitter_value(normal, 0.2)
            if elapsed < 40.0:
                self._log_scenario_once(
                    port_id,
                    slave_id,
                    "enter_low",
                    f"[TEMP_SCENARIO] enter_low gateway={self.config.gatewayId} "
                    f"port={port_id} slave_id={slave_id} threshold={low}",
                )
                return protocol.jitter_value(low - 2.0, 0.2)
            self._log_scenario_once(
                port_id,
                slave_id,
                "recover_normal",
                f"[TEMP_SCENARIO] recover_normal gateway={self.config.gatewayId} "
                f"port={port_id} slave_id={slave_id} thresholds={low}/{high}",
            )
            return protocol.jitter_value(normal, 0.2)

        return protocol.jitter_value(25.0, 0.3)

    def _temperature_normal_value(self, low, high):
        if low is not None and high is not None and low < high:
            return (low + high) / 2.0
        if high is not None:
            return high - 2.0
        if low is not None:
            return low + 2.0
        return 25.0

    def _device_elapsed(self, device):
        try:
            added_at = float(device.get("added_at") or self.started_at or time.time())
        except Exception:
            added_at = self.started_at or time.time()
        return max(0.0, time.time() - added_at)

    def _log_scenario_once(self, port_id, slave_id, event, message):
        key = (port_id, slave_id, event)
        if self.spike_states.get(key):
            return
        self.spike_states[key] = True
        print(message)

    def _log(self, message: str):
        print(f"[GW {self.config.gatewayId}] {message}")
