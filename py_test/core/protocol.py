# core/protocol.py
# -*- coding: utf-8 -*-

import random
import time
from typing import Any, Dict, List, Optional

import config
from core.gateway_config import GatewayConfig, PortConfig, legacy_default_config


_seq = 1000


def now_ms() -> int:
    return int(time.time() * 1000)


def next_seq() -> int:
    global _seq
    _seq += 1
    return _seq


def _ctx(context: Optional[GatewayConfig] = None) -> GatewayConfig:
    return context or legacy_default_config()


def _port(context: GatewayConfig, port_id: Optional[str] = None) -> PortConfig:
    return context.port_config(port_id)


def site(context: Optional[GatewayConfig] = None, port_id: Optional[str] = None) -> Dict[str, Any]:
    ctx = _ctx(context)
    port = _port(ctx, port_id)
    return {
        "factoryId": ctx.factoryId,
        "factoryName": ctx.factoryName,
        "areaId": ctx.areaId,
        "areaName": ctx.areaName,
        "gatewayId": ctx.gatewayId,
        "gatewayName": ctx.gatewayName,
        "portId": port.portId,
        "portName": port.portName,
    }


def mock_device_name(slave_id: int) -> str:
    return f"Device {slave_id}"


def _port_payload(port: PortConfig, devices: Optional[List[Dict[str, Any]]] = None) -> Dict[str, Any]:
    payload = {
        "portId": port.portId,
        "portName": port.portName,
        "slot": port.slot,
        "port": port.port,
        "path": port.path,
        "baud": port.baud,
        "connected": True,
        "status": "connected",
    }
    if devices is not None:
        payload["devices"] = devices
    return payload


def gateway_register(context: Optional[GatewayConfig] = None) -> Dict[str, Any]:
    ctx = _ctx(context)
    return {
        "type": "gateway_register",
        "version": 1,
        "seq": next_seq(),
        "timestampMs": now_ms(),
        "gatewayId": ctx.gatewayId,
        "gatewayName": ctx.gatewayName,
        "gateway_id": ctx.gatewayId,
        "gateway_name": ctx.gatewayName,
        "factoryId": ctx.factoryId,
        "factoryName": ctx.factoryName,
        "areaId": ctx.areaId,
        "areaName": ctx.areaName,
        "status": "online",
        "upTopic": ctx.up_topic(),
        "cmdTopic": ctx.cmd_topic(),
        "ports": [_port_payload(port) for port in ctx.ports],
    }


def gateway_heartbeat(context: Optional[GatewayConfig] = None) -> Dict[str, Any]:
    ctx = _ctx(context)
    return {
        "type": "gateway_heartbeat",
        "version": 1,
        "seq": next_seq(),
        "timestampMs": now_ms(),
        "gatewayId": ctx.gatewayId,
        "gateway_id": ctx.gatewayId,
        "status": "online",
    }


def config_snapshot(
    devices: Optional[List[Dict[str, Any]]] = None,
    reason: str = "",
    seq: Optional[int] = None,
    context: Optional[GatewayConfig] = None,
    ports: Optional[List[Dict[str, Any]]] = None,
) -> Dict[str, Any]:
    """
    Pc_data 的 MqttMessageHandler 同时兼容 config_snapshot 和 device_config_snapshot。
    Mock 默认发送 config_snapshot，保持与当前测试流程一致。
    """
    ctx = _ctx(context)
    if ports is None:
        port = _port(ctx)
        ports = [_port_payload(port, devices or [])]

    return {
        "type": "config_snapshot",
        "version": 1,
        "seq": seq if seq is not None and seq > 0 else next_seq(),
        "timestampMs": now_ms(),
        "reason": reason,
        "fullSnapshot": True,
        "full_snapshot": True,
        "gatewayId": ctx.gatewayId,
        "gatewayName": ctx.gatewayName,
        "gateway_id": ctx.gatewayId,
        "gateway_name": ctx.gatewayName,
        "site": site(ctx, ctx.default_port_id),
        "ports": ports,
    }


def ack(
    cmd: str,
    seq: int,
    ok: bool,
    reason: str,
    slave_id: Optional[int],
    device_type: str = "sensor_th",
    cmd_id: str = "",
    context: Optional[GatewayConfig] = None,
    port_id: Optional[str] = None,
) -> Dict[str, Any]:
    ctx = _ctx(context)
    port = _port(ctx, port_id)
    status = "success" if ok else "failed"
    message = reason or ("success" if ok else "failed")
    device_id = slave_id if slave_id is not None else 0

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
        "gatewayId": ctx.gatewayId,
        "gateway_id": ctx.gatewayId,
        "gatewayName": ctx.gatewayName,
        "portId": port.portId,
        "port_id": port.portId,
        "portName": port.portName,
        "slot": port.slot,
        "master_slot": port.slot,
        "deviceId": device_id,
        "device_id": device_id,
        "slave_id": device_id,
        "slaveAddr": device_id,
        "slaveAddress": device_id,
        "deviceType": device_type,
        "device_type": device_type,
        "pollIntervalMs": 1000,
        "poll_interval_ms": 1000,
    }


def telemetry_pack(
    devices: List[Dict[str, Any]],
    context: Optional[GatewayConfig] = None,
    port_id: Optional[str] = None,
) -> Dict[str, Any]:
    ctx = _ctx(context)
    telemetry_devices = []

    for dev in devices:
        slave_id = int(dev.get("slave_id", 0))
        device_type = dev.get("device_type") or dev.get("deviceType") or "sensor_th"

        if device_type == "relay":
            relay_states = dev.get("relay_states") or dev.get("relayStates") or [False]
            points = []
            for index, state in enumerate(relay_states, start=1):
                points.append(
                    {
                        "pointKey": f"relay.ch{index}",
                        "pointName": f"继电器通道{index}",
                        "valueType": "Boolean",
                        "boolValue": bool(state),
                        "unit": "",
                    }
                )
            telemetry_devices.append(
                {
                    "deviceId": str(slave_id),
                    "device_id": slave_id,
                    "slave_id": slave_id,
                    "slaveAddr": slave_id,
                    "slaveAddress": slave_id,
                    "deviceType": "relay",
                    "device_type": "relay",
                    "deviceName": mock_device_name(slave_id),
                    "device_name": mock_device_name(slave_id),
                    "valid": True,
                    "comm_status": "online",
                    "data_status": "normal",
                    "channelCount": len(relay_states),
                    "channel_count": len(relay_states),
                    "points": points,
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
                    "deviceName": mock_device_name(slave_id),
                    "device_name": mock_device_name(slave_id),
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
        "sourceId": ctx.gatewayId,
        "targetId": "pc_data",
        "site": site(ctx, port_id),
        "devices": telemetry_devices,
    }
