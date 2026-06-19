# handlers/set_threshold_handler.py
# -*- coding: utf-8 -*-

from handlers.base_handler import BaseHandler
from core import protocol


class SetThresholdHandler(BaseHandler):
    name = "set_threshold"

    def can_handle(self, topic, data):
        cmd = self.get_cmd(data)
        return cmd == "set_threshold" or cmd == "set_device_threshold"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)
        threshold_config = self.get_threshold_config(data)

        print(f"[HANDLER][set_threshold] seq={seq}, slave_id={slave_id}")

        if slave_id is None:
            gateway.publish(
                protocol.ack(
                    cmd="set_threshold",
                    seq=seq,
                    ok=False,
                    reason="invalid_slave_id",
                    slave_id=None,
                    cmd_id=cmd_id,
                )
            )
            return True

        device = gateway.state.get_device(slave_id)
        if device is None:
            gateway.publish(
                protocol.ack(
                    cmd="set_threshold",
                    seq=seq,
                    ok=False,
                    reason="device_not_found",
                    slave_id=slave_id,
                    cmd_id=cmd_id,
                )
            )
            return True

        device["threshold_config"] = threshold_config
        device["thresholdConfig"] = threshold_config

        gateway.publish(
            protocol.ack(
                cmd="set_threshold",
                seq=seq,
                ok=True,
                reason="ok",
                slave_id=slave_id,
                device_type=device.get("device_type") or device.get("deviceType") or "sensor_th",
                cmd_id=cmd_id,
            )
        )
        gateway.publish_snapshot("set_threshold_success")
        return True
