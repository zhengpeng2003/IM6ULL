# handlers/set_relay_handler.py
# -*- coding: utf-8 -*-

import threading
import time

from handlers.base_handler import BaseHandler


def _relay_bool(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "on", "yes"}
    return bool(value)


class SetRelayHandler(BaseHandler):
    name = "set_relay"

    def can_handle(self, topic, data):
        return self.get_cmd(data) == "set_relay"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)
        port_id = self.get_port_id(data, gateway.port_id_from_topic(topic) or gateway.default_port_id())
        states = data.get("states")

        print(f"[HANDLER][set_relay] cmd_id={cmd_id}, seq={seq}, port_id={port_id}, slave_id={slave_id}, states={states}")

        if slave_id is None:
            gateway.publish_ack(
                cmd="set_relay",
                seq=seq,
                ok=False,
                reason="invalid_slave_id",
                slave_id=None,
                device_type="relay",
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        device = gateway.state.get_device(port_id, slave_id)
        device_type = ""
        if device:
            device_type = device.get("device_type") or device.get("deviceType") or "sensor_th"
        states_valid = isinstance(states, list) and len(states) > 0
        decision = gateway.behavior.evaluate_set_relay(
            port_id=port_id,
            slave_id=slave_id,
            device_exists=device is not None,
            is_relay=device_type == "relay",
            states_valid=states_valid,
        )

        if not decision.ok:
            gateway.publish_ack(
                cmd="set_relay",
                seq=seq,
                ok=False,
                reason=decision.reason,
                slave_id=slave_id,
                device_type=device_type or "relay",
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        bool_states = [_relay_bool(state) for state in states]
        gateway.state.set_relay_states(port_id, slave_id, bool_states)

        def publish_success():
            if decision.delay_ack_sec > 0.0:
                print(f"[HANDLER][set_relay] delay ack {decision.delay_ack_sec:.1f}s")
                time.sleep(decision.delay_ack_sec)
            gateway.publish_ack(
                cmd="set_relay",
                seq=seq,
                ok=True,
                reason=decision.reason,
                slave_id=slave_id,
                device_type="relay",
                cmd_id=cmd_id,
                port_id=port_id,
            )

        if decision.delay_ack_sec > 0:
            threading.Thread(target=publish_success, daemon=True).start()
        else:
            publish_success()
        return True
