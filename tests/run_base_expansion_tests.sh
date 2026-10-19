#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
# Compile actual native registrations and the production model against the
# existing enum-shaped shim. Digests use the real OpenSSL-backed MD5 bridge.
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=1
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/return_cargo_tests.cpp"
  "$src/Commands/ProofCommandModel.cpp" "$src/Skirmish/ProofSkirmishRules.cpp"
  "$src/Tests/ProofBaseExpansionTests.cpp"
  "$root/tests/portable_unreal_md5_bridge.cpp" -lcrypto)
cat > "$out/expected-names" <<'NAMES'
RedalertProof.Skirmish.BaseExpansion.ChainAndEconomy
RedalertProof.Skirmish.BaseExpansion.PlacementBoundaries
RedalertProof.Skirmish.BaseExpansion.OwnershipAndLiveAnchors
RedalertProof.Skirmish.BaseExpansion.RejectionAtomicity
RedalertProof.Skirmish.BaseExpansion.LifecycleAndDeterminism
NAMES
LC_ALL=C sort "$out/expected-names" -o "$out/expected-names"
"${CXX:-g++}" --version | head -1
for mode in normal optimized sanitized; do
  flags=()
  if [[ $mode == optimized ]]; then flags=(-O2 -DNDEBUG); fi
  if [[ $mode == sanitized ]]; then
    flags=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  fi
  echo "== $mode =="
  "${CXX:-g++}" "${common[@]}" "${flags[@]}" -o "$out/tests"
  # Restricted/ptraced executors can set ASAN_OPTIONS=detect_leaks=0 externally.
  # This does not disable ASan or UBSan and must not be called a leak-check pass.
  if [[ $mode == sanitized && ${ASAN_OPTIONS:-} == *detect_leaks=0* ]]; then
    echo "LeakSanitizer disabled by ASAN_OPTIONS; address and undefined-behavior checks remain enabled."
  fi
  "$out/tests" | tee "$out/$mode.log"
  sed -nE 's/^(.*): (PASS|FAIL)$/\1/p' "$out/$mode.log" | LC_ALL=C sort > "$out/actual-names"
  if ! diff -u "$out/expected-names" "$out/actual-names"; then
    echo "ERROR: expected exactly the five BaseExpansion native groups; missing, duplicate or extra registration." >&2
    exit 1
  fi
done
