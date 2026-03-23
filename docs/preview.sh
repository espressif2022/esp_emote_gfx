#!/usr/bin/env bash
# ESP Emote GFX 文档本地预览脚本
# 一键构建并预览文档

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${1:-8090}"
HOST="${HOST:-127.0.0.1}"

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

# 统一构建文档
echo "[2/4] 构建文档站点..."
bash docs/scripts/build_docs.sh >/dev/null
echo "  ✓ 文档构建完成"

# 启动本地服务器
echo "[3/4] 启动本地预览服务器..."
echo ""
echo "=========================================="
echo "  文档预览地址："
echo ""
echo "    http://$HOST:$PORT"
echo ""
echo "  主要页面："
echo "    - 主页:       http://$HOST:$PORT/index.html"
echo "    - Core API:   http://$HOST:$PORT/api/core/index.html"
echo "    - Widget API: http://$HOST:$PORT/api/widgets/index.html"
echo "    - Doxygen:    http://$HOST:$PORT/doxygen/index.html"
echo ""
echo "  按 Ctrl+C 停止服务器"
echo "=========================================="
echo ""

cd docs/_build/html
python3 -m http.server "$PORT" --bind "$HOST"
