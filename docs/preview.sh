#!/usr/bin/env bash
# ESP Emote GFX 文档本地预览脚本
# 一键构建并预览文档

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${1:-8090}"

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

# 构建 Sphinx 文档
echo "[2/4] 构建 Sphinx 文档..."
make -C docs html 2>&1 | grep -E "(warning|error|Writing|building)" || true
echo "  ✓ Sphinx 构建完成"

# 运行后处理脚本（Doxygen）
echo "[3/4] 生成 Doxygen API 文档..."
if command -v doxygen >/dev/null 2>&1; then
    bash docs/scripts/postprocess_docs.sh >/dev/null 2>&1
    echo "  ✓ Doxygen 文档生成完成"
else
    echo "  ⚠ Doxygen 未安装，跳过 C/C++ API 文档"
    echo "    安装方式: sudo apt-get install doxygen graphviz"
fi

# 启动本地服务器
echo "[4/4] 启动本地预览服务器..."
echo ""
echo "=========================================="
echo "  文档预览地址："
echo ""
echo "    http://10.18.20.57:$PORT"
echo ""
echo "  主要页面："
echo "    - 主页:       http://10.18.20.57:$PORT/index.html"
echo "    - Core API:   http://10.18.20.57:$PORT/api/core/index.html"
echo "    - Widget API: http://10.18.20.57:$PORT/api/widgets/index.html"
echo "    - Doxygen:    http://10.18.20.57:$PORT/doxygen/index.html"
echo ""
echo "  按 Ctrl+C 停止服务器"
echo "=========================================="
echo ""

cd docs/_build/html
python3 -m http.server "$PORT" --bind 10.18.20.57

