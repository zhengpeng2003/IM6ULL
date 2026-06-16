# main.py
# -*- coding: utf-8 -*-

import signal
import sys

from core.mock_gateway import MockGateway


def main():
    gateway = MockGateway()

    def handle_exit(sig, frame):
        print("\n[EXIT] stopping mock linux_data...")
        gateway.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    gateway.start()


if __name__ == "__main__":
    main()