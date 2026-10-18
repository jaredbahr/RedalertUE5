#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
modes=(normal optimized)
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 1 || $1 != --sanitized ]]; then
    echo "usage: $0 [--sanitized]" >&2
    exit 2
  fi
  modes=(sanitized)
fi
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -I"$src"
  "$root/tests/unit_view_grounding_tests.cpp")
"${CXX:-g++}" --version | head -1
for mode in "${modes[@]}"; do
  flags=()
  if [[ $mode == optimized ]]; then flags=(-O2 -DNDEBUG); fi
  if [[ $mode == sanitized ]]; then
    flags=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  fi
  echo "== $mode =="
  "${CXX:-g++}" "${common[@]}" "${flags[@]}" -o "$out/tests"
  if [[ $mode == sanitized ]]; then
    # Leak detection is unavailable in some sandboxed runners. Memory-access
    # and undefined-behavior checks remain active and fail the run on error.
    ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" "$out/tests"
  else
    "$out/tests"
  fi
done
