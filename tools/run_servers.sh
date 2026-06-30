#!/usr/bin/env bash
# 启动名字服务 + gRPC 实例（自动端口并注册）+ Nginx LB（配置从名字服务生成）。
# 单实例无 LB: USE_LB=0 ./tools/run_servers.sh
# 仅名字服务: ./tools/run_registry.sh
# 停止: ./tools/stop_servers.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export RE_ENGINE_ROOT="$ROOT"

USE_LB="${USE_LB:-1}"
PID_DIR="$ROOT/logs/.pids"
NGINX_PREFIX="$ROOT/logs/nginx"
NGINX_CONF="$NGINX_PREFIX/stream_lb.conf"
REGISTRY_PORT="${REGISTRY_PORT:-23790}"
mkdir -p "$PID_DIR" "$NGINX_PREFIX/logs"

start_bg() {
  local name=$1 binary=$2
  local pid_file="$PID_DIR/$name.pid"
  local log_dir="$ROOT/logs/$name"
  mkdir -p "$log_dir"

  if [[ -f "$pid_file" ]] && kill -0 "$(cat "$pid_file")" 2>/dev/null; then
    echo "[$name] 已在运行 pid=$(cat "$pid_file")"
    return 0
  fi
  rm -f "$pid_file"

  env RE_INSTANCE_NAME="$name" nohup "$binary" >>"$log_dir/stdout.log" 2>&1 &
  echo $! >"$pid_file"
  echo "[$name] 已启动 pid=$(cat "$pid_file")"
}

start_registry() {
  local binary=$1
  local pid_file="$PID_DIR/registry.pid"
  local log_dir="$ROOT/logs/registry"
  mkdir -p "$log_dir"

  if [[ -f "$pid_file" ]] && kill -0 "$(cat "$pid_file")" 2>/dev/null; then
    echo "[registry] 已在运行 pid=$(cat "$pid_file")"
    return 0
  fi
  rm -f "$pid_file"

  echo "[registry] 启动 $binary"
  nohup "$binary" >>"$log_dir/stdout.log" 2>&1 &
  echo $! >"$pid_file"
  echo "[registry] 已启动 pid=$(cat "$pid_file")"
}

wait_registry() {
  local ctl=$1
  local port=$2
  local i

  echo "[registry] 等待名字服务就绪 (127.0.0.1:${port})..."
  for i in $(seq 1 50); do
    if "$ctl" ping >/dev/null 2>&1; then
      echo "[registry] 名字服务就绪"
      return 0
    fi
    if command -v ss >/dev/null 2>&1; then
      if ss -tln | grep -q ":${port} "; then
        sleep 0.2
        if "$ctl" ping >/dev/null 2>&1; then
          echo "[registry] 名字服务就绪"
          return 0
        fi
      fi
    fi
    sleep 0.2
  done
  return 1
}

echo "[build] 编译..."
bazel build \
  //src/registry:registry_main \
  //tools:registry_ctl \
  //src/servers/ranking:ranking_main \
  //src/servers/video:video_main \
  //src/servers/browse:browse_main

BIN="$ROOT/bazel-bin"
CTL="$BIN/tools/registry_ctl"
REGISTRY_BIN="$BIN/src/registry/registry_main"

if [[ ! -x "$REGISTRY_BIN" ]]; then
  echo "[registry] 未找到可执行文件: $REGISTRY_BIN" >&2
  exit 1
fi
if [[ ! -x "$CTL" ]]; then
  echo "[registry] 未找到 registry_ctl: $CTL" >&2
  exit 1
fi

start_registry "$REGISTRY_BIN"
if ! wait_registry "$CTL" "$REGISTRY_PORT"; then
  echo "[registry] 名字服务未就绪" >&2
  if [[ -f "$ROOT/logs/registry/stdout.log" ]]; then
    echo "[registry] 最近日志:" >&2
    tail -20 "$ROOT/logs/registry/stdout.log" >&2 || true
  fi
  exit 1
fi

if [[ "$USE_LB" == "1" ]]; then
  start_bg ranking-1 "$BIN/src/servers/ranking/ranking_main"
  start_bg ranking-2 "$BIN/src/servers/ranking/ranking_main"
  start_bg video-1 "$BIN/src/servers/video/video_main"
  start_bg video-2 "$BIN/src/servers/video/video_main"
  start_bg browse-1 "$BIN/src/servers/browse/browse_main"
  start_bg browse-2 "$BIN/src/servers/browse/browse_main"

  echo "[registry] 等待实例注册..."
  sleep 2

  "$CTL" setup-lb "$NGINX_CONF"

  if ! command -v nginx >/dev/null; then
    echo "[nginx] 未找到 nginx，请: sudo apt install nginx-extras" >&2
    exit 1
  fi
  mkdir -p "$NGINX_PREFIX/logs"
  nginx -p "$NGINX_PREFIX/" -c "$NGINX_CONF"
  if [[ ! -f "$NGINX_PREFIX/nginx.pid" ]]; then
    echo "[nginx] 启动失败" >&2
    exit 1
  fi
  cp "$NGINX_PREFIX/nginx.pid" "$PID_DIR/nginx.pid"
  echo "[nginx] LB 已启动（配置见 $NGINX_CONF）"
else
  start_bg ranking "$BIN/src/servers/ranking/ranking_main"
  start_bg video "$BIN/src/servers/video/video_main"
  start_bg browse "$BIN/src/servers/browse/browse_main"
  echo "单实例模式（无 Nginx；客户端从名字服务解析实例）"
fi

echo "  停止: ./tools/stop_servers.sh"
