#!/usr/bin/env bash
# 从项目根启动 video 服务
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export RE_ENGINE_ROOT="$PWD"
exec bazel run //src/servers/video:video_main -- "$@"
