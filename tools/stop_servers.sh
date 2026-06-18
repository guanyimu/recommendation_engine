#!/usr/bin/env bash
# 停止 run_servers.sh 启动的后台服务。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PID_DIR="$ROOT/logs/.pids"

stop_one() {
  local name=$1
  local pid_file="$PID_DIR/$name.pid"
  if [[ ! -f "$pid_file" ]]; then
    echo "[$name] 未运行"
    return 0
  fi
  local pid
  pid="$(cat "$pid_file")"
  if kill -0 "$pid" 2>/dev/null; then
    kill -INT "$pid" 2>/dev/null || true
    for _ in $(seq 1 20); do
      if ! kill -0 "$pid" 2>/dev/null; then
        break
      fi
      sleep 0.2
    done
    if kill -0 "$pid" 2>/dev/null; then
      kill -9 "$pid" 2>/dev/null || true
    fi
    echo "[$name] 已停止 pid=$pid"
  else
    echo "[$name] pid=$pid 已不存在"
  fi
  rm -f "$pid_file"
}

# 先停 browse，再停下游依赖
stop_one browse
stop_one video
stop_one ranking

echo "全部服务已停止。"
