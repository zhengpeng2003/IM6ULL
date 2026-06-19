# handlers/__init__.py
# -*- coding: utf-8 -*-

from handlers.snapshot_handler import SnapshotHandler
from handlers.add_device_handler import AddDeviceHandler
from handlers.remove_device_handler import RemoveDeviceHandler
from handlers.set_relay_handler import SetRelayHandler
from handlers.set_threshold_handler import SetThresholdHandler
from handlers.discover_handler import DiscoverHandler


def create_handlers():
    return [
        SnapshotHandler(),
        AddDeviceHandler(),
        RemoveDeviceHandler(),
        SetThresholdHandler(),
        SetRelayHandler(),
        DiscoverHandler(),
    ]
