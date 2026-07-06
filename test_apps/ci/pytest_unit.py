# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
# Optional: pytest-embedded runner for gfx_test_unit (requires connected board).
#
#   pip install pytest pytest-embedded pytest-embedded-idf pytest-embedded-serial-esp
#   pytest test_apps/ci/pytest_unit.py --target esp32s3 -s
#
# Set serial port:
#   export PYTEST_EMBEDDED_SERIAL=/dev/ttyACM0

import re

import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
def test_gfx_unit_all(dut: Dut) -> None:
    dut.expect(r'Running', timeout=30)
    dut.expect(re.compile(r'(\d+) Tests (\d+) Failures'), timeout=120)
    summary = dut.match.group(0).decode(errors='replace')
    m = re.search(r'(\d+) Tests (\d+) Failures', summary)
    assert m is not None, f'Unexpected summary: {summary!r}'
    failures = int(m.group(2))
    assert failures == 0, f'Unity reported {failures} failure(s)'
