#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export RE_ENGINE_ROOT="$PWD"
exec bazel run //examples/browse_demo:browse_demo -- "$@"
