# handlers/add_device_handler.py
# -*- coding: utf-8 -*-

import threading
import time

from handlers.base_handler import BaseHandler


class AddDeviceHandler(BaseHandler):
    name = "add_device"

    def can_handle(self, topic, data):
        return self.get_cmd(data) == "add_device"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)
        port_id = self.get_port_id(data, gateway.port_id_from_topic(topic) or gateway.default_port_id())
        device_type = self.get_device_type(data)
        poll_interval_ms = self.get_poll_interval_ms(data)
        threshold_config = self.get_threshold_config(data)
        device_options = self.get_device_options(data)

        print(
            f"[HANDLER][add_device] "
            f"seq={seq}, port_id={port_id}, slave_id={slave_id}, type={device_type}, interval={poll_interval_ms}"
        )

        if slave_id is None:
            gateway.publish_ack(
                cmd="add_device",
                seq=seq,
                ok=False,
                reason="invalid_slave_id",
                slave_id=None,
                device_type=device_type,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        exists = gateway.state.exists(port_id, slave_id)
        decision = gateway.behavior.evaluate_add_device(port_id, slave_id, device_type, exists)
        final_device_type = decision.force_device_type or device_type

        if not decision.ok:
            gateway.publish_ack(
                cmd="add_device",
                seq=seq,
                ok=False,
                reason=decision.reason,
                slave_id=slave_id,
                device_type=final_device_type,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        def complete_success():
            if decision.delay_ack_sec > 0:
                time.sleep(decision.delay_ack_sec)
            gateway.state.add_device(
                port_id,
                slave_id,
                device_type=final_device_type,
                poll_interval_ms=poll_interval_ms,
                threshold_config=threshold_config,
                device_options=device_options,
            )
            gateway.publish_ack(
                cmd="add_device",
                seq=seq,
                ok=True,
                reason=decision.reason,
                slave_id=slave_id,
                device_type=final_device_type,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            gateway.publish_snapshot(decision.snapshot_reason or "add_device_success")

        if decision.delay_ack_sec > 0:
            threading.Thread(target=complete_success, daemon=True).start()
        else:
            complete_success()

        return True
