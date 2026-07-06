#!/usr/bin/env bash
# Run gfx_test_unit on a connected ESP32-S3 board and fail if Unity reports failures.
#
# Usage:
#   ESP_PORT=/dev/ttyACM0 ./test_apps/ci/run_unit_on_target.sh
#
# Requires: IDF_PATH, idf.py, Python 3, board connected.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT_DIR="$(cd "${SCRIPT_DIR}/../unit" && pwd)"
ESP_PORT="${ESP_PORT:-/dev/ttyACM0}"
TIMEOUT_SEC="${TIMEOUT_SEC:-120}"

if [[ -z "${IDF_PATH:-}" ]]; then
    echo "ERROR: IDF_PATH is not set. Run: . \$IDF_PATH/export.sh" >&2
    exit 1
fi

# shellcheck disable=SC1090
. "${IDF_PATH}/export.sh"

echo "==> Building and flashing gfx_test_unit (port=${ESP_PORT})"
cd "${UNIT_DIR}"
idf.py -p "${ESP_PORT}" build flash

echo "==> Monitoring Unity output (timeout=${TIMEOUT_SEC}s)"
python3 "${SCRIPT_DIR}/unity_monitor.py" \
    --port "${ESP_PORT}" \
    --timeout "${TIMEOUT_SEC}"
