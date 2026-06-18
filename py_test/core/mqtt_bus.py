# core/mqtt_bus.py
# -*- coding: utf-8 -*-

import json
import time
from typing import Callable, Dict, Any, Optional

import paho.mqtt.client as mqtt

import config


class MqttBus:
    def __init__(
        self,
        on_message: Callable[[str, Dict[str, Any]], None],
        on_connected: Optional[Callable[[], None]] = None,
    ):
        self.on_message_callback = on_message
        self.on_connected_callback = on_connected

        client_id = f"mock_linux_data_{config.GATEWAY_ID}_{int(time.time())}"

        self.client = mqtt.Client(
            client_id=client_id,
            clean_session=True,
        )

        if config.MQTT_USERNAME:
            self.client.username_pw_set(
                config.MQTT_USERNAME,
                config.MQTT_PASSWORD or "",
            )

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def connect_forever(self):
        print("========== Mock Linux_data ==========")
        print(f"broker              = {config.MQTT_HOST}:{config.MQTT_PORT}")
        print(f"register_topic      = {config.REGISTER_TOPIC}")
        print(f"publish up_topic    = {config.UP_TOPIC}")
        print(f"publish port_up     = {config.PORT_UP_TOPIC}")
        print(f"subscribe cmd_topic = {config.CMD_TOPIC}")
        print(f"subscribe cmd_wild  = {config.CMD_WILDCARD_TOPIC}")
        print("=====================================")

        self.client.connect(config.MQTT_HOST, config.MQTT_PORT, keepalive=30)
        self.client.loop_forever()

    def _on_connect(self, client, userdata, flags, rc):
        if rc != 0:
            print(f"[MQTT][CONNECT FAILED] rc={rc}")
            return

        print("[MQTT][CONNECTED]")

        client.subscribe(config.CMD_TOPIC, qos=0)
        client.subscribe(config.CMD_WILDCARD_TOPIC, qos=0)

        print(f"[SUB] {config.CMD_TOPIC}")
        print(f"[SUB] {config.CMD_WILDCARD_TOPIC}")

        if self.on_connected_callback:
            self.on_connected_callback()

    def _on_disconnect(self, client, userdata, rc):
        print(f"[MQTT][DISCONNECT] rc={rc}")

    def _on_message(self, client, userdata, msg):
        try:
            text = msg.payload.decode("utf-8", errors="replace").strip()
            data = json.loads(text)
        except Exception as e:
            print(f"[MQTT][BAD JSON] {e}")
            print(msg.payload)
            return

        print("\n[MQTT][RECV]")
        print(f"topic = {msg.topic}")
        print(json.dumps(data, ensure_ascii=False, indent=2))

        self.on_message_callback(msg.topic, data)

    def publish(self, payload: Dict[str, Any]):
        text = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
        msg_type = payload.get("type") or ""
        if msg_type in {"gateway_register", "port_register", "port_status", "device_config_snapshot", "config_snapshot"}:
            topic = config.REGISTER_TOPIC
        elif msg_type in {"gateway_heartbeat", "gateway_status"}:
            topic = config.UP_TOPIC
        else:
            topic = config.PORT_UP_TOPIC

        print("\n[MQTT][PUB]")
        print(f"topic = {topic}")
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        if msg_type == "gateway_register":
            print(f"[REGISTER] publish {config.REGISTER_TOPIC}")
        elif msg_type == "ack":
            print(f"[PUB ACK] {payload.get('cmd') or payload.get('command')} {payload.get('status')}")
        elif msg_type == "config_snapshot":
            print(f"[PUB SNAPSHOT] reason={payload.get('reason', '')}")
        elif msg_type == "telemetry_pack":
            print(
                f"[PUB TELEMETRY] gatewayId={config.GATEWAY_ID} "
                f"deviceCount={len(payload.get('devices', []))} seq={payload.get('seq')}"
            )

        self.client.publish(
            topic,
            text,
            qos=0,
            retain=False,
        )

    def disconnect(self):
        self.client.disconnect()
