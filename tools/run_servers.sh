#!/usr/bin/env bash
# 编译并后台启动服务；默认 Nginx gRPC 负载均衡（每服务 2 实例）。
# 单实例无 LB: USE_LB=0 ./tools/run_servers.sh
# 停止: ./tools/stop_servers.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export RE_ENGINE_ROOT="$ROOT"

USE_LB="${USE_LB:-1}"
PID_DIR="$ROOT/logs/.pids"
NGINX_PREFIX="$ROOT/logs/nginx"
mkdir -p "$PID_DIR" "$NGINX_PREFIX/logs"

start_server() {
  local name=$1 binary=$2 listen="${3:-}"
  local pid_file="$PID_DIR/$name.pid"
  local log_dir="$ROOT/logs/$name"

  mkdir -p "$log_dir"

  if [[ -f "$pid_file" ]]; then
    local old_pid
    old_pid="$(cat "$pid_file")"
    if kill -0 "$old_pid" 2>/dev/null; then
      echo "[$name] 已在运行 pid=$old_pid"
      return 0
    fi
    rm -f "$pid_file"
  fi

  if [[ -n "$listen" ]]; then
    env RE_LISTEN_ADDRESS="$listen" RE_INSTANCE_NAME="$name" \
      nohup "$binary" >>"$log_dir/stdout.log" 2>&1 &
  else
    env RE_INSTANCE_NAME="$name" nohup "$binary" >>"$log_dir/stdout.log" 2>&1 &
  fi
  echo $! >"$pid_file"
  echo "[$name] 已启动 pid=$(cat "$pid_file")${listen:+ bind=$listen}"
}

start_nginx() {
  local pid_file="$PID_DIR/nginx.pid"
  local nginx_conf=""
  if [[ -f "$pid_file" ]] && kill -0 "$(cat "$pid_file")" 2>/dev/null; then
    echo "[nginx] 已在运行 pid=$(cat "$pid_file")"
    return 0
  fi
  if ! command -v nginx >/dev/null; then
    echo "[nginx] 未找到 nginx，请安装: sudo apt install nginx-extras" >&2
    exit 1
  fi
  if nginx -V 2>&1 | grep -q grpc; then
    nginx_conf="$ROOT/configs/nginx/grpc_lb.conf"
    echo "[nginx] 使用 http grpc_pass 配置"
  elif [[ -f /usr/lib/nginx/modules/ngx_stream_module.so ]]; then
    nginx_conf="$ROOT/configs/nginx/stream_lb.conf"
    echo "[nginx] 无 grpc 模块，使用 stream(TCP) 四层转发（gRPC 可用）"
  else
    echo "[nginx] 无 grpc/stream 模块，请: sudo apt install nginx-extras" >&2
    echo "  或: USE_LB=0 ./tools/run_servers.sh" >&2
    exit 1
  fi
  nginx -p "$NGINX_PREFIX/" -c "$nginx_conf"
  if [[ ! -f "$NGINX_PREFIX/nginx.pid" ]]; then
    echo "[nginx] 启动失败（无 pid 文件），端口可能被占用: ./tools/stop_servers.sh" >&2
    exit 1
  fi
  cp "$NGINX_PREFIX/nginx.pid" "$pid_file"
  echo "[nginx] LB 已监听 50052/50053/50054"
}

echo "[build] 编译三个服务..."
bazel build \
  //src/servers/ranking:ranking_main \
  //src/servers/video:video_main \
  //src/servers/browse:browse_main

BIN="$ROOT/bazel-bin"

if [[ "$USE_LB" == "1" ]]; then
  start_server ranking-1 "$BIN/src/servers/ranking/ranking_main" "127.0.0.1:15052"
  start_server ranking-2 "$BIN/src/servers/ranking/ranking_main" "127.0.0.1:15062"
  start_server video-1 "$BIN/src/servers/video/video_main" "127.0.0.1:15053"
  start_server video-2 "$BIN/src/servers/video/video_main" "127.0.0.1:15063"
  start_server browse-1 "$BIN/src/servers/browse/browse_main" "127.0.0.1:15054"
  start_server browse-2 "$BIN/src/servers/browse/browse_main" "127.0.0.1:15064"
  start_nginx
  echo
  echo "Nginx LB 已启用（client 连 conf 中 50052/50053/50054）。"
else
  start_server ranking "$BIN/src/servers/ranking/ranking_main"
  start_server video "$BIN/src/servers/video/video_main"
  start_server browse "$BIN/src/servers/browse/browse_main"
  echo
  echo "单实例模式（无 Nginx）。"
fi

echo "  停止: ./tools/stop_servers.sh"
