#!/usr/bin/env bash
# 从项目根启动名字服务（前台），日志写入 logs/registry/
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export RE_ENGINE_ROOT="$PWD"
exec bazel run //src/registry:registry_main -- "$@"
