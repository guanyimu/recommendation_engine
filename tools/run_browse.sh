#!/usr/bin/env bash
# 从项目根启动 BrowseServer，日志写入 <项目>/logs/browse/browse.INFO
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export RE_ENGINE_ROOT="$PWD"
exec bazel run //src/servers/browse:browse_main -- "$@"
