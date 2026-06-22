# core/gateway_config.py
# -*- coding: utf-8 -*-

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

import config


def default_threshold_config() -> Dict[str, Any]:
    return {
        "thresholds": {
            "temperature": {
                "enable_alarm": True,
                "enableAlarm": True,
                "alarm_low": 0.0,
                "alarmLow": 0.0,
                "alarm_high": 35.0,
                "alarmHigh": 35.0,
            },
            "humidity": {
                "enable_alarm": True,
                "enableAlarm": True,
                "alarm_low": 0.0,
                "alarmLow": 0.0,
                "alarm_high": 80.0,
                "alarmHigh": 80.0,
            },
        }
    }


@dataclass
class MqttConfig:
    host: str = config.MQTT_HOST
    port: int = config.MQTT_PORT
    username: Optional[str] = config.MQTT_USERNAME
    password: Optional[str] = config.MQTT_PASSWORD


@dataclass
class DeviceConfig:
    slave_id: int
    device_type: str = "sensor_th"
    poll_interval_ms: int = 1000
    threshold_config: Dict[str, Any] = field(default_factory=default_threshold_config)
    device_options: Dict[str, Any] = field(default_factory=dict)


@dataclass
class PortConfig:
    portId: str = config.PORT_ID
    portName: str = config.PORT_NAME
    slot: int = config.PORT_SLOT
    port: str = config.PORT_PATH
    path: str = config.PORT_PATH
    baud: int = config.PORT_BAUD
    devices: List[DeviceConfig] = field(default_factory=list)


@dataclass
class GatewayConfig:
    gatewayId: str = config.GATEWAY_ID
    gatewayName: str = config.GATEWAY_NAME
    factoryId: str = config.FACTORY_ID
    factoryName: str = config.FACTORY_NAME
    areaId: str = config.AREA_ID
    areaName: str = config.AREA_NAME
    ports: List[PortConfig] = field(default_factory=list)
    telemetry_interval_sec: float = config.TELEMETRY_INTERVAL_SEC
    heartbeat_interval_sec: float = config.HEARTBEAT_INTERVAL_SEC
    mqtt: MqttConfig = field(default_factory=MqttConfig)
    behavior: Dict[str, Any] = field(default_factory=dict)

    @property
    def default_port_id(self) -> str:
        return self.ports[0].portId if self.ports else config.PORT_ID

    def port_config(self, port_id: Optional[str] = None) -> PortConfig:
        wanted = port_id or self.default_port_id
        for port in self.ports:
            if port.portId == wanted:
                return port
        return PortConfig(portId=wanted)

    def register_topic(self) -> str:
        return "gateway/register"

    def up_topic(self) -> str:
        return f"gateway/{self.gatewayId}/up"

    def port_up_topic(self, port_id: Optional[str] = None) -> str:
        return f"gateway/{self.gatewayId}/{port_id or self.default_port_id}/up"

    def cmd_topic(self) -> str:
        return f"cmd/{self.gatewayId}"

    def cmd_wildcard_topic(self) -> str:
        return f"cmd/{self.gatewayId}/#"

    def port_cmd_topic(self, port_id: Optional[str] = None) -> str:
        return f"cmd/{self.gatewayId}/{port_id or self.default_port_id}"


def legacy_default_config() -> GatewayConfig:
    return GatewayConfig(
        ports=[
            PortConfig(
                devices=[
                    DeviceConfig(
                        slave_id=1,
                        device_type="sensor_th",
                        poll_interval_ms=1000,
                        threshold_config=default_threshold_config(),
                    )
                ]
            )
        ],
        behavior={"profile": "legacy"},
    )


def _as_int(value: Any, default: int) -> int:
    try:
        return int(value)
    except Exception:
        return default


def _as_float(value: Any, default: float) -> float:
    try:
        return float(value)
    except Exception:
        return default


def _load_device(raw: Dict[str, Any]) -> DeviceConfig:
    device_options = raw.get("device_options")
    if not isinstance(device_options, dict):
        device_options = {}
    channel_count = raw.get("channel_count", raw.get("channelCount"))
    if channel_count is not None and "relay_channel_count" not in device_options:
        device_options = dict(device_options)
        device_options["relay_channel_count"] = _as_int(channel_count, 8)
    threshold_config = raw.get("threshold_config") or raw.get("thresholdConfig")
    if not isinstance(threshold_config, dict):
        threshold_config = default_threshold_config()
    return DeviceConfig(
        slave_id=_as_int(raw.get("slave_id", raw.get("slaveId", raw.get("deviceId", 0))), 0),
        device_type=str(raw.get("device_type") or raw.get("deviceType") or "sensor_th"),
        poll_interval_ms=_as_int(raw.get("poll_interval_ms", raw.get("pollIntervalMs", 1000)), 1000),
        threshold_config=threshold_config,
        device_options=device_options,
    )


def _load_port(raw: Dict[str, Any]) -> PortConfig:
    port_path = str(raw.get("port") or raw.get("path") or config.PORT_PATH)
    devices_raw = raw.get("devices") if isinstance(raw.get("devices"), list) else []
    return PortConfig(
        portId=str(raw.get("portId") or raw.get("port_id") or config.PORT_ID),
        portName=str(raw.get("portName") or raw.get("port_name") or config.PORT_NAME),
        slot=_as_int(raw.get("slot", config.PORT_SLOT), config.PORT_SLOT),
        port=port_path,
        path=str(raw.get("path") or port_path),
        baud=_as_int(raw.get("baud", config.PORT_BAUD), config.PORT_BAUD),
        devices=[_load_device(item) for item in devices_raw if isinstance(item, dict)],
    )


def _load_gateway(raw: Dict[str, Any], mqtt_config: MqttConfig) -> GatewayConfig:
    ports_raw = raw.get("ports") if isinstance(raw.get("ports"), list) else []
    ports = [_load_port(item) for item in ports_raw if isinstance(item, dict)]
    if not ports:
        ports = legacy_default_config().ports
    return GatewayConfig(
        gatewayId=str(raw.get("gatewayId") or raw.get("gateway_id") or config.GATEWAY_ID),
        gatewayName=str(raw.get("gatewayName") or raw.get("gateway_name") or config.GATEWAY_NAME),
        factoryId=str(raw.get("factoryId") or raw.get("factory_id") or config.FACTORY_ID),
        factoryName=str(raw.get("factoryName") or raw.get("factory_name") or config.FACTORY_NAME),
        areaId=str(raw.get("areaId") or raw.get("area_id") or config.AREA_ID),
        areaName=str(raw.get("areaName") or raw.get("area_name") or config.AREA_NAME),
        ports=ports,
        telemetry_interval_sec=_as_float(
            raw.get("telemetry_interval_sec", raw.get("telemetryIntervalSec", config.TELEMETRY_INTERVAL_SEC)),
            config.TELEMETRY_INTERVAL_SEC,
        ),
        heartbeat_interval_sec=_as_float(
            raw.get("heartbeat_interval_sec", raw.get("heartbeatIntervalSec", config.HEARTBEAT_INTERVAL_SEC)),
            config.HEARTBEAT_INTERVAL_SEC,
        ),
        mqtt=mqtt_config,
        behavior=raw.get("behavior") if isinstance(raw.get("behavior"), dict) else {},
    )


def resolve_scenario_path(path: str) -> Path:
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    cwd_candidate = Path.cwd() / candidate
    if cwd_candidate.exists():
        return cwd_candidate
    py_test_dir = Path(__file__).resolve().parents[1]
    return py_test_dir / candidate


def load_scenario(path: str) -> List[GatewayConfig]:
    scenario_path = resolve_scenario_path(path)
    with scenario_path.open("r", encoding="utf-8") as fh:
        root = json.load(fh)
    if not isinstance(root, dict):
        raise ValueError("scenario root must be an object")

    mqtt_raw = root.get("mqtt") if isinstance(root.get("mqtt"), dict) else {}
    mqtt_config = MqttConfig(
        host=str(mqtt_raw.get("host") or config.MQTT_HOST),
        port=_as_int(mqtt_raw.get("port", config.MQTT_PORT), config.MQTT_PORT),
        username=mqtt_raw.get("username", config.MQTT_USERNAME),
        password=mqtt_raw.get("password", config.MQTT_PASSWORD),
    )

    gateways_raw = root.get("gateways")
    if not isinstance(gateways_raw, list) or not gateways_raw:
        raise ValueError("scenario must contain a non-empty gateways array")
    return [_load_gateway(item, mqtt_config) for item in gateways_raw if isinstance(item, dict)]
