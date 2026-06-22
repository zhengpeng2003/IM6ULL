# handlers/remove_device_handler.py
# -*- coding: utf-8 -*-

import threading
import time

from handlers.base_handler import BaseHandler


class RemoveDeviceHandler(BaseHandler):
    name = "remove_device"

    def can_handle(self, topic, data):
        return self.get_cmd(data) == "remove_device"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)
        port_id = self.get_port_id(data, gateway.port_id_from_topic(topic) or gateway.default_port_id())

        print(f"[HANDLER][remove_device] seq={seq}, port_id={port_id}, slave_id={slave_id}")

        if slave_id is None:
            gateway.publish_ack(
                cmd="remove_device",
                seq=seq,
                ok=False,
                reason="invalid_slave_id",
                slave_id=None,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        exists = gateway.state.exists(port_id, slave_id)
        decision = gateway.behavior.evaluate_remove_device(port_id, slave_id, exists)

        if not decision.ok:
            gateway.publish_ack(
                cmd="remove_device",
                seq=seq,
                ok=False,
                reason=decision.reason,
                slave_id=slave_id,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        def complete_success():
            if decision.delay_ack_sec > 0:
                time.sleep(decision.delay_ack_sec)
            gateway.state.remove_device(port_id, slave_id)
            gateway.publish_ack(
                cmd="remove_device",
                seq=seq,
                ok=True,
                reason=decision.reason,
                slave_id=slave_id,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            gateway.publish_snapshot(decision.snapshot_reason or "remove_device_success")

        if decision.delay_ack_sec > 0:
            threading.Thread(target=complete_success, daemon=True).start()
        else:
            complete_success()

        return True
