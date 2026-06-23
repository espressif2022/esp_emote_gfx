#!/usr/bin/env bash
# Fail if src/ includes ESP-IDF headers outside platform/esp_idf/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

violations="$(rg -n '#include\s+[<"](esp_[^">]+)[">]' src/ \
    --glob '!src/platform/esp_idf/**' \
    --glob '!src/backend/esp_lcd/**' || true)"

if [[ -n "$violations" ]]; then
    echo "ERROR: ESP-IDF headers found in src/ outside platform/esp_idf/:" >&2
    echo "$violations" >&2
    echo >&2
    echo "Use gfx_err.h, common/gfx_check.h, and gfx_log_priv.h instead." >&2
    exit 1
fi

echo "OK: no ESP-IDF includes in src/ outside platform/esp_idf/"
