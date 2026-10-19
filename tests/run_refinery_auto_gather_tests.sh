#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=1
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/return_cargo_tests.cpp" "$src/Commands/ProofCommandModel.cpp" "$src/Skirmish/ProofSkirmishRules.cpp"
  "$root/tests/portable_unreal_md5_bridge.cpp" "$src/Tests/ProofRefineryAutoGatherTests.cpp" -lcrypto)
"${CXX:-g++}" --version | head -1
for mode in normal optimized sanitized; do
  flags=()
  if [[ $mode == optimized ]]; then flags=(-O2 -DNDEBUG); fi
  if [[ $mode == sanitized ]]; then flags=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer); fi
  echo "== $mode =="
  "${CXX:-g++}" "${common[@]}" "${flags[@]}" -o "$out/tests"
  "$out/tests"
done
