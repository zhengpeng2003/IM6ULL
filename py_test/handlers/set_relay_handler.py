# handlers/set_relay_handler.py
# -*- coding: utf-8 -*-

from handlers.base_handler import BaseHandler
from core import protocol


class SetRelayHandler(BaseHandler):
    name = "set_relay"

    def can_handle(self, topic, data):
        return self.get_cmd(data) == "set_relay"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)
        states = data.get("states")

        payload = data.get("payload")
        if not isinstance(states, list) and isinstance(payload, dict):
            states = payload.get("states")

        print(f"[HANDLER][set_relay] seq={seq}, slave_id={slave_id}, states={states}")
        print(f"[RX CMD] set_relay seq={seq} slave_id={slave_id}")

        if slave_id is None:
            gateway.publish(
                protocol.ack(
                    cmd="set_relay",
                    seq=seq,
                    ok=False,
                    reason="invalid_slave_id",
                    slave_id=None,
                    device_type="relay",
                    cmd_id=cmd_id,
                )
            )
            return True

        if not isinstance(states, list) or len(states) == 0:
            gateway.publish(
                protocol.ack(
                    cmd="set_relay",
                    seq=seq,
                    ok=False,
                    reason="invalid_relay_states",
                    slave_id=slave_id,
                    device_type="relay",
                    cmd_id=cmd_id,
                )
            )
            return True

        device = gateway.state.get_device(slave_id)
        if device is None:
            gateway.publish(
                protocol.ack(
                    cmd="set_relay",
                    seq=seq,
                    ok=False,
                    reason="device_not_found",
                    slave_id=slave_id,
                    device_type="relay",
                    cmd_id=cmd_id,
                )
            )
            return True

        device_type = device.get("device_type") or device.get("deviceType") or "sensor_th"
        if device_type != "relay":
            gateway.publish(
                protocol.ack(
                    cmd="set_relay",
                    seq=seq,
                    ok=False,
                    reason="not_relay_device",
                    slave_id=slave_id,
                    device_type=device_type,
                    cmd_id=cmd_id,
                )
            )
            return True

        bool_states = [bool(state) for state in states]
        gateway.state.set_relay_states(slave_id, bool_states)
        gateway.publish(
            protocol.ack(
                cmd="set_relay",
                seq=seq,
                ok=True,
                reason="ok",
                slave_id=slave_id,
                device_type="relay",
                cmd_id=cmd_id,
            )
        )
        return True
