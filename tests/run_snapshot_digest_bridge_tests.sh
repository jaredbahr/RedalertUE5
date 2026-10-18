#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-all}"
if [[ $# -gt 1 || ! "$mode" =~ ^(all|normal|optimized|sanitizers|controls)$ ]]; then
    echo "Usage: $0 [all|normal|optimized|sanitizers|controls]" >&2
    exit 2
fi

cxx="${CXX:-g++}"
if ! command -v "$cxx" >/dev/null 2>&1; then
    echo "A C++17 compiler is required (set CXX to its executable)." >&2
    exit 1
fi
crypto_cflags=()
crypto_libs=(-lcrypto)
if command -v pkg-config >/dev/null 2>&1; then
    if ! pkg-config --exists 'libcrypto >= 3.0'; then
        echo "OpenSSL 3 development headers and libcrypto are required; no packages are installed by this runner." >&2
        exit 1
    fi
    read -r -a crypto_cflags <<< "$(pkg-config --cflags libcrypto)"
    read -r -a crypto_libs <<< "$(pkg-config --libs libcrypto)"
fi

build="$(mktemp -d "${TMPDIR:-/tmp}/proof-snapshot-digest.XXXXXX")"
trap 'rm -rf -- "$build"' EXIT
source_root="$root/unreal/Source/RedalertProof"
model="$source_root/Commands/ProofCommandModel.cpp"
bridge="$root/tests/portable_unreal_md5_bridge.cpp"
common=(-std=c++17 -Wall -Wextra -Wpedantic -Werror
    -DWITH_DEV_AUTOMATION_TESTS=0
    -I"$root/tests/portable_unreal" -I"$source_root")

compile() {
    local output="$1" model_source="$2" bridge_source="$3"
    shift 3
    "$cxx" "${common[@]}" "${crypto_cflags[@]}" "$@" \
        "$root/tests/snapshot_digest_bridge_tests.cpp" "$bridge_source" \
        "$model_source" "$source_root/Skirmish/ProofSkirmishRules.cpp" \
        "${crypto_libs[@]}" -o "$output"
}

run_positive() {
    local name="$1"
    shift
    echo "Building $name: $cxx $*"
    compile "$build/$name" "$model" "$bridge" "$@"
    "$build/$name"

    local status=0
    "$build/$name" --disable-md5 >"$build/$name-unavailable.log" 2>&1 || status=$?
    if [[ "$status" -ne 1 ]] || ! grep -Fq \
        'portable MD5 bridge: EVP_MD_fetch(MD5) failed; MD5 unavailable or disabled' \
        "$build/$name-unavailable.log"; then
        cat "$build/$name-unavailable.log" >&2
        echo "FAIL: $name unavailable-MD5 check returned unexpected status $status" >&2
        exit 1
    fi
    echo "PASS: $name unavailable MD5 fails closed (exit 1, fresh process)"
}

run_controls() {
    # Deliberately wrong bridges exist only in the temporary build directory.
    # The constant equals the empty-snapshot answer so the mixed golden must
    # independently reject it. --snapshots-only proves snapshot-level coverage.
    cat >"$build/constant.cpp" <<'EOF'
#include "Misc/SecureHash.h"
FString FMD5::HashAnsiString(const TCHAR*) { return FString("aa1d59044f1a11b0547336759a1cefb2"); }
EOF
    cat >"$build/identity.cpp" <<'EOF'
#include "Misc/SecureHash.h"
FString FMD5::HashAnsiString(const TCHAR* Input) { return FString(Input); }
EOF
    local name status
    for name in constant identity; do
        compile "$build/$name" "$model" "$build/$name.cpp" -O0
        status=0
        "$build/$name" --snapshots-only >"$build/$name.log" 2>&1 || status=$?
        if [[ "$status" -ne 1 ]] || ! grep -Fq 'FAIL: snapshot golden' "$build/$name.log"; then
            cat "$build/$name.log" >&2
            echo "FAIL: $name digest negative control was not rejected" >&2
            exit 1
        fi
        echo "PASS: snapshot goldens reject $name digest control"
    done

    # Omit a committed field only in a disposable copy, never in production.
    if [[ "$(grep -Fc 'Player.TeamId, Player.TotalGathered' "$model")" -ne 1 ]]; then
        echo "FAIL: omitted-field control cannot identify its exact source target" >&2
        exit 1
    fi
    sed 's/Player.TeamId, Player.TotalGathered/Player.TeamId, 0/' "$model" \
        >"$build/model-omit-total-gathered.cpp"
    compile "$build/omitted-field" "$build/model-omit-total-gathered.cpp" "$bridge" -O0
    status=0
    "$build/omitted-field" --snapshots-only >"$build/omitted-field.log" 2>&1 || status=$?
    if [[ "$status" -ne 1 ]] || ! grep -Fq 'FAIL: snapshot golden mixed' "$build/omitted-field.log"; then
        cat "$build/omitted-field.log" >&2
        echo "FAIL: omitted TotalGathered field control was not rejected" >&2
        exit 1
    fi
    echo "PASS: snapshot golden rejects omitted TotalGathered field control"
}

if [[ "$mode" == all || "$mode" == normal ]]; then run_positive normal -O0; fi
if [[ "$mode" == all || "$mode" == optimized ]]; then run_positive optimized -O2; fi
if [[ "$mode" == all || "$mode" == sanitizers ]]; then
    run_positive sanitizers -O1 -g -fsanitize=address,undefined \
        -fno-omit-frame-pointer -fno-sanitize-recover=all
fi
if [[ "$mode" == all || "$mode" == controls ]]; then run_controls; fi
