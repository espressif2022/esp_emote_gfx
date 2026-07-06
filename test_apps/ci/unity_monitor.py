#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
# Monitor ESP serial for Unity test summary; exit 0 on all pass, 1 on fail/timeout.
#
# Usage: unity_monitor.py --port /dev/ttyACM0 [--timeout 120]

from __future__ import annotations

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    print('ERROR: pip install pyserial', file=sys.stderr)
    sys.exit(2)


def main() -> int:
    parser = argparse.ArgumentParser(description='Unity serial monitor for gfx_test_unit')
    parser.add_argument('--port', required=True, help='Serial port, e.g. /dev/ttyACM0')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--timeout', type=int, default=120, help='Max seconds to wait')
    args = parser.parse_args()

    fail_re = re.compile(r'\bFAIL\s*:', re.IGNORECASE)
    ok_re = re.compile(r'(\d+)\s+Tests\s+(\d+)\s+Failures', re.IGNORECASE)

    saw_tests = False
    failures = None
    deadline = time.monotonic() + args.timeout
    buf = ''

    with serial.Serial(args.port, args.baud, timeout=0.5) as set:
        # Reset device via DTR (common on USB-JTAG)
        set.dtr = False
        set.rts = True
        time.sleep(0.1)
        set.rts = False
        time.sleep(0.3)

        while time.monotonic() < deadline:
            chunk = set.read(4096)
            if chunk:
                text = chunk.decode('utf-8', errors='replace')
                sys.stdout.write(text)
                sys.stdout.flush()
                buf += text
                if len(buf) > 65536:
                    buf = buf[-32768:]

                if fail_re.search(text):
                    print('\n[unity_monitor] FAIL detected in output', file=sys.stderr)
                    return 1

                m = ok_re.search(buf)
                if m:
                    saw_tests = True
                    failures = int(m.group(2))
                    if failures == 0:
                        print(f'\n[unity_monitor] All {m.group(1)} tests passed', file=sys.stderr)
                        return 0
                    print(f'\n[unity_monitor] {failures} failure(s)', file=sys.stderr)
                    return 1

            elif saw_tests:
                break

    print(f'\n[unity_monitor] Timeout after {args.timeout}s (no Unity summary)', file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
