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
                self.publish(protocol.telemetry_pack(devices, context=self.config, port_id=port_id), port_id=port_id)

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

    def _log(self, message: str):
        print(f"[GW {self.config.gatewayId}] {message}")
