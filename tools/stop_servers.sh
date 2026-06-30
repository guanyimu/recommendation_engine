#!/usr/bin/env bash
# 通过名字服务查实例 pid 并停止；再停 nginx / registry。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export RE_ENGINE_ROOT="$ROOT"

PID_DIR="$ROOT/logs/.pids"
NGINX_PREFIX="$ROOT/logs/nginx"
CTL="$ROOT/bazel-bin/tools/registry_ctl"

if [[ ! -x "$CTL" ]]; then
  echo "[registry] 编译 registry_ctl..."
  bazel build //tools:registry_ctl
fi

stop_nginx() {
  if [[ -f "$NGINX_PREFIX/nginx.pid" ]]; then
    nginx -p "$NGINX_PREFIX/" -s quit 2>/dev/null || true
    for _ in $(seq 1 25); do
      if [[ ! -f "$NGINX_PREFIX/nginx.pid" ]]; then
        break
      fi
      sleep 0.2
    done
    rm -f "$NGINX_PREFIX/nginx.pid" "$PID_DIR/nginx.pid"
    echo "[nginx] 已停止"
  fi
}

if [[ -x "$CTL" ]]; then
  "$CTL" stop-all || true
else
  echo "[registry] registry_ctl 未编译，跳过名字服务 stop-all"
fi

stop_nginx

if [[ -f "$PID_DIR/registry.pid" ]]; then
  pid="$(cat "$PID_DIR/registry.pid")"
  if kill -0 "$pid" 2>/dev/null; then
    kill -TERM "$pid" 2>/dev/null || true
    sleep 0.5
    kill -0 "$pid" 2>/dev/null && kill -9 "$pid" 2>/dev/null || true
    echo "[registry] 已停止 pid=$pid"
  fi
  rm -f "$PID_DIR/registry.pid"
fi

rm -f "$PID_DIR"/*.pid 2>/dev/null || true
echo "全部服务已停止。"
