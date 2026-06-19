# handlers/snapshot_handler.py
# -*- coding: utf-8 -*-

from handlers.base_handler import BaseHandler


class SnapshotHandler(BaseHandler):
    name = "snapshot"

    def can_handle(self, topic, data):
        msg_type = self.get_type(data)
        cmd = self.get_cmd(data)

        return (
            msg_type == "request_config_snapshot"
            or cmd == "request_config_snapshot"
            or msg_type == "get_config_snapshot"
            or cmd == "get_config_snapshot"
            or msg_type == "get_config"
            or cmd == "get_config"
        )

    def handle(self, gateway, topic, data):
        seq = self.get_seq(data)

        print(f"[HANDLER][snapshot] request config snapshot, seq={seq}")

        # 如果 Pc_data 发 request_config_snapshot 带 seq，这里用同一个 seq 回去
        gateway.publish_snapshot("request_config_snapshot", seq=seq)
        return True
