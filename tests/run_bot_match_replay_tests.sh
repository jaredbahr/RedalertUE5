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
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror
  -ffunction-sections -fdata-sections -Wl,--gc-sections
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/bot_match_replay_tests.cpp" "$src/Commands/ProofCommandModel.cpp"
  "$src/Skirmish/ProofSkirmishBot.cpp" "$src/Skirmish/ProofSkirmishRules.cpp")
"${CXX:-g++}" --version | head -1
for mode in "${modes[@]}"; do
  flags=()
  if [[ $mode == optimized ]]; then flags=(-O2 -DNDEBUG); fi
  if [[ $mode == sanitized ]]; then
    flags=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  fi
  echo "== $mode =="
  "${CXX:-g++}" "${common[@]}" "${flags[@]}" -o "$out/tests"
  "$out/tests"
done
