#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
# An optional source tree lets the same tests challenge the unmodified baseline.
source_root=${1:-"$root"}
src="$source_root/unreal/Source/RedalertProof"
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
python3 - "$src" "$out" <<'PY'
import hashlib
import pathlib
import sys

src, out = map(pathlib.Path, sys.argv[1:])
pawn_path = src / 'Presentation/ProofRTSPawn.cpp'
pawn = pawn_path.read_bytes()
start_marker = b'void AProofRTSPawn::Produce()\n'
end_marker = b'void AProofRTSPawn::Request('
if pawn.count(start_marker) != 1 or pawn.count(end_marker) != 1:
    raise SystemExit('Produce/Request source boundaries changed; review the source-bound test adapter.')
start = pawn.index(start_marker)
end = pawn.index(end_marker, start)
method = pawn[start:end]
if not method.rstrip().endswith(b'}') or method.count(b'void AProofRTSPawn::') != 1:
    raise SystemExit('Expected exactly one complete Produce method before Request.')
(out / 'factory_produce_method.inc').write_bytes(method)
print('Compiling complete current Produce method unchanged through test-only adapters.')
print('Produce method SHA-256:', hashlib.sha256(method).hexdigest())
for relative in ('Presentation/ProofRTSPawn.cpp', 'Commands/ProofCommandModel.cpp',
                 'Commands/ProofCommandModel.h', 'Skirmish/ProofSkirmishRules.cpp',
                 'Skirmish/ProofSkirmishRules.h'):
    print(relative, hashlib.sha256((src / relative).read_bytes()).hexdigest())
PY
status=0
for mode in normal optimized; do
  flags=(); [[ $mode != optimized ]] || flags=(-O2 -DNDEBUG)
  echo "== $mode =="
  timeout 60s "${CXX:-g++}" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$root/tests/factory_production_shim" -I"$src" -I"$out" "${flags[@]}" \
    "$root/tests/factory_production_tests.cpp" "$src/Commands/ProofCommandModel.cpp" \
    "$src/Skirmish/ProofSkirmishRules.cpp" -o "$out/checks"
  timeout 30s "$out/checks" || status=1
done
exit "$status"
