# -*- coding: utf-8 -*-

import sys
import types
import unittest
from pathlib import Path
from unittest.mock import patch


PY_TEST_DIR = Path(__file__).resolve().parents[1]
if str(PY_TEST_DIR) not in sys.path:
    sys.path.insert(0, str(PY_TEST_DIR))


def install_mqtt_stub():
    paho = types.ModuleType("paho")
    mqtt_pkg = types.ModuleType("paho.mqtt")
    client_mod = types.ModuleType("paho.mqtt.client")
    client_mod.Client = object
    mqtt_pkg.client = client_mod
    paho.mqtt = mqtt_pkg
    sys.modules.setdefault("paho", paho)
    sys.modules.setdefault("paho.mqtt", mqtt_pkg)
    sys.modules.setdefault("paho.mqtt.client", client_mod)


install_mqtt_stub()

from core.behavior_profile import BehaviorProfile
from core.gateway_config import GatewayConfig, PortConfig
from core.gateway_state import GatewayState
from core.mock_gateway import MockGateway
from core import protocol
from handlers.add_device_handler import AddDeviceHandler
from handlers.remove_device_handler import RemoveDeviceHandler


class ImmediateThread:
    def __init__(self, target=None, daemon=None):
        self.target = target
        self.daemon = daemon

    def start(self):
        if self.target:
            self.target()


def gateway_config():
    return GatewayConfig(
        gatewayId="gateway_002",
        gatewayName="Mock i.MX6ULL Abnormal",
        ports=[
            PortConfig(portId="port_001", portName="RS485-1", devices=[]),
            PortConfig(portId="port_002", portName="RS485-2", devices=[]),
        ],
        behavior={"profile": "custom"},
    )


def add_device_payload(
    slave_id,
    port_id="port_001",
    device_type="sensor_th",
    threshold_enabled=True,
    temp_high=30.0,
    temp_low=10.0,
):
    thresholds = {
        "temperature": {
            "enable_alarm": True,
        },
        "humidity": {
            "enable_alarm": False,
        },
    }
    if temp_high is not None:
        thresholds["temperature"]["alarm_high"] = temp_high
    if temp_low is not None:
        thresholds["temperature"]["alarm_low"] = temp_low
    payload = {
        "type": "command",
        "cmd": "add_device",
        "cmd_id": f"cmd-add-{port_id}-{slave_id}",
        "seq": 1000 + slave_id,
        "gatewayId": "gateway_002",
        "portId": port_id,
        "deviceId": slave_id,
        "slave_id": slave_id,
        "device_type": device_type,
        "poll_interval_ms": 1000,
        "device": {
            "portId": port_id,
            "slave_id": slave_id,
            "deviceId": slave_id,
            "slaveAddress": slave_id,
            "device_type": device_type,
            "deviceType": device_type,
            "poll_interval_ms": 1000,
            "pollIntervalMs": 1000,
        },
        "payload": {
            "portId": port_id,
            "slave_id": slave_id,
            "deviceId": slave_id,
            "device_type": device_type,
            "deviceType": device_type,
            "poll_interval_ms": 1000,
            "pollIntervalMs": 1000,
        },
    }
    if threshold_enabled is not None:
        payload["threshold_enabled"] = threshold_enabled
        payload["payload"]["threshold_enabled"] = threshold_enabled
    if temp_high is not None or temp_low is not None:
        payload["thresholds"] = thresholds
        payload["payload"]["thresholds"] = thresholds
    return payload


def remove_device_payload(slave_id, port_id="port_001"):
    return {
        "type": "command",
        "cmd": "remove_device",
        "cmd_id": f"cmd-remove-{port_id}-{slave_id}",
        "seq": 2000 + slave_id,
        "gatewayId": "gateway_002",
        "portId": port_id,
        "deviceId": slave_id,
        "slave_id": slave_id,
        "device": {
            "portId": port_id,
            "slave_id": slave_id,
            "deviceId": slave_id,
        },
    }


class FakeGateway:
    def __init__(self):
        self.config = gateway_config()
        self.behavior = BehaviorProfile.from_config(self.config.behavior)
        self.state = GatewayState(self.config.ports, self.config.default_port_id)
        self.published = []

    def port_id_from_topic(self, topic):
        prefix = self.config.cmd_topic() + "/"
        return topic[len(prefix):] if topic.startswith(prefix) else None

    def default_port_id(self):
        return self.config.default_port_id

    def publish_ack(self, **kwargs):
        self.published.append({"kind": "ack", **kwargs})

    def publish_snapshot(self, reason, seq=0):
        self.published.append({"kind": "snapshot", "reason": reason, "seq": seq})


def fake_mock_gateway(config):
    mock = MockGateway.__new__(MockGateway)
    mock.config = config
    mock.behavior = BehaviorProfile.from_config(config.behavior)
    mock.started_at = 0.0
    mock.alarm_states = {}
    mock.spike_states = {}
    mock.published = []
    mock.publish = lambda payload, port_id=None: mock.published.append((port_id, payload))
    return mock


class AddRemoveDeviceBehaviorTest(unittest.TestCase):
    def telemetry_point_keys(self, device, gateway):
        pack = protocol.telemetry_pack([device], context=gateway.config, port_id="port_001")
        return {point["pointKey"] for point in pack["devices"][0]["points"]}

    def test_address_1_and_6_regular_temperature_humidity_success(self):
        for slave_id in (1, 6):
            gateway = FakeGateway()
            AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(slave_id))
            device = gateway.state.get_device("port_001", slave_id)
            acks = [item for item in gateway.published if item["kind"] == "ack"]

            self.assertTrue(acks[-1]["ok"])
            self.assertIsNotNone(device)
            self.assertEqual({"temperature", "humidity"}, self.telemetry_point_keys(device, gateway))

    def test_address_2_device_no_response(self):
        gateway = FakeGateway()
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(2))

        acks = [item for item in gateway.published if item["kind"] == "ack"]
        self.assertFalse(acks[-1]["ok"])
        self.assertEqual(acks[-1]["reason"], "device_no_response")

    def test_add_duplicate_is_scoped_to_same_port(self):
        gateway = FakeGateway()
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(3, "port_001"))
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(3, "port_001"))
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_002", add_device_payload(3, "port_002"))

        acks = [item for item in gateway.published if item["kind"] == "ack"]
        self.assertTrue(acks[0]["ok"])
        self.assertFalse(acks[1]["ok"])
        self.assertEqual(acks[1]["reason"], "device_exists")
        self.assertTrue(acks[2]["ok"])
        self.assertIsNotNone(gateway.state.get_device("port_001", 3))
        self.assertIsNotNone(gateway.state.get_device("port_002", 3))

    def test_address_4_port_not_found_and_7_relay(self):
        gateway = FakeGateway()
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(4))
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(7, device_type="sensor_th"))

        acks = [item for item in gateway.published if item["kind"] == "ack"]
        self.assertEqual(acks[0]["reason"], "port_not_found")
        self.assertTrue(acks[1]["ok"])
        self.assertEqual(gateway.state.get_device("port_001", 7)["device_type"], "relay")

    def test_address_5_delete_delay_success(self):
        gateway = FakeGateway()
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(5))
        with patch("handlers.remove_device_handler.threading.Thread", ImmediateThread), \
             patch("handlers.remove_device_handler.time.sleep") as sleep_mock:
            RemoveDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", remove_device_payload(5))

        sleep_mock.assert_called_once_with(3.0)
        acks = [item for item in gateway.published if item["kind"] == "ack"]
        self.assertTrue(acks[-1]["ok"])
        self.assertEqual(acks[-1]["reason"], "ok_after_3s")
        self.assertIsNone(gateway.state.get_device("port_001", 5))

    def test_remove_missing_rule_for_address_2(self):
        gateway = FakeGateway()
        RemoveDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", remove_device_payload(2))

        acks = [item for item in gateway.published if item["kind"] == "ack"]
        self.assertFalse(acks[-1]["ok"])
        self.assertEqual(acks[-1]["reason"], "device_not_found")

    def test_address_8_add_delay_success(self):
        gateway = FakeGateway()
        with patch("handlers.add_device_handler.threading.Thread", ImmediateThread), \
             patch("handlers.add_device_handler.time.sleep") as sleep_mock:
            AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", add_device_payload(8))

        sleep_mock.assert_called_once_with(3.0)
        acks = [item for item in gateway.published if item["kind"] == "ack"]
        self.assertTrue(acks[-1]["ok"])
        self.assertEqual(acks[-1]["reason"], "ok_after_3s")
        self.assertIsNotNone(gateway.state.get_device("port_001", 8))


class TemperatureAlarmScenarioTest(unittest.TestCase):
    def add_device(self, slave_id, port_id="port_001", threshold_enabled=True, temp_high=30.0, temp_low=10.0):
        gateway = FakeGateway()
        AddDeviceHandler().handle(
            gateway,
            f"cmd/gateway_002/{port_id}",
            add_device_payload(
                slave_id,
                port_id,
                threshold_enabled=threshold_enabled,
                temp_high=temp_high,
                temp_low=temp_low,
            ),
        )
        return gateway

    def sample_temperature(self, mock, port_id, device, now):
        with patch("core.mock_gateway.time.time", return_value=now):
            return mock._telemetry_values(port_id, [device])[device["slave_id"]]["temperature"]

    def publish_alarm(self, mock, port_id, device, temperature):
        mock._publish_threshold_alarm_events(port_id, [device], {device["slave_id"]: {"temperature": temperature}})

    def test_slave_9_uses_real_port_and_threshold_then_recovers_once(self):
        gateway = self.add_device(9, "port_002", temp_high=40.0)
        device = gateway.state.get_device("port_002", 9)
        device["added_at"] = 100.0
        mock = fake_mock_gateway(gateway.config)

        normal = self.sample_temperature(mock, "port_002", device, 109.0)
        high = self.sample_temperature(mock, "port_002", device, 111.0)
        recovered = self.sample_temperature(mock, "port_002", device, 121.0)

        self.assertLess(normal, 40.0)
        self.assertGreater(high, 40.0)
        self.assertAlmostEqual(high, 42.0, delta=0.25)
        self.assertAlmostEqual(normal, 25.0, delta=0.25)
        self.assertAlmostEqual(recovered, 25.0, delta=0.25)
        self.assertLess(recovered, 40.0)

        self.publish_alarm(mock, "port_002", device, high)
        self.publish_alarm(mock, "port_002", device, high)
        self.publish_alarm(mock, "port_002", device, recovered)
        self.publish_alarm(mock, "port_002", device, recovered)

        events = [payload for _, payload in mock.published]
        self.assertEqual([event["state"] for event in events], ["active", "recovered"])
        self.assertEqual(events[0]["alarm_id"], "gateway_002.port_002.9.temperature.threshold_high")
        self.assertEqual(events[1]["alarm_id"], events[0]["alarm_id"])

    def test_slave_10_stays_hot_and_never_recovers(self):
        gateway = self.add_device(10, "port_001", temp_high=50.0)
        device = gateway.state.get_device("port_001", 10)
        device["added_at"] = 200.0
        mock = fake_mock_gateway(gateway.config)

        high = self.sample_temperature(mock, "port_001", device, 211.0)
        still_high = self.sample_temperature(mock, "port_001", device, 240.0)

        self.assertAlmostEqual(high, 52.0, delta=0.25)
        self.assertAlmostEqual(still_high, 52.0, delta=0.25)

        self.publish_alarm(mock, "port_001", device, high)
        self.publish_alarm(mock, "port_001", device, still_high)
        self.publish_alarm(mock, "port_001", device, 25.0)

        events = [payload for _, payload in mock.published]
        self.assertEqual([event["state"] for event in events], ["active"])
        self.assertEqual(events[0]["alarm_type"], "threshold_high")

    def test_slave_11_low_temperature_recovers_once(self):
        gateway = self.add_device(11, "port_001", temp_high=40.0, temp_low=15.0)
        device = gateway.state.get_device("port_001", 11)
        device["added_at"] = 500.0
        mock = fake_mock_gateway(gateway.config)

        normal = self.sample_temperature(mock, "port_001", device, 509.0)
        low = self.sample_temperature(mock, "port_001", device, 511.0)
        recovered = self.sample_temperature(mock, "port_001", device, 521.0)

        self.assertAlmostEqual(normal, 27.5, delta=0.25)
        self.assertAlmostEqual(low, 13.0, delta=0.25)
        self.assertAlmostEqual(recovered, 27.5, delta=0.25)

        self.publish_alarm(mock, "port_001", device, low)
        self.publish_alarm(mock, "port_001", device, low)
        self.publish_alarm(mock, "port_001", device, recovered)
        self.publish_alarm(mock, "port_001", device, recovered)

        events = [payload for _, payload in mock.published]
        self.assertEqual([event["state"] for event in events], ["active", "recovered"])
        self.assertEqual(events[0]["alarm_type"], "threshold_low")
        self.assertEqual(events[0]["alarm_id"], "gateway_002.port_001.11.temperature.threshold_low")
        self.assertEqual(events[1]["alarm_id"], events[0]["alarm_id"])

    def test_slave_12_low_temperature_never_recovers(self):
        gateway = self.add_device(12, "port_001", temp_high=40.0, temp_low=15.0)
        device = gateway.state.get_device("port_001", 12)
        device["added_at"] = 600.0
        mock = fake_mock_gateway(gateway.config)

        low = self.sample_temperature(mock, "port_001", device, 611.0)
        still_low = self.sample_temperature(mock, "port_001", device, 640.0)

        self.assertAlmostEqual(low, 13.0, delta=0.25)
        self.assertAlmostEqual(still_low, 13.0, delta=0.25)

        self.publish_alarm(mock, "port_001", device, low)
        self.publish_alarm(mock, "port_001", device, still_low)
        self.publish_alarm(mock, "port_001", device, 27.5)

        events = [payload for _, payload in mock.published]
        self.assertEqual([event["state"] for event in events], ["active"])
        self.assertEqual(events[0]["alarm_type"], "threshold_low")

    def test_slave_13_high_then_low_both_recover(self):
        gateway = self.add_device(13, "port_001", temp_high=40.0, temp_low=15.0)
        device = gateway.state.get_device("port_001", 13)
        device["added_at"] = 700.0
        mock = fake_mock_gateway(gateway.config)

        normal_a = self.sample_temperature(mock, "port_001", device, 709.0)
        high = self.sample_temperature(mock, "port_001", device, 711.0)
        normal_b = self.sample_temperature(mock, "port_001", device, 721.0)
        low = self.sample_temperature(mock, "port_001", device, 731.0)
        normal_c = self.sample_temperature(mock, "port_001", device, 741.0)

        self.assertAlmostEqual(normal_a, 27.5, delta=0.25)
        self.assertAlmostEqual(high, 42.0, delta=0.25)
        self.assertAlmostEqual(normal_b, 27.5, delta=0.25)
        self.assertAlmostEqual(low, 13.0, delta=0.25)
        self.assertAlmostEqual(normal_c, 27.5, delta=0.25)

        for value in [normal_a, high, normal_b, low, normal_c]:
            self.publish_alarm(mock, "port_001", device, value)

        events = [payload for _, payload in mock.published]
        self.assertEqual(
            [(event["alarm_type"], event["state"]) for event in events],
            [
                ("threshold_high", "active"),
                ("threshold_high", "recovered"),
                ("threshold_low", "active"),
                ("threshold_low", "recovered"),
            ],
        )

    def test_disabled_threshold_sends_normal_telemetry_without_alarm(self):
        gateway = self.add_device(9, "port_001", threshold_enabled=False, temp_high=30.0)
        device = gateway.state.get_device("port_001", 9)
        device["added_at"] = 300.0
        mock = fake_mock_gateway(gateway.config)

        temperature = self.sample_temperature(mock, "port_001", device, 320.0)
        self.assertLess(temperature, 30.0)

        self.publish_alarm(mock, "port_001", device, 32.0)
        self.assertEqual(mock.published, [])

    def test_threshold_values_drive_high_and_normal_temperature(self):
        for threshold in [30.0, 40.0, 50.0]:
            gateway = self.add_device(9, "port_001", temp_high=threshold)
            device = gateway.state.get_device("port_001", 9)
            device["added_at"] = 400.0
            mock = fake_mock_gateway(gateway.config)

            normal = self.sample_temperature(mock, "port_001", device, 409.0)
            high = self.sample_temperature(mock, "port_001", device, 411.0)
            self.assertAlmostEqual(normal, (10.0 + threshold) / 2.0, delta=0.25)
            self.assertAlmostEqual(high, threshold + 2.0, delta=0.25)

    def test_relay_telemetry_uses_configured_channel_count(self):
        gateway = FakeGateway()
        payload = add_device_payload(7, "port_001", device_type="relay")
        payload["device"]["device_options"] = {"relay_channel_count": 4}
        AddDeviceHandler().handle(gateway, "cmd/gateway_002/port_001", payload)
        device = gateway.state.get_device("port_001", 7)

        pack = protocol.telemetry_pack([device], context=gateway.config, port_id="port_001")
        relay_device = pack["devices"][0]
        self.assertEqual(relay_device["device_type"], "relay")
        self.assertEqual(relay_device["channelCount"], 4)
        self.assertEqual(len(relay_device["points"]), 4)


if __name__ == "__main__":
    unittest.main()
