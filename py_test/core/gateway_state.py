# core/gateway_state.py
# -*- coding: utf-8 -*-

import time
from typing import Any, Dict, List, Optional

from core.gateway_config import PortConfig, legacy_default_config


def mock_device_name(slave_id: int) -> str:
    return f"Device {slave_id}"


class GatewayState:
    def __init__(self, ports: Optional[List[PortConfig]] = None, default_port_id: Optional[str] = None):
        self.default_port_id = default_port_id or (ports[0].portId if ports else "port_001")
        self.port_configs: Dict[str, PortConfig] = {}
        self.ports: Dict[str, Dict[str, Any]] = {}
        if ports:
            for port in ports:
                self.add_port(port)

    def add_port(self, port: PortConfig):
        self.port_configs[port.portId] = port
        self.ports.setdefault(port.portId, {"devices": {}})

    def init_default(self):
        """
        Mock 启动时默认带一个从站 1。
        这样 Pc_data 收到 gateway_register + config_snapshot 后，
        Pc_ui 能看到网关和初始设备。
        """
        legacy = legacy_default_config()
        self.default_port_id = legacy.default_port_id
        for port in legacy.ports:
            self.add_port(port)
            for dev in port.devices:
                self.add_device(
                    port.portId,
                    dev.slave_id,
                    device_type=dev.device_type,
                    poll_interval_ms=dev.poll_interval_ms,
                    threshold_config=dev.threshold_config,
                    threshold_config_source="scenario" if dev.threshold_config is not None else "",
                    device_options=dev.device_options,
                )

    def init_from_config(self, ports: List[PortConfig]):
        for port in ports:
            self.add_port(port)
            for dev in port.devices:
                self.add_device(
                    port.portId,
                    dev.slave_id,
                    device_type=dev.device_type,
                    poll_interval_ms=dev.poll_interval_ms,
                    threshold_config=dev.threshold_config,
                    threshold_config_source="scenario" if dev.threshold_config is not None else "",
                    device_options=dev.device_options,
                )

    def clear(self):
        for port_state in self.ports.values():
            port_state["devices"].clear()

    def exists(self, *args, port_id: Optional[str] = None, slave_id: Optional[int] = None) -> bool:
        port_id, slave_id = self._port_slave(args, port_id, slave_id)
        return slave_id in self._devices(port_id)

    def add_device(
        self,
        *args,
        slave_id: Optional[int] = None,
        device_type: str = "sensor_th",
        poll_interval_ms: int = 1000,
        threshold_config: Optional[Dict[str, Any]] = None,
        threshold_config_source: str = "",
        device_options: Optional[Dict[str, Any]] = None,
        port_id: Optional[str] = None,
    ):
        port_id, slave_id = self._port_slave(args, port_id, slave_id)
        if slave_id is None:
            return
        if device_options is None:
            device_options = {}

        relay_channel_count = 8
        if device_type == "relay":
            try:
                requested_count = int(device_options.get("relay_channel_count", relay_channel_count))
                if 1 <= requested_count <= 64:
                    relay_channel_count = requested_count
            except Exception:
                relay_channel_count = 8

        device = {
            "deviceId": slave_id,
            "device_id": slave_id,
            "slave_id": slave_id,
            "slaveAddr": slave_id,
            "slaveAddress": slave_id,
            "deviceType": device_type,
            "device_type": device_type,
            "deviceName": mock_device_name(slave_id),
            "device_name": mock_device_name(slave_id),
            "pollIntervalMs": poll_interval_ms,
            "poll_interval_ms": poll_interval_ms,
            "enabled": True,
            "status": "online",
            "comm_status": "online",
            "data_status": "normal",
            "threshold_enabled": bool(threshold_config),
            "thresholdEnabled": bool(threshold_config),
            "threshold_config": threshold_config,
            "thresholdConfig": threshold_config,
            "threshold_config_source": threshold_config_source if threshold_config is not None else "",
            "thresholdConfigSource": threshold_config_source if threshold_config is not None else "",
            "added_at": time.time(),
            "alarm_scenario_state": {},
        }
        if device_type == "relay":
            relay_states = [False] * relay_channel_count
            device["relay_states"] = relay_states
            device["relayStates"] = list(relay_states)
            device["channelCount"] = relay_channel_count
            device["channel_count"] = relay_channel_count

        self._devices(port_id)[int(slave_id)] = device

    def remove_device(self, *args, port_id: Optional[str] = None, slave_id: Optional[int] = None):
        port_id, slave_id = self._port_slave(args, port_id, slave_id)
        self._devices(port_id).pop(slave_id, None)

    def get_device(self, *args, port_id: Optional[str] = None, slave_id: Optional[int] = None) -> Optional[Dict[str, Any]]:
        port_id, slave_id = self._port_slave(args, port_id, slave_id)
        return self._devices(port_id).get(slave_id)

    def set_relay_states(self, *args, states: Optional[List[bool]] = None, port_id: Optional[str] = None, slave_id: Optional[int] = None) -> bool:
        parsed_args = list(args)
        if states is None and parsed_args:
            states = parsed_args.pop()
        port_id, slave_id = self._port_slave(tuple(parsed_args), port_id, slave_id)
        device = self._devices(port_id).get(slave_id)
        if not device or device.get("device_type") != "relay":
            return False

        relay_states = list(states or [])
        device["relay_states"] = relay_states
        device["relayStates"] = list(relay_states)
        device["channelCount"] = len(relay_states)
        device["channel_count"] = len(relay_states)
        return True

    def list_devices(self, port_id: Optional[str] = None) -> List[Dict[str, Any]]:
        return list(self._devices(port_id or self.default_port_id).values())

    def list_ports(self) -> List[str]:
        return list(self.ports.keys())

    def device_list(self, port_id: Optional[str] = None) -> List[Dict[str, Any]]:
        return self.list_devices(port_id or self.default_port_id)

    def build_snapshot_ports(self) -> List[Dict[str, Any]]:
        snapshot_ports = []
        for port_id in self.list_ports():
            port = self.port_configs.get(port_id) or PortConfig(portId=port_id)
            snapshot_ports.append(
                {
                    "portId": port.portId,
                    "portName": port.portName,
                    "slot": port.slot,
                    "port": port.port,
                    "path": port.path,
                    "baud": port.baud,
                    "connected": True,
                    "status": "connected",
                    "devices": self.list_devices(port_id),
                }
            )
        return snapshot_ports

    def _devices(self, port_id: Optional[str]) -> Dict[int, Dict[str, Any]]:
        actual_port_id = port_id or self.default_port_id
        self.ports.setdefault(actual_port_id, {"devices": {}})
        return self.ports[actual_port_id]["devices"]

    def _port_slave(self, args, port_id: Optional[str], slave_id: Optional[int]):
        if len(args) == 1:
            slave_id = args[0]
        elif len(args) >= 2:
            port_id = args[0]
            slave_id = args[1]
        if port_id is None:
            port_id = self.default_port_id
        if slave_id is not None:
            try:
                slave_id = int(slave_id)
            except Exception:
                slave_id = None
        return port_id, slave_id
