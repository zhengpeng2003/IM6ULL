# handlers/discover_handler.py
# -*- coding: utf-8 -*-

import config
from core import protocol
from handlers.base_handler import BaseHandler


class DiscoverHandler(BaseHandler):
    name = "discover"

    def can_handle(self, topic, data):
        msg_type = self.get_type(data)
        cmd = self.get_cmd(data)

        return (
            topic == config.BROADCAST_DOWN_TOPIC
            or msg_type == "discover_gateways"
            or cmd == "discover_gateways"
            or msg_type == "gateway_discover"
            or cmd == "gateway_discover"
        )

    def handle(self, gateway, topic, data):
        print("[HANDLER][discover] publish gateway_register and config_snapshot")

        gateway.publish(protocol.gateway_register())
        gateway.publish_snapshot("discover_gateways")
        return True
