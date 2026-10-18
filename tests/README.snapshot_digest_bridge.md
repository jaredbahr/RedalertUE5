# Test-only snapshot digest bridge

This opt-in portable harness links and executes the repository's production
`FProofSnapshot::ComputeDigest()` with a real MD5 implementation supplied by
OpenSSL EVP. It replaces neither the production digest body nor the portable
`Misc/SecureHash.h` declaration. It does not add a hash stub to the existing
default runners; those runners retain their existing undefined-hash guard.

The preparation baseline is commit
`d965aa2163cccc2646ed4b954fabb228a54a1e50`. All ten copied production/portable
dependencies were checked against their Git blob identities during standalone
bridge preparation. The accompanying last-travel-direction change extends the
production digest with an optional direction tag. This bridge still calls that
actual production function; it does not substitute a copied canonicalizer.
The bridge's own added files are:

- `tests/portable_unreal_md5_bridge.cpp`
- `tests/snapshot_digest_bridge_tests.cpp`
- `tests/run_snapshot_digest_bridge_tests.sh`
- this README

The fixed no-direction goldens below deliberately remain stable. Direction
defaults, full scalar precision, sorting, unexpected nonzero Z, copied stationary
state, and combined rally/direction tags are checked separately by
`tests/run_last_travel_direction_tests.sh`, using this same real MD5 bridge.

## Dependencies and use

The runner needs Bash, a C++17 compiler, and **OpenSSL 3.x development headers
and libcrypto** with an available MD5 provider implementation. The command-line
`openssl` executable alone is not enough. Development packages are commonly
named `libssl-dev` or `openssl-devel`; the runner never installs anything.
`pkg-config` is used when available to find `libcrypto >= 3.0`. Without it,
standard compiler header/library search paths and `-lcrypto` are used. Set
`CXX` to a compiler executable if it is not `g++`.

From the repository root:

```sh
bash tests/run_snapshot_digest_bridge_tests.sh           # all modes + controls
bash tests/run_snapshot_digest_bridge_tests.sh normal    # -O0
bash tests/run_snapshot_digest_bridge_tests.sh optimized # -O2
bash tests/run_snapshot_digest_bridge_tests.sh sanitizers # ASan + UBSan
bash tests/run_snapshot_digest_bridge_tests.sh controls   # deliberate bad copies
```

Every build uses `-std=c++17 -Wall -Wextra -Wpedantic -Werror`. Sanitizers use
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all`. No linker garbage collection is used to discard
`ComputeDigest`. Builds and negative-control copies live in a fresh temporary
directory, removed when the runner exits. No default test runner is altered.

## Bridge contract and failure behavior

- Input must be a valid, non-null, NUL-terminated ASCII C string. The portable
  shim's `TCHAR` is `char`; native Unreal's text conversions are not reproduced.
- Bytes are hashed up to, but not including, the first NUL. Anything after that
  terminator is ignored, including non-ASCII bytes. Empty strings are supported.
- Bytes above `0x7f` before the terminator and null pointers are rejected with a
  clear `std::invalid_argument`. This is not a general binary/UTF-8 hash API.
- The result has exactly 32 lowercase hexadecimal digits.
- The bridge obtains MD5 with `EVP_MD_fetch`, checks the algorithm size and each
  allocation/digest operation, and owns EVP resources with RAII. Failures throw
  a descriptive exception; the harness prints `FAIL:` and exits nonzero. There
  is no dummy digest, fallback algorithm, silent skip, or forced provider load.
- Default OpenSSL provider policy is respected, including environments where
  MD5 is unavailable. The normal suite fails in that situation; do not weaken
  an installed security policy just to make this diagnostic test run.

For each positive build, the runner launches a **separate fresh process** with
`--disable-md5`. That process sets an impossible provider query in its own
OpenSSL default context, then calls the real production `ComputeDigest()`.
The runner requires exit status 1 and the exact MD5-fetch failure diagnostic.
An unexpected successful digest or unrelated failure fails the runner. This
changes no system configuration and persists no settings. This exercises the
unavailable-algorithm path; it does not inject every possible EVP failure.

## Independent expectations and coverage

The bridge is first checked against all seven fixed vectors in
[RFC 1321, Appendix A.5](https://www.rfc-editor.org/rfc/rfc1321.txt): empty,
`a`, `abc`, `message digest`, the alphabet, the alphanumeric string, and the
80-digit string. The expected hashes are literals, not runtime outputs from
the function under test. Additional bridge tests check output shape, repeated
interleaved calls, NUL handling, null input, and non-ASCII rejection.

Snapshot expectations come from these independently specified ASCII strings,
hashed with Python `hashlib` before the harness was written:

Empty canonical input:

```text
revision:0;epoch:0;skirmish:0;phase:0;winner:-1;
```

Expected MD5: `aa1d59044f1a11b0547336759a1cefb2`.

Mixed canonical input (one line, no newline):

```text
revision:7;epoch:3;skirmish:1;phase:1;winner:-1;p:1,200,0,0;p:2,125,0,50;u:4,1,-3,4,0.5,100,4,0;q:4,1,10,20,0;u:9,2,1.25,-2.5,0,75,1,7;r:3,1,-7,8,0,0;r:8,2,5,6,0,125;
```

Expected MD5: `431a40e1809cacc8fff55d85e13312b7`.

For example, verify a literal independently without invoking the bridge:

```python
import hashlib
canonical = b"revision:0;epoch:0;skirmish:0;phase:0;winner:-1;"
assert hashlib.md5(canonical).hexdigest() == "aa1d59044f1a11b0547336759a1cefb2"
```

The C++ fixture constructs the mixed snapshot directly with intentionally
unsorted arrays and checks the fixed expected hash from the actual production
method. It does not reimplement the canonical serializer to compute an oracle.
Coverage includes:

- Fixed empty and mixed golden digests, plus repeated interleaved snapshot calls.
- All eight reorder combinations for the fixture's two players, two units, and
  two resources. Digest computation must leave each original array's order intact.
- Thirty isolated changes: all current header/player/unit/resource digest
  fields, all XYZ coordinates, rally flag/XYZ, and record removals.
- Enabled zero rally and disabled nonzero rally coordinates, which must differ
  from a default disabled/zero rally.
- Constant-hash and identity-hash negative controls that run the snapshot
  checks alone. The constant deliberately equals the empty-snapshot golden, so
  the mixed golden must reject it independently.
- An omitted-`TotalGathered` negative control in a disposable production-source
  copy, rejected by the mixed snapshot golden. The baseline file is never edited.

## Evidence and limits

The standalone suite reports **481 checks per positive build**. The three
negative controls and each fresh-process unavailable-MD5 check are additional
runner checks, outside that count.

Preparation execution on 2026-10-18 used GCC 14.2.0 and OpenSSL 3.5.7. Normal
and optimized modes passed, including their unavailable-MD5 checks. All three
negative controls were rejected as expected. The ASan+UBSan mode also passed with the explicit
invocation below, including its fresh-process unavailable-MD5 check:

```sh
ASAN_OPTIONS=detect_leaks=0 bash tests/run_snapshot_digest_bridge_tests.sh sanitizers
```

The first default sanitizer invocation stopped because LeakSanitizer reported
that it cannot operate under ptrace in this executor. Leak checking therefore
remains **unverified**. Disabling leak detection for the recorded retry does not
disable AddressSanitizer or UndefinedBehaviorSanitizer; the runner itself does
not set or weaken sanitizer options. Use the ordinary sanitizer command in an
environment that supports LeakSanitizer.

This is portable test evidence, **not native Unreal execution**. Native Unreal
`FString` formatting, `TCHAR` conversion, platform behavior, engine integration,
PowerShell runners, and UE 5.7 execution remain unverified. The fixture pins the
C numeric locale because the portable `FString` uses `printf`. The snapshot
goldens are not a claim of engine-wide serialization equivalence. Duplicate
record-ID ordering and arbitrary floating-point edge cases are outside this
focused fixture. This bridge suite's fixtures leave direction unset. The
companion last-travel suite validates nondefault direction through the same
production digest and bridge. The bridge itself changes no production behavior.

The digest is the model's diagnostic visible-state comparison, not an
authentication mechanism or a lockstep proof.

Primary API references:

- [OpenSSL EVP digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
  documents explicit algorithm fetch, checked operation results, and resource
  ownership. This bridge uses OpenSSL 3 APIs.
- [Epic's UE 5.5 `FMD5::HashAnsiString` documentation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/Misc/FMD5/HashAnsiString?application_version=5.5)
  describes ASCII input and 32 lowercase hexadecimal digits. It supplies the
  limited contract modeled here, not proof of a native UE 5.7 run.
