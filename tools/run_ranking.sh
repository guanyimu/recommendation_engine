#!/usr/bin/env bash
# 从项目根启动 ranking，确保日志写入 <项目>/logs/ranking/
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export RE_ENGINE_ROOT="$PWD"
exec bazel run //src/servers/ranking:ranking_main -- "$@"
