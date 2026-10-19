#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
# Compile the actual native tests and production model/planner. The existing
# enum-shaped UE shim deliberately does not replace the real MD5 digest path.
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=1
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/return_cargo_tests.cpp"
  "$src/Commands/ProofCommandModel.cpp" "$src/Skirmish/ProofSkirmishRules.cpp"
  "$src/Skirmish/ProofSkirmishBot.cpp" "$src/Tests/ProofAreaGuardTests.cpp"
  "$root/tests/portable_unreal_md5_bridge.cpp" -lcrypto)
cat > "$out/expected-names" <<'NAMES'
RedalertProof.Skirmish.AreaGuard.AdmissionAndReplay
RedalertProof.Skirmish.AreaGuard.AcquireChaseReturn
RedalertProof.Skirmish.AreaGuard.LeashAndTargetLoss
RedalertProof.Skirmish.AreaGuard.OverridesAndLifecycle
RedalertProof.Skirmish.AreaGuard.DamageAndConservation
RedalertProof.Skirmish.AreaGuard.DigestAndLateObserver
RedalertProof.Skirmish.AreaGuard.PlannerAndGrace
RedalertProof.Skirmish.AreaGuard.PublicModelPath
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
  # LeakSanitizer cannot operate under a ptraced/restricted executor. In that
  # environment explicitly invoke ASAN_OPTIONS=detect_leaks=0; this still runs
  # AddressSanitizer and UBSan, but must not be reported as a leak-check pass.
  if [[ $mode == sanitized && ${ASAN_OPTIONS:-} == *detect_leaks=0* ]]; then
    echo "LeakSanitizer disabled by ASAN_OPTIONS; address and undefined-behavior checks remain enabled."
  fi
  "$out/tests" | tee "$out/$mode.log"
  sed -nE 's/^(.*): (PASS|FAIL)$/\1/p' "$out/$mode.log" | LC_ALL=C sort > "$out/actual-names"
  if ! diff -u "$out/expected-names" "$out/actual-names"; then
    echo "ERROR: expected exactly the eight AreaGuard native groups; missing, duplicate or extra registration." >&2
    exit 1
  fi
done
