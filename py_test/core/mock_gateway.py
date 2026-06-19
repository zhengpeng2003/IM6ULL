# core/mock_gateway.py
# -*- coding: utf-8 -*-

import threading
import time

import config
from core.gateway_state import GatewayState
from core.mqtt_bus import MqttBus
from core.command_dispatcher import CommandDispatcher
from core import protocol
from handlers import create_handlers


class MockGateway:
    def __init__(self):
        self.running = True
        self.started = False

        self.state = GatewayState()
        self.dispatcher = CommandDispatcher(create_handlers())

        self.bus = MqttBus(
            on_message=self.on_mqtt_message,
            on_connected=self.on_mqtt_connected,
        )

    def start(self):
        self.bus.connect_forever()

    def on_mqtt_connected(self):
        if self.started:
            return

        self.started = True

        print("[GATEWAY] mqtt connected")
        print("[GATEWAY] register gateway first")

        self.state.init_default()

        # 关键：先注册网关
        self.publish(protocol.gateway_register())

        # 再发配置快照
        self.publish_snapshot("startup")

        threading.Thread(target=self.telemetry_loop, daemon=True).start()
        threading.Thread(target=self.heartbeat_loop, daemon=True).start()

    def on_mqtt_message(self, topic, data):
        print("[GATEWAY] mqtt message callback")

        if data.get("type") == "ack":
            print("[GATEWAY] ignore ack message")
            return

        cmd = data.get("cmd") or data.get("command") or data.get("commandType") or data.get("action") or ""
        if cmd in {"gateway_register", "config_snapshot", "device_config_snapshot"}:
            print(f"[GATEWAY] ignore pc_data ack-like command: {cmd}")
            return

        if topic == config.CMD_TOPIC:
            print("[GATEWAY] gateway command topic")
        elif topic.startswith(config.CMD_TOPIC + "/"):
            print("[GATEWAY] port/device command topic")
        else:
            print(f"[GATEWAY] ignore command on unexpected topic: {topic}")
            return

        handled = self.dispatcher.dispatch(self, topic, data)

        if not handled:
            seq = self._safe_seq(data)
            cmd_id = str(data.get("cmd_id") or data.get("cmdId") or data.get("commandId") or "")
            self.publish(
                protocol.ack(
                    cmd="unknown_command",
                    seq=seq,
                    ok=False,
                    reason="unknown_command",
                    slave_id=None,
                    cmd_id=cmd_id,
                )
            )

    def telemetry_loop(self):
        while self.running:
            time.sleep(config.TELEMETRY_INTERVAL_SEC)

            if not self.running:
                break

            devices = self.state.device_list()
            if not devices:
                continue

            self.publish(protocol.telemetry_pack(devices))

    def heartbeat_loop(self):
        while self.running:
            time.sleep(config.HEARTBEAT_INTERVAL_SEC)

            if not self.running:
                break

            self.publish(protocol.gateway_heartbeat())

    def publish(self, payload):
        self.bus.publish(payload)

    def publish_snapshot(self, reason: str, seq: int = 0):
        self.publish(
            protocol.config_snapshot(
                devices=self.state.device_list(),
                reason=reason,
                seq=seq,
            )
        )

    def stop(self):
        self.running = False
        self.bus.disconnect()

    def _safe_seq(self, data):
        try:
            return int(data.get("seq") or data.get("sequence") or 0)
        except Exception:
            return 0
