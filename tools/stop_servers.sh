#!/usr/bin/env bash
# 停止 run_servers.sh 启动的后台服务（含 Nginx LB）。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PID_DIR="$ROOT/logs/.pids"
NGINX_PREFIX="$ROOT/logs/nginx"

# LB 端口 + 多实例后端端口（USE_LB=1 时）
OUR_PORTS=(50052 50053 50054 15052 15062 15053 15063 15054 15064)

wait_pid_exit() {
  local pid=$1
  local label=${2:-pid}
  for _ in $(seq 1 25); do
    if ! kill -0 "$pid" 2>/dev/null; then
      return 0
    fi
    sleep 0.2
  done
  if kill -0 "$pid" 2>/dev/null; then
    kill -9 "$pid" 2>/dev/null || true
    sleep 0.2
    echo "[$label] 强制结束 pid=$pid"
  fi
}

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
    wait_pid_exit "$pid" "$name"
    echo "[$name] 已停止 pid=$pid"
  else
    echo "[$name] pid=$pid 已不存在"
  fi
  rm -f "$pid_file"
}

# 按 pid 发 SIGQUIT（nginx 优雅退出），失败则 TERM / KILL
stop_pid_graceful() {
  local pid=$1
  local label=${2:-nginx}
  if ! kill -0 "$pid" 2>/dev/null; then
    return 0
  fi
  kill -QUIT "$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
  wait_pid_exit "$pid" "$label"
}

stop_nginx() {
  local stopped=0

  # 1) 项目 prefix 下的 pid 文件
  for pid_file in "$NGINX_PREFIX/nginx.pid" "$PID_DIR/nginx.pid"; do
    if [[ -f "$pid_file" ]]; then
      local pid
      pid="$(cat "$pid_file")"
      if kill -0 "$pid" 2>/dev/null; then
        stop_pid_graceful "$pid" "nginx"
        stopped=1
      fi
    fi
  done

  # 2) nginx -p <我们的 prefix> 启动的 master（pid 文件丢失时兜底）
  if command -v pgrep >/dev/null; then
    while read -r pid; do
      [[ -z "$pid" ]] && continue
      stop_pid_graceful "$pid" "nginx"
      stopped=1
    done < <(pgrep -f "nginx -p ${NGINX_PREFIX}/" 2>/dev/null || true)
  fi

  # 3) 仍占 LB 端口的 nginx 进程（上次 stop 只删了 pid 文件的残留）
  if command -v ss >/dev/null; then
    for port in 50052 50053 50054; do
      while read -r pid; do
        [[ -z "$pid" ]] && continue
        local comm
        comm="$(ps -p "$pid" -o comm= 2>/dev/null || true)"
        if [[ "$comm" == *nginx* ]]; then
          stop_pid_graceful "$pid" "nginx"
          stopped=1
        fi
      done < <(ss -tlnp 2>/dev/null | grep ":${port} " | grep -o 'pid=[0-9]*' | cut -d= -f2 | sort -u)
    done
  fi

  rm -f "$NGINX_PREFIX/nginx.pid" "$PID_DIR/nginx.pid"

  if [[ "$stopped" -eq 1 ]]; then
    echo "[nginx] 已停止"
  else
    echo "[nginx] 未运行"
  fi
}

# 清理仍占用本项目端口的 ranking/video/browse 残留进程
stop_stale_on_ports() {
  command -v ss >/dev/null || return 0

  local port pid comm args
  for port in "${OUR_PORTS[@]}"; do
    while read -r pid; do
      [[ -z "$pid" ]] && continue
      comm="$(ps -p "$pid" -o comm= 2>/dev/null || true)"
      args="$(ps -p "$pid" -o args= 2>/dev/null || true)"
      case "$comm $args" in
        *nginx*|*ranking_main*|*video_main*|*browse_main*)
          if [[ "$args" == *"$ROOT"* ]] || [[ "$comm" == *nginx* ]]; then
            echo "[cleanup] 端口 $port 仍被 pid=$pid ($comm) 占用，正在结束..."
            kill -INT "$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
            wait_pid_exit "$pid" "cleanup"
          fi
          ;;
      esac
    done < <(ss -tlnp 2>/dev/null | grep "127.0.0.1:${port} " | grep -o 'pid=[0-9]*' | cut -d= -f2 | sort -u)
  done
}

report_leftover_ports() {
  command -v ss >/dev/null || return 0
  local found=0
  for port in "${OUR_PORTS[@]}"; do
    if ss -tln 2>/dev/null | grep -q "127.0.0.1:${port} "; then
      if [[ "$found" -eq 0 ]]; then
        echo "[warn] 以下端口仍被占用：" >&2
        found=1
      fi
      ss -tlnp 2>/dev/null | grep "127.0.0.1:${port} " >&2 || \
        ss -tln 2>/dev/null | grep "127.0.0.1:${port} " >&2
    fi
  done
  if [[ "$found" -eq 1 ]]; then
    echo "  可手动: ss -tlnp | grep 5005" >&2
    return 1
  fi
  return 0
}

stop_nginx
stop_one browse-2
stop_one browse-1
stop_one browse
stop_one video-2
stop_one video-1
stop_one video
stop_one ranking-2
stop_one ranking-1
stop_one ranking

stop_stale_on_ports

if report_leftover_ports; then
  echo "全部服务已停止。"
else
  echo "部分端口未释放，请检查上方 [warn]。" >&2
  exit 1
fi
