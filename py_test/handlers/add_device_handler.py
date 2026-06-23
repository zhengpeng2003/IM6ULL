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
        threshold_config = self.get_threshold_config(data) if self.has_threshold_config(data) else None
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
                threshold_config_source="add_device" if threshold_config is not None else "",
                device_options=device_options,
            )
            self.log_add_device_threshold(gateway, port_id, slave_id, final_device_type, threshold_config)
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

    def log_add_device_threshold(self, gateway, port_id, slave_id, device_type, threshold_config):
        prefix = (
            f"[ADD_DEVICE_THRESHOLD] gateway={gateway.config.gatewayId} "
            f"port={port_id} slave_id={slave_id} device_type={device_type}"
        )
        if not isinstance(threshold_config, dict):
            if slave_id in {9, 10}:
                print(f"{prefix} threshold_enabled=false thresholds=null")
            return

        enabled = bool(threshold_config.get("threshold_enabled", threshold_config.get("thresholdEnabled", False)))
        thresholds = threshold_config.get("thresholds")
        if slave_id in {9, 10}:
            print(f"{prefix} threshold_enabled={str(enabled).lower()} thresholds={thresholds}")
