#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
# Actual native registration bodies and real source model/rules/bot. This is a
# portable compiler check, not an Unreal native-engine or runtime acceptance.
# Choose at most two CPUs from this process's permitted set; do not assume 0,1.
cpus=$(python3 - <<'PY'
import os
print(','.join(str(cpu) for cpu in sorted(os.sched_getaffinity(0))[:2]))
PY
)
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=1
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/return_cargo_tests.cpp" "$src/Commands/ProofCommandModel.cpp"
  "$src/Skirmish/ProofSkirmishRules.cpp" "$src/Skirmish/ProofSkirmishBot.cpp"
  "$src/Tests/ProofTimedProductionTests.cpp" "$root/tests/portable_unreal_md5_bridge.cpp" -lcrypto)
cat > "$out/expected-names" <<'NAMES'
RedalertProof.Skirmish.TimedProduction.StartAndFunding
RedalertProof.Skirmish.TimedProduction.ControlsAndIdentity
RedalertProof.Skirmish.TimedProduction.QuantizedTimeAndDigest
RedalertProof.Skirmish.TimedProduction.BlockedReleaseAndRally
RedalertProof.Skirmish.TimedProduction.LossAndRefundLiability
RedalertProof.Skirmish.TimedProduction.OwnersAndLifecycle
RedalertProof.Skirmish.TimedProduction.BotPlanningAndReplay
NAMES
LC_ALL=C sort "$out/expected-names" -o "$out/expected-names"
"${CXX:-g++}" --version | head -1
printf 'Bounded portable runner: CPUs %s; compiler timeout 180s; execution timeout 90s.\n' "$cpus"
for mode in normal optimized sanitized; do
  flags=()
  if [[ $mode == optimized ]]; then flags=(-O2 -DNDEBUG); fi
  if [[ $mode == sanitized ]]; then
    flags=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  fi
  echo "== $mode =="
  timeout --signal=TERM --kill-after=5s 180s taskset -c "$cpus" "${CXX:-g++}" "${common[@]}" "${flags[@]}" -o "$out/tests"
  if [[ $mode == sanitized && ${ASAN_OPTIONS:-} == *detect_leaks=0* ]]; then
    echo 'LeakSanitizer disabled externally; this is not a leak-check pass. Address and undefined-behavior checks remain enabled.'
  fi
  timeout --signal=TERM --kill-after=5s 90s taskset -c "$cpus" "$out/tests" | tee "$out/$mode.log"
  sed -nE 's/^(.*): (PASS|FAIL)$/\1/p' "$out/$mode.log" | LC_ALL=C sort > "$out/actual-names"
  if ! diff -u "$out/expected-names" "$out/actual-names"; then
    echo 'ERROR: expected exactly seven TimedProduction native groups; missing, duplicate or extra registration.' >&2
    exit 1
  fi
done
