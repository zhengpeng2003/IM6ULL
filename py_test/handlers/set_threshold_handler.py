# handlers/set_threshold_handler.py
# -*- coding: utf-8 -*-

from handlers.base_handler import BaseHandler


class SetThresholdHandler(BaseHandler):
    name = "set_threshold"

    def can_handle(self, topic, data):
        cmd = self.get_cmd(data)
        return cmd == "set_threshold" or cmd == "set_device_threshold"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)
        port_id = self.get_port_id(data, gateway.port_id_from_topic(topic) or gateway.default_port_id())
        threshold_config = self.get_threshold_config(data)

        print(f"[HANDLER][set_threshold] seq={seq}, port_id={port_id}, slave_id={slave_id}")

        if slave_id is None:
            gateway.publish_ack(
                cmd="set_threshold",
                seq=seq,
                ok=False,
                reason="invalid_slave_id",
                slave_id=None,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        device = gateway.state.get_device(port_id, slave_id)
        decision = gateway.behavior.evaluate_set_threshold(
            port_id=port_id,
            slave_id=slave_id,
            device_exists=device is not None,
        )

        if not decision.ok:
            gateway.publish_ack(
                cmd="set_threshold",
                seq=seq,
                ok=False,
                reason=decision.reason,
                slave_id=slave_id,
                cmd_id=cmd_id,
                port_id=port_id,
            )
            return True

        device["threshold_config"] = threshold_config
        device["thresholdConfig"] = threshold_config

        gateway.publish_ack(
            cmd="set_threshold",
            seq=seq,
            ok=True,
            reason=decision.reason,
            slave_id=slave_id,
            device_type=device.get("device_type") or device.get("deviceType") or "sensor_th",
            cmd_id=cmd_id,
            port_id=port_id,
        )
        gateway.publish_snapshot(decision.snapshot_reason or "set_threshold_success")
        return True
