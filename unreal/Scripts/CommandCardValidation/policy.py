"""PR72 receipt policy. Parsing/checking evidence never establishes physical input."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import struct
from datetime import datetime, timezone
from pathlib import Path

BOUNDS = {"admission": 20, "qualification": 650, "build": 900, "native": 120, "render": 220, "interactive": 220}
FROZEN_HEAD = "22575d19c0f50805aeacf1d0b1d1b5f2d861d65d"
SHA = re.compile(r"[a-f0-9]{64}\Z")
HEAD = re.compile(r"[a-f0-9]{40}\Z")
RESOURCE_PINS = {
    "readiness": "4b2953450392581c694bf791efa41d986f198e9de699077d022d3922f1a09e18",
    "admission": "f891ebaf39f09a738578ce0da9d94fa47352b27e84d8ce0fb56d447e49c5c353",
}


class Rejected(ValueError):
    pass


def release_own_console():
    """Detach only this child from its console; retain redirected I/O handles."""
    if os.name == "nt":
        import ctypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.FreeConsole.argtypes = []
        kernel.FreeConsole.restype = ctypes.c_int
        if not kernel.FreeConsole():
            raise OSError(ctypes.get_last_error(), "Own-console release failed")


def require(condition, message):
    if not condition:
        raise Rejected(message)


def pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, "duplicate JSON property")
        result[key] = value
    return result


def loads(raw):
    require(type(raw) is bytes and 0 < len(raw) <= 1024 * 1024, "JSON byte limit")
    try:
        value = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=pairs,
                           parse_constant=lambda _: (_ for _ in ()).throw(Rejected("nonfinite JSON")))
    except (UnicodeError, ValueError, RecursionError) as exc:
        raise Rejected("invalid JSON: " + str(exc)) from exc
    require(type(value) is dict, "object receipt required")
    def depth(item, level=0):
        require(level <= 16, "JSON depth limit")
        if type(item) is dict:
            for child in item.values():
                depth(child, level + 1)
        elif type(item) is list:
            for child in item:
                depth(child, level + 1)
    depth(value)
    return value


def read(path):
    with Path(path).open("rb") as stream:
        return loads(stream.read(1024 * 1024 + 1))


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def number(value, minimum=0):
    return type(value) in (int, float) and math.isfinite(value) and value >= minimum


def flag(obj, key, expected):
    require(key in obj and type(obj[key]) is bool and obj[key] is expected, "invalid " + key)


def null(obj, key):
    require(key in obj and obj[key] is None, "missing/non-null " + key)


def utc(value):
    require(type(value) is str and len(value) <= 50, "timestamp required")
    try:
        result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise Rejected("invalid timestamp") from exc
    require(result.tzinfo is not None and result.utcoffset().total_seconds() == 0, "UTC timestamp required")
    return result


def interval(record, start_key="startedUtc", end_key="finishedUtc"):
    start, end = utc(record.get(start_key)), utc(record.get(end_key))
    require(end >= start, "reversed timestamps")
    return start, end


def identity(record, head):
    require(HEAD.fullmatch(head) is not None and record.get("head") == head, "source head mismatch")


def validate_outcome(value, mode, expected_frequency):
    require(mode in BOUNDS, "unknown stage")
    require(value.get("stem") == mode and type(value.get("boundSeconds")) is int
            and value["boundSeconds"] == BOUNDS[mode], "stage bound mismatch")
    for key in ("passed", "resumed", "jobAssigned", "cleanupSucceeded", "outputComplete"):
        flag(value, key, True)
    for key in ("timedOut", "reservationLost", "leaseCancelled", "terminationRequested"):
        flag(value, key, False)
    for key in ("error", "cleanupError"):
        null(value, key)
    require(type(value.get("exitCode")) is int and value["exitCode"] == 0, "outer child exit failed")
    require(type(value.get("activeOwnedProcessesAfterCleanup")) is int
            and value["activeOwnedProcessesAfterCleanup"] == 0, "owned survivors")
    for key in ("pid", "creationFileTime", "frequency", "deadlineStartQpc", "observedExitQpc", "completedQpc"):
        require(integer(value.get(key), 1), "missing native identity/timing: " + key)
    origin, end, final, frequency = (value[k] for k in
        ("deadlineStartQpc", "observedExitQpc", "completedQpc", "frequency"))
    require(integer(expected_frequency, 1) and frequency == expected_frequency, "native QPC frequency mismatch")
    require(origin <= end <= final < origin + BOUNDS[mode] * frequency, "late or invalid stage completion")
    for key in ("stdoutBytes", "stderrBytes"):
        require(integer(value.get(key)) and value[key] <= 1024 * 1024, "incomplete/big output")
    interval(value)


def validate_session(value, head, viewport, frequency):
    identity(value, head)
    require(viewport in ("1280x720", "960x540") and value.get("requestedViewport") == viewport,
            "viewport mismatch")
    flag(value, "expired", False)
    flag(value, "physicalInputVerified", False)
    flag(value, "humanConfirmationRequired", True)
    null(value, "error")
    require(type(value.get("exitCode")) is int and value["exitCode"] == 0, "UE exit failed or missing")
    require(type(value.get("sessionLimitSeconds")) is int and value["sessionLimitSeconds"] == 180,
            "interactive limit mismatch")
    require(type(value.get("affinityMask")) is int and value["affinityMask"] == 68
            and value.get("priority") == "Normal", "resource policy mismatch")
    for key in ("uePid", "ueCreationFileTime", "launchStartQpc", "exitObservedQpc", "qpcFrequency"):
        require(integer(value.get(key), 1), "missing UE launch/exit evidence: " + key)
    require(integer(frequency, 1) and value["qpcFrequency"] == frequency, "QPC frequency mismatch")
    require(integer(value.get("windowHandle"), 1) and integer(value.get("policyPolls"), 1), "missing loaded-window/policy observation")
    origin, end, frequency = (value[k] for k in ("launchStartQpc", "exitObservedQpc", "qpcFrequency"))
    require(origin <= end < origin + 180 * frequency, "interactive launch-relative deadline")
    cleanup = value.get("cleanup")
    require(type(cleanup) is dict, "cleanup receipt required")
    flag(cleanup, "nativeExitConfirmed", True)
    flag(cleanup, "terminationRequested", False)
    null(cleanup, "error")
    require(number(cleanup.get("elapsedSeconds")) and cleanup["elapsedSeconds"] <= 5, "cleanup budget")
    interval(value)
    return {"valid": True, "physicalInputVerified": False}


def validate_ready(value, now):
    # Original resource-only sampler may retain its frozen-source identity. It is
    # never relabeled as PR72 gameplay or as a passing original acceptance gate.
    flag(value, "eligible", True)
    flag(value, "acceptanceEvidence", False)
    null(value, "error")
    require(value.get("frozenHead") == FROZEN_HEAD, "resource sampler identity mismatch")
    require(number(value.get("elapsedSeconds"), 600) and value["elapsedSeconds"] < 650, "readiness duration")
    rows = value.get("samples")
    require(type(rows) is list and 2 <= len(rows) <= 256, "readiness samples required")
    previous = None
    for row in rows:
        require(type(row) is dict, "sample object required")
        null(row, "error")
        stamp = utc(row.get("utc"))
        if previous is not None:
            require(0 < (stamp - previous).total_seconds() <= 10, "readiness sample gap")
        previous = stamp
        for key in ("aggregate", "cpu2", "cpu6"):
            require(number(row.get(key)) and row[key] < 80, "CPU threshold")
        for key in ("conflicts", "games", "otherValidationProcesses"):
            require(type(row.get(key)) is int and row[key] == 0, "competing workload")
        require(integer(row.get("freeMemoryKiB")) and row["freeMemoryKiB"] >= 8 * 1024 * 1024, "RAM threshold")
        require(integer(row.get("freeDBytes")) and row["freeDBytes"] >= 20 * 1024**3, "disk threshold")
    require((utc(rows[-1]["utc"]) - utc(rows[0]["utc"])).total_seconds() >= 600, "short quiet window")
    completed = utc(value.get("utc"))
    require(utc(rows[-1]["utc"]) <= completed <= now, "readiness completion chronology")
    return completed


def validate_lease(lease, ready, admission, head, now):
    identity(lease, head)
    flag(lease, "ownedLanesDrained", True)
    require(type(lease.get("cancelPath")) is str and not str(lease["cancelPath"]).startswith("\\\\"), "local cancellation path required")
    regular(lease["cancelPath"])
    created, expires = utc(lease.get("createdUtc")), utc(lease.get("expiresUtc"))
    require((expires - created).total_seconds() == 1200, "unextended 20-minute lease required")
    require(ready <= created <= now < expires, "qualification must precede reservation")
    require((created - ready).total_seconds() <= 60, "qualification stale before reservation")
    flag(admission, "eligible", True)
    flag(admission, "clean", True)
    require(admission.get("head") == FROZEN_HEAD and admission.get("mask") == "0x44"
            and admission.get("priority") == "Normal", "original resource admission identity")
    for key in ("conflictsBefore", "conflictsAfter", "gamesBefore", "gamesAfter"):
        require(type(admission.get(key)) is int and admission[key] == 0, "admission conflicts")
    require(integer(admission.get("freeMemoryKiB")) and admission["freeMemoryKiB"] >= 8 * 1024 * 1024, "admission RAM")
    require(integer(admission.get("freeDBytes")) and admission["freeDBytes"] >= 20 * 1024**3, "admission disk")
    samples = admission.get("samples")
    require(type(samples) is list and len(samples) == 3, "three dispatch samples required")
    stamps = [utc(row.get("utc")) for row in samples]
    require(created <= stamps[0] < stamps[1] < stamps[2], "dispatch admission must follow reservation")
    require((stamps[-1] - stamps[0]).total_seconds() <= 20, "admission exceeded its bound")
    for row in samples:
        for key in ("aggregate", "cpu2", "cpu6"):
            require(number(row.get(key)) and row[key] < 80, "dispatch CPU")
    done = utc(admission.get("utc"))
    require(stamps[-1] <= done <= now and (now - done).total_seconds() <= 60, "stale dispatch admission")
    return expires


def file_ref(reference):
    require(type(reference) is dict and type(reference.get("path")) is str
            and type(reference.get("sha256")) is str and SHA.fullmatch(reference["sha256"]), "hashed file reference required")
    path = regular(reference["path"])
    require(path.is_file(), "regular absolute receipt required")
    require(0 < path.stat().st_size <= 1024 * 1024, "receipt size limit")
    with path.open("rb") as stream:
        raw = stream.read(1024 * 1024 + 1)
    require(0 < len(raw) <= 1024 * 1024 and hashlib.sha256(raw).hexdigest() == reference["sha256"], "receipt hash/size mismatch")
    return loads(raw)


def regular(path):
    require(isinstance(path, (str, Path)), "path required")
    path = Path(path)
    require(path.is_absolute(), "absolute path required")
    for parent in (path, *path.parents):
        if parent.exists():
            require(not parent.is_symlink() and not (getattr(parent.lstat(), "st_file_attributes", 0) & 0x400),
                    "reparse paths are not eligible")
    return path


def verify_bindings(path, digest, review_path):
    require(type(digest) is str and SHA.fullmatch(digest), "reviewed binding SHA256 required")
    binding = file_ref({"path": str(regular(path)), "sha256": digest})
    require(binding.get("scope") == "PR72", "wrong binding scope")
    head = binding.get("sourceHead")
    require(type(head) is str and HEAD.fullmatch(head), "exact source head required")
    review = read(regular(review_path))
    flag(review, "approved", True)
    require(review.get("bindingsSha256") == digest and review.get("sourceHead") == head
            and review.get("reviewType") == "independent" and type(review.get("reviewer")) is str
            and bool(review["reviewer"].strip()), "independent exact-binding review required")
    root = regular(binding.get("sourceRoot", ""))
    require(root.is_dir() and root.drive.upper() == "D:", "isolated D source root required")
    files = binding.get("files")
    require(type(files) is list and 1 <= len(files) <= 1024, "bounded source pins required")
    seen = set()
    for item in files:
        require(type(item) is dict, "source pin object required")
        relative = item.get("relative")
        require(type(relative) is str and relative and "\\" not in relative
                and not Path(relative).drive and not Path(relative).is_absolute()
                and not ({"..", ".", ""} & set(relative.split("/")))
                and relative.casefold() not in seen, "invalid or repeated source pin")
        seen.add(relative.casefold())
        target = regular(root / relative)
        require(target.is_file() and integer(item.get("bytes")) and item["bytes"] <= 16 * 1024**2
                and target.stat().st_size == item["bytes"] and type(item.get("sha256")) is str
                and SHA.fullmatch(item["sha256"]), "source pin shape")
        with target.open("rb") as stream:
            require(hashlib.file_digest(stream, "sha256").hexdigest() == item["sha256"], "source bytes changed: " + relative)
    required = {"unreal/Scripts/CommandCardValidation/" + name for name in
                ("policy.py", "run-stage.ps1", "stage-worker.ps1", "CommandCardStageSupervisor.cs")}
    required.update("unreal/Scripts/" + name for name in
                    ("build-native-proof.ps1", "test-hud-layout-model.ps1", "test-small-match.ps1",
                     "start-small-match.ps1", "start-local-interactive.ps1"))
    require({name.casefold() for name in required} <= seen, "incomplete execution-source binding")
    require(type(binding.get("cppTree")) is str and HEAD.fullmatch(binding["cppTree"]), "C++ tree binding required")
    require(regular(binding.get("storageRoot", "")).drive.upper() == "D:", "designated local storage required")
    regular(binding.get("engineRoot", ""))
    assets = binding.get("assets")
    require(type(assets) is list and len(assets) == 14, "exact existing authored-content set required")
    asset_names = set()
    for asset in assets:
        require(type(asset) is dict, "asset pin object required")
        relative = asset.get("relative")
        require(type(relative) is str and relative and "\\" not in relative
                and not Path(relative).drive and not Path(relative).is_absolute()
                and not ({"..", ".", ""} & set(relative.split("/")))
                and relative.casefold() not in asset_names, "asset path mismatch")
        asset_names.add(relative.casefold())
        target = regular(root / "unreal/Content" / relative)
        require(target.is_file() and integer(asset.get("bytes"), 1) and target.stat().st_size == asset["bytes"]
                and type(asset.get("sha256")) is str and SHA.fullmatch(asset["sha256"]), "actual pinned content required")
        with target.open("rb") as stream:
            require(hashlib.file_digest(stream, "sha256").hexdigest() == asset["sha256"], "authored content changed")
    for key in ("pwsh", "python", "git", "editor", "editorCmd"):
        entry = binding.get("executables", {}).get(key, {})
        target = regular(entry.get("path", ""))
        require(target.is_file() and type(entry.get("sha256")) is str and SHA.fullmatch(entry["sha256"]), "executable pin required")
        with target.open("rb") as stream:
            require(hashlib.file_digest(stream, "sha256").hexdigest() == entry["sha256"], "executable changed")
    for key, expected in RESOURCE_PINS.items():
        entry = binding.get("resourceTools", {}).get(key, {})
        target = regular(entry.get("path", ""))
        require(entry.get("sha256") == expected and target.is_file(), "original resource-only sampler pin required")
        with target.open("rb") as stream:
            require(hashlib.file_digest(stream, "sha256").hexdigest() == expected, "resource sampler changed")
    return binding


def validate_payload(mode, value, head, viewport, frequency=None):
    if mode == "interactive":
        return validate_session(value, head, viewport, frequency)
    identity(value, head)
    flag(value, "passed", True)
    flag(value, "dirtyWorkingTree", False)
    null(value, "error")
    interval(value)
    if mode == "native":
        for key, expected in (("testCount", 13), ("succeeded", 13), ("failed", 0)):
            require(type(value.get(key)) is int and value[key] == expected, "native test count mismatch")
        cleanup = value.get("cleanup")
        require(type(cleanup) is dict, "native cleanup required")
        flag(cleanup, "verifiedExited", True)
        flag(cleanup, "terminationRequested", False)
        null(cleanup, "error")
    elif mode == "render":
        require(value.get("scenario") == "Victory" and type(value.get("scenarioTimeoutSeconds")) is int
                and value["scenarioTimeoutSeconds"] == 220, "render case mismatch")
        require(type(value.get("cleanupErrors")) is list and not value["cleanupErrors"], "render cleanup errors")
        flag(value, "forcedCleanup", False)
        size = value.get("renderConfiguration", {})
        width, height = map(int, viewport.split("x"))
        require(type(size.get("width")) is int and type(size.get("height")) is int
                and (size["width"], size["height"]) == (width, height), "render viewport mismatch")
        shots = value.get("screenshots")
        expected = {"01-base.png", "02-result.png", "03-left-edge.png", "04-right-edge.png",
                    "05-panel-edge.png", "06-camera-before.png", "07-camera-after.png"}
        require(type(shots) is list and len(shots) == 7 and {s.get("name") for s in shots} == expected,
                "exact fresh capture set required")
    else:
        raise Rejected("unknown payload mode")
    return {"valid": True, "physicalInputVerified": False}


def validate_build_reference(prerequisites, head, root, frequency):
    outer = file_ref(prerequisites.get("buildOutcome"))
    validate_outcome(outer, "build", frequency)
    proof = file_ref(prerequisites.get("build"))
    require(Path(prerequisites["build"]["path"]).parent == Path(prerequisites["buildOutcome"]["path"]).parent,
            "build proof and native outcome must belong to the same case")
    identity(proof, head)
    flag(proof, "automatedStageValidated", True)
    null(proof, "error")
    require(proof.get("case") == "Build" and proof.get("mode") == "build"
            and regular(proof.get("sourceRoot", "")) == regular(root), "build provenance mismatch")
    start, end = interval(proof)
    outer_start, outer_end = interval(outer)
    require(outer_start <= start <= end <= outer_end, "build proof outside owned stage")
    for field, relative in (("dllSha256", "unreal/Binaries/Win64/UnrealEditor-RedalertProof.dll"),
                            ("modulesSha256", "unreal/Binaries/Win64/UnrealEditor.modules")):
        path = regular(Path(root) / relative)
        require(type(proof.get(field)) is str and SHA.fullmatch(proof[field]), "actual build hash required")
        require(path.is_file(), "actual build artifact missing")
        with path.open("rb") as stream:
            require(hashlib.file_digest(stream, "sha256").hexdigest() == proof[field], "actual built bytes changed")
        times = path.stat()
        created = datetime.fromtimestamp(getattr(times, "st_birthtime", times.st_ctime), timezone.utc)
        modified = datetime.fromtimestamp(times.st_mtime, timezone.utc)
        require(start <= created <= end and start <= modified <= end, "build artifact timestamp outside build interval")
    module = read(Path(root) / "unreal/Binaries/Win64/UnrealEditor.modules")
    require(type(module.get("BuildId")) is str and bool(module["BuildId"].strip())
            and module.get("Modules", {}).get("RedalertProof") == "UnrealEditor-RedalertProof.dll"
            and module["BuildId"] == proof.get("buildId"), "actual module mapping mismatch")


def validate_prerequisites(prerequisites, head, now, frequency):
    ready = file_ref(prerequisites.get("readiness"))
    admission = file_ref(prerequisites.get("admission"))
    lease = file_ref(prerequisites.get("lease"))
    ready_outer = file_ref(prerequisites.get("readinessOutcome"))
    dispatch_outer = file_ref(prerequisites.get("admissionOutcome"))
    validate_outcome(ready_outer, "qualification", frequency)
    validate_outcome(dispatch_outer, "admission", frequency)
    completed = validate_ready(ready, now)
    for receipt, outer in ((ready, ready_outer), (admission, dispatch_outer)):
        start, end = interval(outer)
        require(start <= utc(receipt["samples"][0]["utc"]) <= utc(receipt["utc"]) <= end <= now,
                "resource receipt outside its owned stage")
    validate_lease(lease, completed, admission, head, now)


def validate_artifacts(receipt_path, head, viewport, started):
    value = read(regular(receipt_path))
    identity(value, head)
    run = regular(value.get("runDirectory", ""))
    require(run.is_dir() and Path(receipt_path).parent == run, "receipt/run directory mismatch")
    lower, upper = utc(started), utc(value.get("finishedUtc"))
    require(lower <= utc(value.get("startedUtc")) <= upper, "stale inner receipt")
    logs = [regular(value["log"])] if "log" in value else [regular(p) for p in run.glob("*.log")]
    require(1 <= len(logs) <= 4, "bounded complete fresh logs required")
    all_text = ""
    total_bytes = 0
    for path in logs:
        require(path.parent == run and path.is_file() and 0 < path.stat().st_size <= 64 * 1024**2, "missing/oversized log")
        require(datetime.fromtimestamp(path.stat().st_mtime, timezone.utc) >= lower, "stale log")
        with path.open("rb") as stream:
            raw = stream.read(64 * 1024**2 + 1)
        total_bytes += len(raw)
        require(total_bytes <= 64 * 1024**2, "aggregate log cap")
        all_text += raw.decode("utf-8-sig", errors="strict")
    if "requestedViewport" in value:
        width, height = viewport.split("x")
        marker = rf"PROOF_INTERACTIVE_READY owner=\d+ width={width} height={height} maxFPS=30\.000 physicalInput=unverified"
        require(len(re.findall(marker, all_text)) == 1, "actual ready viewport marker required exactly once")
    if "screenshots" in value:
        for shot in value["screenshots"]:
            path = regular(shot.get("path", ""))
            require(path.parent == run / "screenshots" and path.name == shot.get("name") and path.is_file(), "capture path mismatch")
            require(1024 < path.stat().st_size <= 32 * 1024**2, "bounded PNG required")
            raw = path.read_bytes()
            require(1024 < len(raw) <= 32 * 1024**2 and type(shot.get("bytes")) is int
                    and len(raw) == shot["bytes"], "capture byte count mismatch")
            require(raw[:8] == b"\x89PNG\r\n\x1a\n" and len(raw) >= 24, "PNG required")
            require(struct.unpack(">II", raw[16:24]) == tuple(map(int, viewport.split("x"))), "actual PNG dimensions mismatch")
            require(type(shot.get("sha256")) is str and hashlib.sha256(raw).hexdigest() == shot["sha256"].lower(), "capture hash mismatch")
            require(datetime.fromtimestamp(path.stat().st_mtime, timezone.utc) >= lower, "stale capture")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["interactive", "native", "render", "bindings", "prerequisites", "build-reference", "artifacts", "outcome"])
    parser.add_argument("receipt")
    parser.add_argument("--head", required=True)
    parser.add_argument("--viewport", choices=["1280x720", "960x540"], default="1280x720")
    parser.add_argument("--sha256")
    parser.add_argument("--review")
    parser.add_argument("--frequency", type=int)
    parser.add_argument("--root")
    parser.add_argument("--started")
    parser.add_argument("--stage", choices=list(BOUNDS))
    args = parser.parse_args()
    if args.mode == "bindings":
        binding = verify_bindings(args.receipt, args.sha256, args.review)
        require(binding["sourceHead"] == args.head, "wrong requested head")
        print(json.dumps({"valid": True, "runtimeAcceptance": False}))
    elif args.mode == "prerequisites":
        prerequisites = file_ref({"path": args.receipt, "sha256": args.sha256})
        validate_prerequisites(prerequisites, args.head, datetime.now(timezone.utc), args.frequency)
        print(json.dumps({"valid": True, "runtimeAcceptance": False}))
    elif args.mode == "build-reference":
        validate_build_reference(read(args.receipt), args.head, args.root, args.frequency)
        print(json.dumps({"valid": True, "runtimeAcceptance": False}))
    elif args.mode == "artifacts":
        validate_artifacts(args.receipt, args.head, args.viewport, args.started)
        print(json.dumps({"valid": True, "runtimeAcceptance": False}))
    elif args.mode == "outcome":
        validate_outcome(read(args.receipt), args.stage, args.frequency)
        print(json.dumps({"valid": True, "runtimeAcceptance": False}))
    else:
        print(json.dumps(validate_payload(args.mode, read(args.receipt), args.head, args.viewport, args.frequency)))


if __name__ == "__main__":
    release_own_console()
    main()
