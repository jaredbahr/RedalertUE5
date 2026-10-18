#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-all}"
if [[ $# -gt 1 || ! "$mode" =~ ^(all|normal|optimized|sanitizers)$ ]]; then
    echo "Usage: $0 [all|normal|optimized|sanitizers]" >&2
    exit 2
fi
cxx="${CXX:-g++}"
build="$(mktemp -d "${TMPDIR:-/tmp}/proof-unit-facing.XXXXXX")"
trap 'rm -rf -- "$build"' EXIT
run() {
    local name="$1"; shift
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror "$@" \
        -I"$root/unreal/Source/RedalertProof" "$root/tests/unit_view_facing_tests.cpp" -o "$build/$name"
    "$build/$name"
}
if [[ "$mode" == all || "$mode" == normal ]]; then run normal; fi
if [[ "$mode" == all || "$mode" == optimized ]]; then run optimized -O2 -DNDEBUG; fi
if [[ "$mode" == all || "$mode" == sanitizers ]]; then
    run sanitizers -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
fi
