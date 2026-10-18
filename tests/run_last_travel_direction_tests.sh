#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-all}
if [[ $# -gt 1 || ! "$mode" =~ ^(all|normal|optimized|sanitizers|controls)$ ]]; then
  echo "Usage: $0 [all|normal|optimized|sanitizers|controls]" >&2; exit 2
fi
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
  "$build/$name"
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
PY
  local name status expected
  for name in missing-update wrong-record saturated-update destination-derived stop-clears digest-omitted; do
    compile "$build/$name" "$build/$name.cpp" -O0
    status=0
    "$build/$name" >"$build/$name.log" 2>&1 || status=$?
    case "$name" in
      missing-update|wrong-record) expected='FAIL: multi-record identity actual displacement' ;;
      saturated-update) expected='FAIL: positive Advance updates direction at saturated revision' ;;
      destination-derived) expected='FAIL: rounded-step records actual positive Y, not requested bearing' ;;
      stop-clears) expected='FAIL: Stop preserves direction' ;;
      digest-omitted) expected='FAIL: direction digest commits ordered d tag' ;;
    esac
    if [[ $status -ne 1 ]] || ! grep -Fq "$expected" "$build/$name.log"; then
      cat "$build/$name.log" >&2
      echo "FAIL: $name control did not fail its intended oracle (status $status)." >&2; exit 1
    fi
    echo "PASS: $name control rejected by its intended oracle"
  done
}
"$cxx" --version | head -1
if [[ "$mode" == all || "$mode" == normal ]]; then positive normal -O0; fi
if [[ "$mode" == all || "$mode" == optimized ]]; then positive optimized -O2 -DNDEBUG; fi
if [[ "$mode" == all || "$mode" == sanitizers ]]; then
  positive sanitizers -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
fi
if [[ "$mode" == all || "$mode" == controls ]]; then controls; fi
