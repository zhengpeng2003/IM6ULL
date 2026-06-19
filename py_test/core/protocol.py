# core/protocol.py
# -*- coding: utf-8 -*-

import random
import time
from typing import Dict, Any, List, Optional

import config


_seq = 1000


def now_ms() -> int:
    return int(time.time() * 1000)


def next_seq() -> int:
    global _seq
    _seq += 1
    return _seq


def site() -> Dict[str, Any]:
    return {
        "factoryId": config.FACTORY_ID,
        "factoryName": config.FACTORY_NAME,
        "areaId": config.AREA_ID,
        "areaName": config.AREA_NAME,
        "gatewayId": config.GATEWAY_ID,
        "gatewayName": config.GATEWAY_NAME,
        "portId": config.PORT_ID,
        "portName": config.PORT_NAME,
    }


def gateway_register() -> Dict[str, Any]:
    return {
        "type": "gateway_register",
        "version": 1,
        "seq": next_seq(),
        "timestampMs": now_ms(),

        "gatewayId": config.GATEWAY_ID,
        "gatewayName": config.GATEWAY_NAME,
        "gateway_id": config.GATEWAY_ID,
        "gateway_name": config.GATEWAY_NAME,

        "factoryId": config.FACTORY_ID,
        "factoryName": config.FACTORY_NAME,
        "areaId": config.AREA_ID,
        "areaName": config.AREA_NAME,

        "status": "online",
        "upTopic": config.UP_TOPIC,
        "cmdTopic": config.CMD_TOPIC,

        "ports": [
            {
                "portId": config.PORT_ID,
                "portName": config.PORT_NAME,
                "slot": config.PORT_SLOT,
                "port": config.PORT_PATH,
                "path": config.PORT_PATH,
                "baud": config.PORT_BAUD,
                "connected": True,
                "status": "connected",
            }
        ],
    }


def gateway_heartbeat() -> Dict[str, Any]:
    return {
        "type": "gateway_heartbeat",
        "version": 1,
        "seq": next_seq(),
        "timestampMs": now_ms(),
        "gatewayId": config.GATEWAY_ID,
        "gateway_id": config.GATEWAY_ID,
        "status": "online",
    }


def config_snapshot(devices: List[Dict[str, Any]], reason: str, seq: Optional[int] = None) -> Dict[str, Any]:
    """
    Pc_data 的 MqttMessageHandler 同时兼容 config_snapshot 和 device_config_snapshot。
    Mock 默认发送 config_snapshot，保持与当前测试流程一致。
    """
    return {
        "type": "config_snapshot",
        "version": 1,
        "seq": seq if seq is not None and seq > 0 else next_seq(),
        "timestampMs": now_ms(),
        "reason": reason,

        "fullSnapshot": True,
        "full_snapshot": True,

        "gatewayId": config.GATEWAY_ID,
        "gatewayName": config.GATEWAY_NAME,
        "gateway_id": config.GATEWAY_ID,
        "gateway_name": config.GATEWAY_NAME,

        "site": site(),

        "ports": [
            {
                "portId": config.PORT_ID,
                "portName": config.PORT_NAME,
                "slot": config.PORT_SLOT,
                "port": config.PORT_PATH,
                "path": config.PORT_PATH,
                "baud": config.PORT_BAUD,
                "connected": True,
                "status": "connected",
                "devices": devices,
            }
        ],
    }


def ack(
    cmd: str,
    seq: int,
    ok: bool,
    reason: str,
    slave_id: Optional[int],
    device_type: str = "sensor_th",
    cmd_id: str = "",
) -> Dict[str, Any]:
    status = "success" if ok else "failed"
    message = reason or ("success" if ok else "failed")

    return {
        "type": "ack",
        "stage": "done",
        "cmd_id": cmd_id,

        "cmd": cmd,
        "cmdType": cmd,
        "command": cmd,
        "commandType": cmd,
        "action": cmd,

        "seq": seq,
        "sequence": seq,

        "ok": ok,
        "success": ok,
        "status": status,

        "reason": reason,
        "message": message,

        "timestampMs": now_ms(),

        "gatewayId": config.GATEWAY_ID,
        "gateway_id": config.GATEWAY_ID,
        "gatewayName": config.GATEWAY_NAME,

        "portId": config.PORT_ID,
        "port_id": config.PORT_ID,
        "portName": config.PORT_NAME,

        "slot": config.PORT_SLOT,
        "master_slot": config.PORT_SLOT,

        "deviceId": slave_id if slave_id is not None else 0,
        "device_id": slave_id if slave_id is not None else 0,
        "slave_id": slave_id if slave_id is not None else 0,
        "slaveAddr": slave_id if slave_id is not None else 0,
        "slaveAddress": slave_id if slave_id is not None else 0,

        "deviceType": device_type,
        "device_type": device_type,

        "pollIntervalMs": 1000,
        "poll_interval_ms": 1000,
    }


def telemetry_pack(devices: List[Dict[str, Any]]) -> Dict[str, Any]:
    telemetry_devices = []

    for dev in devices:
        slave_id = int(dev.get("slave_id", 0))
        device_type = dev.get("device_type") or dev.get("deviceType") or "sensor_th"

        if device_type == "relay":
            relay_states = dev.get("relay_states") or dev.get("relayStates") or [False]
            ch1_state = bool(relay_states[0]) if relay_states else False
            telemetry_devices.append(
                {
                    "deviceId": str(slave_id),
                    "device_id": slave_id,
                    "slave_id": slave_id,
                    "slaveAddr": slave_id,
                    "slaveAddress": slave_id,

                    "deviceType": "relay",
                    "device_type": "relay",
                    "deviceName": f"继电器从站{slave_id}",
                    "device_name": f"继电器从站{slave_id}",

                    "valid": True,
                    "comm_status": "online",
                    "data_status": "normal",

                    "points": [
                        {
                            "pointKey": "relay.ch1",
                            "pointName": "继电器通道1",
                            "valueType": "Boolean",
                            "boolValue": ch1_state,
                            "unit": "",
                        }
                    ],
                }
            )
        else:
            telemetry_devices.append(
                {
                    "deviceId": str(slave_id),
                    "device_id": slave_id,
                    "slave_id": slave_id,
                    "slaveAddr": slave_id,
                    "slaveAddress": slave_id,

                    "deviceType": "sensor_th",
                    "device_type": "sensor_th",
                    "deviceName": f"温湿度从站{slave_id}",
                    "device_name": f"温湿度从站{slave_id}",

                    "valid": True,
                    "comm_status": "online",
                    "data_status": "normal",

                    "points": [
                        {
                            "pointKey": "temperature",
                            "pointName": "温度",
                            "valueType": "Number",
                            "numberValue": round(25.0 + random.random() * 3.0, 2),
                            "unit": "℃",
                        },
                        {
                            "pointKey": "humidity",
                            "pointName": "湿度",
                            "valueType": "Number",
                            "numberValue": round(60.0 + random.random() * 5.0, 2),
                            "unit": "%",
                        },
                    ],
                }
            )

    seq = next_seq()

    return {
        "type": "telemetry_pack",
        "version": 1,

        "seq": seq,
        "sequence": seq,

        "timestampMs": now_ms(),

        "sourceId": config.GATEWAY_ID,
        "targetId": "pc_data",

        "site": site(),
        "devices": telemetry_devices,
    }
