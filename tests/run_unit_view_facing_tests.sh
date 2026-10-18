#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-all}"
if [[ $# -gt 1 || ! "$mode" =~ ^(all|normal|optimized|sanitizers|controls)$ ]]; then
    echo "Usage: $0 [all|normal|optimized|sanitizers|controls]" >&2
    exit 2
fi
cxx="${CXX:-g++}"
build="$(mktemp -d "${TMPDIR:-/tmp}/proof-unit-facing.XXXXXX")"
trap 'rm -rf -- "$build"' EXIT
run() {
    local name="$1"; shift
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror "$@" \
        -I"$root/unreal/Source/RedalertProof" "$root/tests/unit_view_facing_tests.cpp" -o "$build/$name"
    "$build/$name"
    local capability_status=0
    "$build/$name" --fp-modes-supported || capability_status=$?
    if [[ $capability_status -eq 0 ]]; then
        for fp_mode in gradual ftz daz ftz-daz; do "$build/$name" "$fp_mode"; done
    elif [[ $capability_status -eq 1 ]]; then
        echo "Explicit MXCSR controls unavailable for this target; native mode only."
    else
        echo "FP capability probe failed unexpectedly (status $capability_status)." >&2
        exit 1
    fi
}
if [[ "$mode" == all || "$mode" == normal ]]; then run normal; fi
if [[ "$mode" == all || "$mode" == optimized ]]; then run optimized -O2 -DNDEBUG; fi
if [[ "$mode" == all || "$mode" == sanitizers ]]; then
    run sanitizers -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
fi
if [[ "$mode" == all || "$mode" == controls ]]; then
    mkdir -p "$build/mutant/Presentation"
    python3 - "$root/unreal/Source/RedalertProof/Presentation/ProofUnitViewFacing.h" "$build/mutant/Presentation/ProofUnitViewFacing.h" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = '        const double SX = X / Scale, SY = Y / Scale;'
assert source.count(old) == 1
Path(sys.argv[2]).write_text(source.replace(old,
    '        const double InverseScale = 1.0 / Scale;\n'
    '        const double SX = X * InverseScale, SY = Y * InverseScale;', 1))
PY
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG \
        -I"$build/mutant" "$root/tests/unit_view_facing_tests.cpp" -o "$build/reciprocal-mutant"
    if "$build/reciprocal-mutant" --fp-modes-supported; then
        status=0
        "$build/reciprocal-mutant" gradual >"$build/reciprocal-mutant.log" 2>&1 || status=$?
        if [[ $status -ne 1 ]] || ! grep -Fq 'subnormal_cardinal: valid finite vectors rejected' "$build/reciprocal-mutant.log"; then
            cat "$build/reciprocal-mutant.log" >&2
            echo "FAIL: reciprocal control did not fail its intended subnormal oracle (status $status)." >&2
            exit 1
        fi
        echo "PASS: reciprocal-rewrite control rejected by strict subnormal cardinal oracle"
    else
        echo "Reciprocal fault control requires explicit gradual mode on an x86 MXCSR target." >&2
        exit 1
    fi
fi
