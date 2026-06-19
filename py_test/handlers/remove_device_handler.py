# handlers/remove_device_handler.py
# -*- coding: utf-8 -*-

import threading
import time

from handlers.base_handler import BaseHandler
from core import protocol


class RemoveDeviceHandler(BaseHandler):
    name = "remove_device"

    def can_handle(self, topic, data):
        return self.get_cmd(data) == "remove_device"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        cmd_id = self.get_cmd_id(data)
        slave_id = self.get_slave_id(data)

        print(f"[HANDLER][remove_device] seq={seq}, slave_id={slave_id}")
        print(f"[RX CMD] remove_device seq={seq} slave_id={slave_id}")

        if slave_id is None:
            gateway.publish(
                protocol.ack(
                    cmd="remove_device",
                    seq=seq,
                    ok=False,
                    reason="invalid_slave_id",
                    slave_id=None,
                    cmd_id=cmd_id,
                )
            )
            return True

        # 2：模拟设备不存在
        if slave_id == 2:
            gateway.publish(
                protocol.ack(
                    cmd="remove_device",
                    seq=seq,
                    ok=False,
                    reason="device_not_found",
                    slave_id=slave_id,
                    cmd_id=cmd_id,
                )
            )
            return True

        # 5：模拟延迟删除
        if slave_id == 5:
            def delayed_remove():
                time.sleep(5)
                gateway.state.remove_device(slave_id)
                gateway.publish(
                    protocol.ack(
                        cmd="remove_device",
                        seq=seq,
                        ok=True,
                        reason="ok_after_5s",
                        slave_id=slave_id,
                        cmd_id=cmd_id,
                    )
                )
                gateway.publish_snapshot("remove_device_delayed_ack")

            threading.Thread(target=delayed_remove, daemon=True).start()
            return True

        # 默认：真实删除
        gateway.state.remove_device(slave_id)

        gateway.publish(
            protocol.ack(
                cmd="remove_device",
                seq=seq,
                ok=True,
                reason="ok",
                slave_id=slave_id,
                cmd_id=cmd_id,
            )
        )

        # 关键：删除后发 config_snapshot，让 Pc_data/Pc_ui 刷新设备表
        gateway.publish_snapshot("remove_device_success")

        return True
