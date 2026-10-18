#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=1
  -ffunction-sections -fdata-sections -Wl,--gc-sections
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/bot_miner_recovery_tests.cpp" "$src/Commands/ProofCommandModel.cpp"
  "$src/Skirmish/ProofSkirmishBot.cpp" "$src/Skirmish/ProofSkirmishRules.cpp")
"${CXX:-g++}" --version | head -1
for mode in normal optimized; do
  flags=()
  if [[ $mode == optimized ]]; then flags=(-O2 -DNDEBUG); fi
  echo "== $mode =="
  "${CXX:-g++}" "${common[@]}" "${flags[@]}" -o "$out/tests"
  "$out/tests"
done
