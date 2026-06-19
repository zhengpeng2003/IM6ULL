# handlers/discover_handler.py
# -*- coding: utf-8 -*-

from core import protocol
from handlers.base_handler import BaseHandler


class DiscoverHandler(BaseHandler):
    name = "discover"

    def can_handle(self, topic, data):
        _ = topic
        return self.get_cmd(data) == "discover"

    def handle(self, gateway, topic, data):
        print("[HANDLER][discover] publish gateway_register and config_snapshot")

        gateway.publish(protocol.gateway_register())
        gateway.publish_snapshot("discover_gateways")
        return True
