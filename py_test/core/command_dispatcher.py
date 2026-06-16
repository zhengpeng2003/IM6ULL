# core/command_dispatcher.py
# -*- coding: utf-8 -*-

from typing import Dict, Any, List


class CommandDispatcher:
    def __init__(self, handlers: List):
        self.handlers = handlers

    def dispatch(self, gateway, topic: str, data: Dict[str, Any]) -> bool:
        for handler in self.handlers:
            if handler.can_handle(topic, data):
                print(f"[DISPATCH] handler = {handler.name}")
                return handler.handle(gateway, topic, data)

        print("[DISPATCH][WARN] no handler matched")
        return False