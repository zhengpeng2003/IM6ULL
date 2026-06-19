# core/gateway_state.py
# -*- coding: utf-8 -*-

from typing import Dict, Any, List, Optional


class GatewayState:
    def __init__(self):
        self.devices: Dict[int, Dict[str, Any]] = {}

    def init_default(self):
        """
        Mock 启动时默认带一个从站 1。
        这样 Pc_data 收到 gateway_register + config_snapshot 后，
        Pc_ui 能看到网关和初始设备。
        """
        self.add_device(
            slave_id=1,
            device_type="sensor_th",
            poll_interval_ms=1000,
            threshold_config={
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
            },
        )

    def clear(self):
        self.devices.clear()

    def exists(self, slave_id: int) -> bool:
        return slave_id in self.devices

    def add_device(
        self,
        slave_id: int,
        device_type: str = "sensor_th",
        poll_interval_ms: int = 1000,
        threshold_config: Optional[Dict[str, Any]] = None,
    ):
        if threshold_config is None:
            threshold_config = {
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

        self.devices[slave_id] = {
            "deviceId": slave_id,
            "device_id": slave_id,
            "slave_id": slave_id,
            "slaveAddr": slave_id,
            "slaveAddress": slave_id,

            "deviceType": device_type,
            "device_type": device_type,

            "deviceName": f"继电器从站{slave_id}" if device_type == "relay" else f"温湿度从站{slave_id}",
            "device_name": f"继电器从站{slave_id}" if device_type == "relay" else f"温湿度从站{slave_id}",

            "pollIntervalMs": poll_interval_ms,
            "poll_interval_ms": poll_interval_ms,

            "enabled": True,
            "status": "online",
            "comm_status": "online",
            "data_status": "normal",

            "threshold_enabled": True,
            "thresholdEnabled": True,

            "threshold_config": threshold_config,
            "thresholdConfig": threshold_config,
        }

    def remove_device(self, slave_id: int):
        self.devices.pop(slave_id, None)

    def get_device(self, slave_id: int) -> Optional[Dict[str, Any]]:
        return self.devices.get(slave_id)

    def set_relay_states(self, slave_id: int, states: List[bool]) -> bool:
        device = self.devices.get(slave_id)
        if not device or device.get("device_type") != "relay":
            return False

        device["relay_states"] = list(states)
        device["relayStates"] = list(states)
        return True

    def device_list(self) -> List[Dict[str, Any]]:
        return list(self.devices.values())
