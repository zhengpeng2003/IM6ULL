# core/mock_gateway_fleet.py
# -*- coding: utf-8 -*-

import time
from typing import List

from core.behavior_profile import BehaviorProfile
from core.gateway_config import GatewayConfig, legacy_default_config, load_scenario
from core.mock_gateway import MockGateway


class MockGatewayFleet:
    def __init__(self, gateways: List[MockGateway]):
        self.gateways = gateways
        self.running = False

    @classmethod
    def from_legacy_default(cls) -> "MockGatewayFleet":
        return cls.from_configs([legacy_default_config()])

    @classmethod
    def from_scenario(cls, path: str) -> "MockGatewayFleet":
        return cls.from_configs(load_scenario(path))

    @classmethod
    def from_configs(cls, configs: List[GatewayConfig]) -> "MockGatewayFleet":
        gateways = []
        for index, gateway_config in enumerate(configs):
            behavior = BehaviorProfile.from_config(gateway_config.behavior)
            gateways.append(MockGateway(gateway_config=gateway_config, behavior_profile=behavior, index=index))
        return cls(gateways)

    def add_gateway_from_config(self, gateway_config: GatewayConfig):
        behavior = BehaviorProfile.from_config(gateway_config.behavior)
        self.gateways.append(
            MockGateway(gateway_config=gateway_config, behavior_profile=behavior, index=len(self.gateways))
        )

    def start_all(self):
        self.running = True
        for gateway in self.gateways:
            gateway.start()

    def stop_all(self):
        self.running = False
        for gateway in self.gateways:
            gateway.stop()

    def loop_forever(self):
        self.start_all()
        try:
            while self.running:
                time.sleep(0.5)
        finally:
            self.stop_all()
