#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GSP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${GSP_DIR}/../.." && pwd)"
PORT_DIR="${SCRIPT_DIR}/port"

emcc \
    "${PORT_DIR}/gsp_wasm_api.c" \
    "${GSP_DIR}/gsp_load.c" \
    "${PORT_DIR}/gsp_wasm_platform.c" \
    "${ROOT_DIR}/src/core/fs/gfx_fs.c" \
    "${ROOT_DIR}/src/core/fs/gfx_fs_file.c" \
    "${ROOT_DIR}/src/core/types/gfx_color.c" \
    "${ROOT_DIR}/src/core/log/gfx_log.c" \
    "${ROOT_DIR}/src/core/display/gfx_backend.c" \
    "${ROOT_DIR}/src/core/display/gfx_display.c" \
    "${ROOT_DIR}/src/core/display/gfx_refresh.c" \
    "${ROOT_DIR}/src/core/object/gfx_object.c" \
    "${ROOT_DIR}/src/core/object/gfx_widget_class.c" \
    "${ROOT_DIR}/src/core/runtime/gfx_core.c" \
    "${ROOT_DIR}/src/core/runtime/gfx_timer.c" \
    "${ROOT_DIR}/src/core/runtime/gfx_touch.c" \
    "${ROOT_DIR}/src/core/tween/gfx_tween.c" \
    "${ROOT_DIR}/src/backend/memory/memory_backend.c" \
    "${ROOT_DIR}/src/codecs/image/gfx_image_decoder.c" \
    "${ROOT_DIR}/src/fonts/gfx_font_adapter.c" \
    "${ROOT_DIR}/src/fonts/gfx_font_lvgl.c" \
    "${ROOT_DIR}/src/render/gfx_render.c" \
    "${ROOT_DIR}/src/render/sw/gfx_blend.c" \
    "${ROOT_DIR}/src/render/sw/gfx_sw_draw.c" \
    "${ROOT_DIR}/src/widgets/basic/gfx_button.c" \
    "${ROOT_DIR}/src/widgets/basic/gfx_container.c" \
    "${ROOT_DIR}/src/widgets/basic/gfx_list.c" \
    "${ROOT_DIR}/src/widgets/basic/gfx_wheel.c" \
    "${ROOT_DIR}/src/widgets/img/gfx_img.c" \
    "${ROOT_DIR}/src/widgets/img/gfx_image_resource.c" \
    "${ROOT_DIR}/src/widgets/label/gfx_label.c" \
    "${ROOT_DIR}/src/widgets/label/gfx_label_draw.c" \
    "${ROOT_DIR}/src/widgets/label/gfx_label_obj.c" \
    "${ROOT_DIR}/src/platform/host/host_accel_stub.c" \
    "${ROOT_DIR}/src/platform/host/host_font.c" \
    "${ROOT_DIR}/src/platform/host/host_jpeg_stub.c" \
    "${ROOT_DIR}/src/platform/host/host_subsystem_stub.c" \
    "${ROOT_DIR}/src/platform/host/host_touch_stub.c" \
    -I"${GSP_DIR}" \
    -I"${PORT_DIR}" \
    -I"${ROOT_DIR}/include" \
    -I"${ROOT_DIR}/src" \
    -I"${ROOT_DIR}/simulation/port/include" \
    -std=c11 \
    -O2 \
    -DGFX_HOST_BUILD=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=createGspWasm \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["HEAPU8","UTF8ToString"]' \
    -s EXPORTED_FUNCTIONS='[
        "_gsp_wasm_malloc",
        "_gsp_wasm_free",
        "_gsp_wasm_validate",
        "_gsp_wasm_get_width",
        "_gsp_wasm_get_height",
        "_gsp_wasm_render_into",
        "_gsp_wasm_runtime_create",
        "_gsp_wasm_runtime_destroy",
        "_gsp_wasm_runtime_render_into",
        "_gsp_wasm_runtime_hit_test",
        "_gsp_wasm_runtime_pointer_down",
        "_gsp_wasm_runtime_pointer_move",
        "_gsp_wasm_runtime_pointer_up",
        "_gsp_wasm_runtime_click",
        "_gsp_wasm_runtime_last_call",
        "_gsp_wasm_runtime_clear_last_call"
    ]' \
    -o "${SCRIPT_DIR}/gsp_wasm.js"

echo "Generated ${SCRIPT_DIR}/gsp_wasm.js and ${SCRIPT_DIR}/gsp_wasm.wasm"
