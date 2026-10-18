#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-all}
fp_mode=${2:-matrix}
if [[ $# -gt 2 || ! "$mode" =~ ^(all|normal|optimized|sanitizers|controls)$ || ! "$fp_mode" =~ ^(matrix|native|gradual|ftz|daz|both)$ ]]; then
  echo "Usage: $0 [all|normal|optimized|sanitizers|controls] [matrix|native|gradual|ftz|daz|both]" >&2; exit 2
fi
# Each run is a separate process. Native default comes first and never sets
# MXCSR; explicit modes save and restore the complete state within that process.
fp_modes=("$fp_mode")
if [[ "$fp_mode" == matrix ]]; then fp_modes=(native gradual ftz daz both); fi
cxx=${CXX:-g++}
crypto_cflags=()
crypto_libs=(-lcrypto)
if command -v pkg-config >/dev/null 2>&1; then
  if ! pkg-config --exists 'libcrypto >= 3.0'; then
    echo 'OpenSSL 3 development headers and libcrypto are required; this runner installs nothing.' >&2; exit 1
  fi
  read -r -a crypto_cflags <<< "$(pkg-config --cflags libcrypto)"
  read -r -a crypto_libs <<< "$(pkg-config --libs libcrypto)"
fi
build=$(mktemp -d "${TMPDIR:-/tmp}/proof-last-travel.XXXXXX")
trap 'rm -rf -- "$build"' EXIT
src="$root/unreal/Source/RedalertProof"
model="$src/Commands/ProofCommandModel.cpp"
bridge="$root/tests/portable_unreal_md5_bridge.cpp"
if [[ ! -f "$bridge" ]]; then echo "Missing real MD5 bridge: $bridge" >&2; exit 1; fi
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -DWITH_DEV_AUTOMATION_TESTS=1
  -I"$root/tests/portable_unreal" -I"$src")
compile() {
  local output=$1 model_source=$2; shift 2
  "$cxx" "${common[@]}" "${crypto_cflags[@]}" "$@" \
    "$root/tests/last_travel_direction_tests.cpp" "$src/Tests/ProofLastTravelDirectionTests.cpp" \
    "$model_source" "$src/Skirmish/ProofSkirmishRules.cpp" "$bridge" "${crypto_libs[@]}" -o "$output"
}
positive() {
  local name=$1; shift
  echo "== $name: $cxx $* =="
  compile "$build/$name" "$model" "$@"
  local fp
  for fp in "${fp_modes[@]}"; do "$build/$name" "$fp"; done
}
controls() {
  # Mutations are disposable; the candidate model and real MD5 bridge stay intact.
  python3 - "$model" "$build" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
out = Path(sys.argv[2])
def mutation(name, old, new):
    if source.count(old) != 1:
        raise SystemExit(f'{name}: expected exactly one mutation target')
    (out / f'{name}.cpp').write_text(source.replace(old, new, 1))
update = '        RecordLastTravelDirection(State.Units[Index], BeforeMove[Index].Position);'
mutation('missing-update', update, '        // Negative control: omit the actual-travel update.')
mutation('wrong-record', update, '        RecordLastTravelDirection(State.Units[Index], BeforeMove[(Index + 1) % BeforeMove.Num()].Position);')
mutation('saturated-update', update, '        if (State.Revision != MAX_int32) { RecordLastTravelDirection(State.Units[Index], BeforeMove[Index].Position); }')
mutation('destination-derived', update, '''        const FOrder* DirectionOrder = Orders.Find(State.Units[Index].UnitId);
        if (DirectionOrder && DirectionOrder->Type == EProofCommandType::Move)
        {
            FProofUnitRecord Requested = State.Units[Index];
            Requested.Position = DirectionOrder->Destination;
            RecordLastTravelDirection(Requested, BeforeMove[Index].Position);
            State.Units[Index].LastTravelDirection = Requested.LastTravelDirection;
        }
        else { RecordLastTravelDirection(State.Units[Index], BeforeMove[Index].Position); }''')
mutation('stop-clears', '            Orders.Remove(Command.UnitId);', '''            Orders.Remove(Command.UnitId);
            for (FProofUnitRecord& Record : State.Units)
                if (Record.UnitId == Command.UnitId) { Record.LastTravelDirection = FVector::ZeroVector; }''')
mutation('digest-omitted', '        if (Unit.LastTravelDirection != FVector::ZeroVector)', '        if (false)')
mutation('reciprocal-rewrite', '''    const double ScaledX = X / Scale;
    const double ScaledY = Y / Scale;''', '''    const double ReciprocalScale = 1.0 / Scale;
    const double ScaledX = X * ReciprocalScale;
    const double ScaledY = Y * ReciprocalScale;''')
mutation('numeric-noop', '    const double X = Unit.Position.X - PreviousPosition.X;',
    '    return; // Negative control: blanket numeric helper no-op.\n    const double X = Unit.Position.X - PreviousPosition.X;')
PY
  local name status expected
  # Gradual mode is intentional: the reciprocal fault must face nonzero
  # subnormal arithmetic, independent of the invoking process's default mode.
  echo "== negative controls: isolated gradual-underflow mode =="
  for name in missing-update wrong-record saturated-update destination-derived stop-clears digest-omitted reciprocal-rewrite numeric-noop; do
    compile "$build/$name" "$build/$name.cpp" -O0
    status=0
    "$build/$name" gradual >"$build/$name.log" 2>&1 || status=$?
    case "$name" in
      missing-update|wrong-record) expected='FAIL: multi-record identity actual displacement' ;;
      saturated-update) expected='FAIL: positive Advance updates direction at saturated revision' ;;
      destination-derived) expected='FAIL: rounded-step records actual positive Y, not requested bearing' ;;
      stop-clears) expected='FAIL: Stop preserves direction' ;;
      digest-omitted) expected='FAIL: direction digest commits ordered d tag' ;;
      reciprocal-rewrite) expected='FAIL: numeric subnormal_diagonal strict analytic direction' ;;
      numeric-noop) expected='FAIL: numeric minimum_normal_ratio_1_2 strict analytic direction' ;;
    esac
    if [[ $status -ne 1 ]] || ! grep -Fq "$expected" "$build/$name.log"; then
      cat "$build/$name.log" >&2
      echo "FAIL: $name control did not fail its intended oracle (status $status)." >&2; exit 1
    fi
    echo "PASS: $name control rejected by its intended oracle"
    grep -F "$expected" "$build/$name.log"
    grep -E '^FP mode=.* checks, .* failures$' "$build/$name.log"
  done
}
"$cxx" --version | head -1
if [[ "$mode" == all || "$mode" == normal ]]; then positive normal -O0; fi
if [[ "$mode" == all || "$mode" == optimized ]]; then positive optimized -O2 -DNDEBUG; fi
if [[ "$mode" == all || "$mode" == sanitizers ]]; then
  positive sanitizers -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
fi
if [[ "$mode" == all || "$mode" == controls ]]; then controls; fi
