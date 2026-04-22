#!/usr/bin/env bash
# Restart the HTTP server for gui_designer on port 8080.
# Usage: ./serve.sh [port]

PORT=${1:-8080}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Kill any existing instance on the same port
OLD=$(lsof -ti tcp:"$PORT" 2>/dev/null)
if [ -n "$OLD" ]; then
    echo "Stopping existing server(s) on port $PORT: PID $OLD"
    kill "$OLD" 2>/dev/null
    sleep 0.5
fi

cd "$SCRIPT_DIR"
echo "Starting HTTP server at http://$(hostname -I | awk '{print $1}'):$PORT"
python3 -m http.server "$PORT" --bind 0.0.0.0
