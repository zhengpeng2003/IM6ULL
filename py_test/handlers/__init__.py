# handlers/__init__.py
# -*- coding: utf-8 -*-

from handlers.snapshot_handler import SnapshotHandler
from handlers.add_device_handler import AddDeviceHandler
from handlers.remove_device_handler import RemoveDeviceHandler


def create_handlers():
    return [
        SnapshotHandler(),
        AddDeviceHandler(),
        RemoveDeviceHandler(),
    ]
