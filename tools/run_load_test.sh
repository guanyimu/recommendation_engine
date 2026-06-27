#!/usr/bin/env bash
# Browse 多进程压测（逻辑在 C++ browse_load_test 内）。
# 需先: ./tools/run_servers.sh
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export RE_ENGINE_ROOT="$PWD"
exec bazel run //examples/browse_load_test:browse_load_test -- "$@"
