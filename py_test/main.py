# main.py
# -*- coding: utf-8 -*-

import argparse
import signal
import sys

from core.mock_gateway_fleet import MockGatewayFleet


def parse_args():
    parser = argparse.ArgumentParser(description="Mock Linux_data MQTT gateway fleet")
    parser.add_argument(
        "--scenario",
        default="",
        help="Path to scenario JSON. Relative paths are resolved from the current working directory first.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    fleet = MockGatewayFleet.from_scenario(args.scenario) if args.scenario else MockGatewayFleet.from_legacy_default()

    def handle_exit(sig, frame):
        _ = sig
        _ = frame
        print("\n[EXIT] stopping mock linux_data fleet...")
        fleet.stop_all()
        sys.exit(0)

    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    fleet.loop_forever()


if __name__ == "__main__":
    main()
