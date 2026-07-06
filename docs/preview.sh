#!/usr/bin/env bash
# ESP Emote GFX 文档本地预览脚本
# 一键构建并预览文档

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${1:-8090}"
# Bind all interfaces by default so LAN URLs (e.g. http://10.x.x.x:PORT/) work; use 127.0.0.1 for local-only.
BIND_ADDR="${DOCS_PREVIEW_BIND:-0.0.0.0}"

cd "$REPO_ROOT"

echo "=========================================="
echo "  ESP Emote GFX 文档本地预览"
echo "=========================================="
echo ""

# 关闭已存在的 http.server 进程
echo "[0/4] 检查并关闭已有服务..."
OLD_PIDS=$(ps -ef | grep "python.*http.server.*$PORT" | grep -v grep | awk '{print $2}')
if [ -n "$OLD_PIDS" ]; then
    echo "  关闭端口 $PORT 上的旧进程: $OLD_PIDS"
    echo "$OLD_PIDS" | xargs kill -9 2>/dev/null || true
    sleep 1
else
    echo "  ✓ 无旧进程"
fi

# 检查并安装依赖
echo "[1/4] 检查依赖..."
if ! python3 -c "import sphinx" 2>/dev/null; then
    echo "  安装 Sphinx 依赖..."
    pip install -r docs/requirements.txt -q
else
    echo "  ✓ Sphinx 已安装"
fi

# 自动生成 API RST + 构建 Sphinx + 后处理 Doxygen
echo "[2/4] 自动生成并构建文档..."
if command -v doxygen >/dev/null 2>&1; then
    bash docs/scripts/postprocess_docs.sh
    echo "  ✓ API 文档、Sphinx、Doxygen 全部完成"
else
    bash docs/scripts/postprocess_docs.sh --skip-doxygen
    echo "  ✓ API 文档和 Sphinx 构建完成"
    echo "  ⚠ Doxygen 未安装，跳过 C/C++ API 文档"
    echo "    安装方式: sudo apt-get install doxygen graphviz"
fi

# 启动本地服务器
echo "[3/4] 启动本地预览服务器..."
echo ""
echo "=========================================="
echo "  文档预览地址："
echo ""
echo "    http://127.0.0.1:$PORT  (same host)"
echo "    http://<this-machine-LAN-ip>:$PORT  (other devices; server binds $BIND_ADDR)"
echo ""
echo "  主要页面（EN / 中文 分目录；顶部可切换语言）："
echo "    - 语言选择:   http://127.0.0.1:$PORT/index.html"
echo "    - English:    http://127.0.0.1:$PORT/en/index.html"
echo "    - 中文:       http://127.0.0.1:$PORT/zh_CN/index.html"
echo "    - Core API:   http://127.0.0.1:$PORT/en/api/core/index.html"
echo "    - Widget API: http://127.0.0.1:$PORT/en/api/widgets/index.html"
echo "    - Doxygen:    http://127.0.0.1:$PORT/doxygen/index.html"
echo ""
echo "  按 Ctrl+C 停止服务器"
echo "=========================================="
echo ""

cd docs/_build/html
python3 -m http.server "$PORT" --bind "$BIND_ADDR"

