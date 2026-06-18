#!/usr/bin/env bash
# 编译并后台启动 ranking / video / browse
# 停止: ./tools/stop_servers.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export RE_ENGINE_ROOT="$ROOT"

PID_DIR="$ROOT/logs/.pids"
mkdir -p "$PID_DIR" "$ROOT/logs/ranking" "$ROOT/logs/video" "$ROOT/logs/browse"

start_server() {
  local name=$1 binary=$2
  local pid_file="$PID_DIR/$name.pid"

  if [[ -f "$pid_file" ]]; then
    local old_pid
    old_pid="$(cat "$pid_file")"
    if kill -0 "$old_pid" 2>/dev/null; then
      echo "[$name] 已在运行 pid=$old_pid"
      return 0
    fi
    rm -f "$pid_file"
  fi

  nohup "$binary" >>"$ROOT/logs/$name/stdout.log" 2>&1 &
  echo $! >"$pid_file"
  echo "[$name] 已启动 pid=$(cat "$pid_file") 日志 logs/$name/stdout.log"
}

echo "[build] 编译三个服务..."
bazel build \
  //src/servers/ranking:ranking_main \
  //src/servers/video:video_main \
  //src/servers/browse:browse_main

BIN="$ROOT/bazel-bin"
start_server ranking "$BIN/src/servers/ranking/ranking_main"
start_server video "$BIN/src/servers/video/video_main"
start_server browse "$BIN/src/servers/browse/browse_main"
