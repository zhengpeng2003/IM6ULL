# handlers/add_device_handler.py
# -*- coding: utf-8 -*-

import threading
import time

from handlers.base_handler import BaseHandler
from core import protocol


class AddDeviceHandler(BaseHandler):
    name = "add_device"

    def can_handle(self, topic, data):
        return self.get_cmd(data) == "add_device"

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)
        slave_id = self.get_slave_id(data)
        device_type = self.get_device_type(data)
        poll_interval_ms = self.get_poll_interval_ms(data)
        threshold_config = self.get_threshold_config(data)

        print(
            f"[HANDLER][add_device] "
            f"seq={seq}, slave_id={slave_id}, type={device_type}, interval={poll_interval_ms}"
        )
        print(f"[RX CMD] add_device seq={seq} slave_id={slave_id}")

        if slave_id is None:
            gateway.publish(
                protocol.ack(
                    cmd="add_device",
                    seq=seq,
                    ok=False,
                    reason="invalid_slave_id",
                    slave_id=None,
                    device_type=device_type,
                )
            )
            return True

        # 2：模拟 Modbus 设备无响应
        if slave_id == 2:
            gateway.publish(
                protocol.ack(
                    cmd="add_device",
                    seq=seq,
                    ok=False,
                    reason="device_no_response",
                    slave_id=slave_id,
                    device_type=device_type,
                )
            )
            return True

        # 3：模拟重复设备
        if slave_id == 3 and gateway.state.exists(slave_id):
            gateway.publish(
                protocol.ack(
                    cmd="add_device",
                    seq=seq,
                    ok=False,
                    reason="device_exists",
                    slave_id=slave_id,
                    device_type=device_type,
                )
            )
            return True

        # 4：模拟端口不存在
        if slave_id == 4:
            gateway.publish(
                protocol.ack(
                    cmd="add_device",
                    seq=seq,
                    ok=False,
                    reason="port_not_found",
                    slave_id=slave_id,
                    device_type=device_type,
                )
            )
            return True

        # 6：模拟延迟 ACK
        if slave_id == 6:
            def delayed_ack():
                time.sleep(5)
                gateway.state.add_device(
                    slave_id=slave_id,
                    device_type=device_type,
                    poll_interval_ms=poll_interval_ms,
                    threshold_config=threshold_config,
                )
                gateway.publish(
                    protocol.ack(
                        cmd="add_device",
                        seq=seq,
                        ok=True,
                        reason="ok_after_5s",
                        slave_id=slave_id,
                        device_type=device_type,
                    )
                )
                gateway.publish_snapshot("add_device_delayed_ack")

            threading.Thread(target=delayed_ack, daemon=True).start()
            return True

        # 7：强制模拟继电器
        if slave_id == 7:
            device_type = "relay"

        # 默认：真实成功
        gateway.state.add_device(
            slave_id=slave_id,
            device_type=device_type,
            poll_interval_ms=poll_interval_ms,
            threshold_config=threshold_config,
        )

        gateway.publish(
            protocol.ack(
                cmd="add_device",
                seq=seq,
                ok=True,
                reason="ok",
                slave_id=slave_id,
                device_type=device_type,
            )
        )

        # 关键：ACK 后立刻发 config_snapshot，让 Pc_data/Pc_ui 同步真实设备表
        gateway.publish_snapshot("add_device_success")

        return True
