#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
src="$root/unreal/Source/RedalertProof"
# Exercise the actual runtime accounting header against public model histories.
# Portable source execution is not Unreal/native PowerShell/runtime acceptance.
cpus=$(python3 - <<'PY'
import os
print(','.join(str(cpu) for cpu in sorted(os.sched_getaffinity(0))[:2]))
PY
)
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=0
  -I"$root/tests/portable_unreal" -I"$src"
  "$root/tests/production_accounting_tests.cpp" "$src/Commands/ProofCommandModel.cpp"
  "$src/Skirmish/ProofSkirmishRules.cpp" "$root/tests/portable_unreal_md5_bridge.cpp" -lcrypto)
"${CXX:-g++}" --version | head -1
printf 'Bounded portable accounting runner: CPUs %s; compiler timeout 180s; execution timeout 90s.\n' "$cpus"
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
  python3 - "$out/$mode.log" <<'PY'
import json
import sys
from pathlib import Path
rows = Path(sys.argv[1]).read_text().splitlines()
contexts = [json.loads(row.removeprefix('CONTEXT ')) for row in rows if row.startswith('CONTEXT ')]
assert len(contexts) == 3, 'Expected three actual accounting context JSON records'
for context in contexts:
    assert set(context) == {'phase', 'jobs', 'units', 'players'}, 'Context field set'
    for job in context['jobs']:
        assert set(job) == {'owner', 'sequence', 'factory', 'work', 'paid', 'held'}, 'Complete job field set'
assert rows[-1] == 'PASS production accounting actual-header/public-model checks=1573 (portable only)', 'Exact accounting check result'
print('Actual context JSON records and complete job schema verified.')
PY
done
