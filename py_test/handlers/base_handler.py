# handlers/base_handler.py
# -*- coding: utf-8 -*-

from typing import Dict, Any, Optional


class BaseHandler:
    name = "base"

    def can_handle(self, topic: str, data: Dict[str, Any]) -> bool:
        return False

    def handle(self, gateway, topic: str, data: Dict[str, Any]) -> bool:
        return False

    def get_type(self, data: Dict[str, Any]) -> str:
        return data.get("type") or ""

    def get_cmd(self, data: Dict[str, Any]) -> str:
        return (
            data.get("cmd")
            or data.get("command")
            or data.get("commandType")
            or data.get("action")
            or ""
        )

    def get_seq(self, data: Dict[str, Any]) -> int:
        try:
            return int(data.get("seq") or data.get("sequence") or data.get("cmd_id") or 0)
        except Exception:
            return 0

    def get_slave_id(self, data: Dict[str, Any]) -> Optional[int]:
        device = data.get("device")

        if isinstance(device, dict):
            for key in ["slave_id", "slaveAddr", "slaveAddress", "deviceId", "device_id", "slave_addr"]:
                if key in device:
                    try:
                        return int(device[key])
                    except Exception:
                        pass

        target = data.get("target")

        if isinstance(target, dict):
            for key in ["slave_id", "slaveAddr", "slaveAddress", "deviceId", "device_id", "slave_addr"]:
                if key in target:
                    try:
                        return int(target[key])
                    except Exception:
                        pass

        for key in ["slave_id", "slaveAddr", "slaveAddress", "deviceId", "device_id", "slave_addr"]:
            if key in data:
                try:
                    return int(data[key])
                except Exception:
                    pass

        return None

    def get_device_type(self, data: Dict[str, Any]) -> str:
        device = data.get("device")

        if isinstance(device, dict):
            return (
                device.get("device_type")
                or device.get("deviceType")
                or device.get("type")
                or "sensor_th"
            )

        return data.get("device_type") or data.get("deviceType") or "sensor_th"

    def get_poll_interval_ms(self, data: Dict[str, Any]) -> int:
        device = data.get("device")

        if isinstance(device, dict):
            try:
                return int(device.get("poll_interval_ms") or device.get("pollIntervalMs") or 1000)
            except Exception:
                return 1000

        try:
            return int(data.get("poll_interval_ms") or data.get("pollIntervalMs") or 1000)
        except Exception:
            return 1000

    def get_threshold_config(self, data: Dict[str, Any]) -> Dict[str, Any]:
        device = data.get("device")

        if isinstance(device, dict):
            cfg = device.get("threshold_config") or device.get("thresholdConfig")
            if isinstance(cfg, dict):
                return self.normalize_threshold_config(cfg)

        cfg = data.get("threshold_config") or data.get("thresholdConfig")
        if isinstance(cfg, dict):
            return self.normalize_threshold_config(cfg)

        return self.default_threshold_config()

    def normalize_threshold_config(self, cfg: Dict[str, Any]) -> Dict[str, Any]:
        """
        兼容 Pc_ui 发来的简单格式：
        {
            "enable": true,
            "temp_high": 35,
            "humi_high": 80
        }

        转成 Pc_data config_snapshot 当前更容易解析的格式：
        {
            "thresholds": {
                "temperature": {"enable_alarm": true, "alarm_high": 35},
                "humidity": {"enable_alarm": true, "alarm_high": 80}
            }
        }
        """
        if "thresholds" in cfg and isinstance(cfg["thresholds"], dict):
            return cfg

        enable = bool(cfg.get("enable", cfg.get("enable_alarm", cfg.get("enableAlarm", True))))
        temp_high = float(cfg.get("temp_high", cfg.get("tempHigh", cfg.get("alarm_high", 35.0))))
        humi_high = float(cfg.get("humi_high", cfg.get("humiHigh", 80.0)))

        return {
            "thresholds": {
                "temperature": {
                    "enable_alarm": enable,
                    "enableAlarm": enable,
                    "alarm_low": 0.0,
                    "alarmLow": 0.0,
                    "alarm_high": temp_high,
                    "alarmHigh": temp_high,
                },
                "humidity": {
                    "enable_alarm": enable,
                    "enableAlarm": enable,
                    "alarm_low": 0.0,
                    "alarmLow": 0.0,
                    "alarm_high": humi_high,
                    "alarmHigh": humi_high,
                },
            }
        }

    def default_threshold_config(self) -> Dict[str, Any]:
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