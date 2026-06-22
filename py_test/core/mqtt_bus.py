# core/mqtt_bus.py
# -*- coding: utf-8 -*-

import json
import time
from typing import Any, Callable, Dict, Optional

import paho.mqtt.client as mqtt

from core.gateway_config import GatewayConfig


class MqttBus:
    def __init__(
        self,
        gateway_config: GatewayConfig,
        on_message: Callable[[str, Dict[str, Any]], None],
        on_connected: Optional[Callable[[], None]] = None,
        client_suffix: str = "",
    ):
        self.gateway_config = gateway_config
        self.on_message_callback = on_message
        self.on_connected_callback = on_connected

        suffix = client_suffix or str(int(time.time() * 1000))
        client_id = f"python_mock_{gateway_config.gatewayId}_{suffix}"

        self.client = mqtt.Client(
            client_id=client_id,
            clean_session=True,
        )

        if gateway_config.mqtt.username:
            self.client.username_pw_set(
                gateway_config.mqtt.username,
                gateway_config.mqtt.password or "",
            )

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def connect(self):
        cfg = self.gateway_config
        print("========== Mock Linux_data ==========")
        print(f"gateway             = {cfg.gatewayId}")
        print(f"broker              = {cfg.mqtt.host}:{cfg.mqtt.port}")
        print(f"register_topic      = {cfg.register_topic()}")
        print(f"publish up_topic    = {cfg.up_topic()}")
        print(f"publish port_up     = {cfg.port_up_topic()}")
        print(f"subscribe cmd       = {cfg.cmd_topic()}")
        print(f"subscribe cmd_wild  = {cfg.cmd_wildcard_topic()}")
        print("=====================================")

        self.client.connect(cfg.mqtt.host, cfg.mqtt.port, keepalive=30)
        self.client.loop_start()

    def connect_forever(self):
        self.connect()
        try:
            while True:
                time.sleep(0.5)
        finally:
            self.disconnect()

    def _on_connect(self, client, userdata, flags, rc):
        cfg = self.gateway_config
        if rc != 0:
            print(f"[GW {cfg.gatewayId}][MQTT][CONNECT FAILED] rc={rc}")
            return

        print(f"[GW {cfg.gatewayId}][MQTT][CONNECTED]")

        client.subscribe(cfg.cmd_topic(), qos=0)
        client.subscribe(cfg.cmd_wildcard_topic(), qos=0)

        print(f"[GW {cfg.gatewayId}][SUB] {cfg.cmd_topic()}")
        print(f"[GW {cfg.gatewayId}][SUB] {cfg.cmd_wildcard_topic()}")

        if self.on_connected_callback:
            self.on_connected_callback()

    def _on_disconnect(self, client, userdata, rc):
        print(f"[GW {self.gateway_config.gatewayId}][MQTT][DISCONNECT] rc={rc}")

    def _on_message(self, client, userdata, msg):
        try:
            text = msg.payload.decode("utf-8", errors="replace").strip()
            data = json.loads(text)
        except Exception as e:
            print(f"[GW {self.gateway_config.gatewayId}][MQTT][BAD JSON] {e}")
            print(msg.payload)
            return

        print(f"\n[GW {self.gateway_config.gatewayId}][MQTT][RECV]")
        print(f"topic = {msg.topic}")
        print(json.dumps(data, ensure_ascii=False, indent=2))

        self.on_message_callback(msg.topic, data)

    def publish(self, payload: Dict[str, Any], port_id: Optional[str] = None):
        cfg = self.gateway_config
        text = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
        msg_type = payload.get("type") or ""
        if msg_type in {"gateway_register", "port_register", "device_register", "device_config_snapshot", "config_snapshot"}:
            topic = cfg.register_topic()
        elif msg_type in {"gateway_heartbeat", "gateway_status"}:
            topic = cfg.up_topic()
        else:
            topic = cfg.port_up_topic(port_id or payload.get("portId") or payload.get("port_id"))

        print(f"\n[GW {cfg.gatewayId}][MQTT][PUB]")
        print(f"topic = {topic}")
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        if msg_type == "gateway_register":
            print(f"[GW {cfg.gatewayId}] register published")
        elif msg_type == "ack":
            print(f"[GW {cfg.gatewayId}] ack published cmd={payload.get('cmd')} status={payload.get('status')}")
        elif msg_type == "config_snapshot":
            print(f"[GW {cfg.gatewayId}] config_snapshot published reason={payload.get('reason', '')}")
        elif msg_type == "telemetry_pack":
            payload_port_id = payload.get("site", {}).get("portId") or port_id or cfg.default_port_id
            print(
                f"[GW {cfg.gatewayId}][{payload_port_id}] telemetry published "
                f"devices={len(payload.get('devices', []))}"
            )

        self.client.publish(
            topic,
            text,
            qos=0,
            retain=False,
        )

    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()
