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
            return int(data.get("seq") or data.get("sequence") or 0)
        except Exception:
            return 0

    def get_cmd_id(self, data: Dict[str, Any]) -> str:
        return str(data.get("cmd_id") or data.get("cmdId") or data.get("commandId") or "")

    def get_port_id(self, data: Dict[str, Any], default: str = "port_001") -> str:
        target = data.get("target")
        if isinstance(target, dict):
            value = target.get("portId") or target.get("port_id")
            if value:
                return str(value)

        device = data.get("device")
        if isinstance(device, dict):
            value = device.get("portId") or device.get("port_id")
            if value:
                return str(value)

        value = data.get("portId") or data.get("port_id")
        return str(value) if value else default

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

    def has_threshold_config(self, data: Dict[str, Any]) -> bool:
        if self._raw_threshold_config(data) is not None:
            return True
        if "threshold_enabled" in data or "thresholdEnabled" in data:
            return True
        payload = data.get("payload")
        return isinstance(payload, dict) and ("threshold_enabled" in payload or "thresholdEnabled" in payload)

    def get_threshold_config(self, data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        raw = self._raw_threshold_config(data)
        if raw is None:
            return None
        return self.normalize_threshold_config(raw)

    def _raw_threshold_config(self, data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        device = data.get("device")

        if isinstance(device, dict):
            thresholds = device.get("thresholds")
            if isinstance(thresholds, dict):
                return {
                    "threshold_enabled": device.get("threshold_enabled", device.get("thresholdEnabled", True)),
                    "thresholdEnabled": device.get("thresholdEnabled", device.get("threshold_enabled", True)),
                    "thresholds": thresholds,
                }
            cfg = device.get("threshold_config") or device.get("thresholdConfig")
            if isinstance(cfg, dict):
                return cfg

        cfg = data.get("threshold_config") or data.get("thresholdConfig")
        if isinstance(cfg, dict):
            return cfg

        thresholds = data.get("thresholds")
        if isinstance(thresholds, dict):
            return {
                "threshold_enabled": data.get("threshold_enabled", data.get("thresholdEnabled", True)),
                "thresholdEnabled": data.get("thresholdEnabled", data.get("threshold_enabled", True)),
                "thresholds": thresholds,
            }

        payload = data.get("payload")
        if isinstance(payload, dict):
            thresholds = payload.get("thresholds")
            if isinstance(thresholds, dict):
                return {
                    "threshold_enabled": payload.get("threshold_enabled", payload.get("thresholdEnabled", True)),
                    "thresholdEnabled": payload.get("thresholdEnabled", payload.get("threshold_enabled", True)),
                    "thresholds": thresholds,
                }
            cfg = payload.get("threshold_config") or payload.get("thresholdConfig")
            if isinstance(cfg, dict):
                return cfg

        if "threshold_enabled" in data or "thresholdEnabled" in data:
            enabled = data.get("threshold_enabled", data.get("thresholdEnabled", False))
            return {"threshold_enabled": enabled, "thresholdEnabled": enabled, "thresholds": {}}

        if isinstance(payload, dict) and ("threshold_enabled" in payload or "thresholdEnabled" in payload):
            enabled = payload.get("threshold_enabled", payload.get("thresholdEnabled", False))
            return {"threshold_enabled": enabled, "thresholdEnabled": enabled, "thresholds": {}}

        return None

    def get_device_options(self, data: Dict[str, Any]) -> Dict[str, Any]:
        device = data.get("device")
        if isinstance(device, dict):
            options = device.get("device_options")
            if isinstance(options, dict):
                return options
        return {}

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
            normalized = dict(cfg)
            enabled = bool(cfg.get("threshold_enabled", cfg.get("thresholdEnabled", True)))
            normalized["threshold_enabled"] = enabled
            normalized["thresholdEnabled"] = enabled
            return normalized

        enable = bool(cfg.get("enable", cfg.get("enable_alarm", cfg.get("enableAlarm", True))))
        threshold_enabled = bool(cfg.get("threshold_enabled", cfg.get("thresholdEnabled", True)))
        result = {
            "threshold_enabled": threshold_enabled,
            "thresholdEnabled": threshold_enabled,
            "thresholds": {},
        }

        if any(key in cfg for key in ["temp_high", "tempHigh", "alarm_high", "alarmHigh"]):
            temp_high = float(cfg.get("temp_high", cfg.get("tempHigh", cfg.get("alarm_high", cfg.get("alarmHigh")))))
            result["thresholds"]["temperature"] = {
                "enable_alarm": enable,
                "enableAlarm": enable,
                "alarm_high": temp_high,
                "alarmHigh": temp_high,
            }

        if any(key in cfg for key in ["humi_high", "humiHigh"]):
            humi_high = float(cfg.get("humi_high", cfg.get("humiHigh")))
            result["thresholds"]["humidity"] = {
                "enable_alarm": enable,
                "enableAlarm": enable,
                "alarm_high": humi_high,
                "alarmHigh": humi_high,
            }

        return result

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
