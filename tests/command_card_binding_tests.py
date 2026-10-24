"""In-memory negative controls; no engine, process signalling or qualification."""
import copy
import importlib.util
import hashlib
import json
import struct
import tempfile
from datetime import datetime, timedelta, timezone
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("card_policy", ROOT / "unreal/Scripts/CommandCardValidation/policy.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)
HEAD = "1" * 40
NOW = datetime(2026, 1, 1, 0, 20, tzinfo=timezone.utc)
FREQ = 10_000_000


def stamp(value):
    return value.isoformat()


def session():
    return dict(head=HEAD, requestedViewport="1280x720", expired=False, error=None,
                exitCode=0, sessionLimitSeconds=180, affinityMask=68, priority="Normal",
                uePid=42, ueCreationFileTime=134000000000000000,
                launchStartQpc=100, exitObservedQpc=100 + 10 * FREQ, qpcFrequency=FREQ,
                windowHandle=55, policyPolls=5, physicalInputVerified=False, humanConfirmationRequired=True,
                cleanup=dict(nativeExitConfirmed=True, terminationRequested=False, elapsedSeconds=.1, error=None),
                startedUtc=stamp(NOW - timedelta(seconds=15)), finishedUtc=stamp(NOW))


def outcome(mode="interactive"):
    return dict(stem=mode, boundSeconds=policy.BOUNDS[mode], passed=True, resumed=True, jobAssigned=True,
                cleanupSucceeded=True, outputComplete=True, timedOut=False, reservationLost=False,
                leaseCancelled=False, terminationRequested=False, error=None, cleanupError=None,
                exitCode=0, activeOwnedProcessesAfterCleanup=0, pid=99, creationFileTime=123,
                frequency=FREQ, deadlineStartQpc=100, observedExitQpc=200, completedQpc=300,
                stdoutBytes=5, stderrBytes=0, startedUtc=stamp(NOW - timedelta(seconds=1)), finishedUtc=stamp(NOW))


def ready():
    start = NOW - timedelta(seconds=610)
    rows = [dict(utc=stamp(start + timedelta(seconds=i * 5)), aggregate=10, cpu2=15, cpu6=20,
                 conflicts=0, games=0, otherValidationProcesses=0, freeMemoryKiB=8 * 1024**2,
                 freeDBytes=20 * 1024**3, error=None) for i in range(121)]
    return dict(eligible=True, acceptanceEvidence=False, error=None, frozenHead=policy.FROZEN_HEAD,
                elapsedSeconds=601.0, samples=rows, utc=stamp(NOW - timedelta(seconds=9)))


def lease():
    return dict(head=HEAD, ownedLanesDrained=True, createdUtc=stamp(NOW - timedelta(seconds=8)),
                expiresUtc=stamp(NOW + timedelta(seconds=1192)), cancelPath=str(ROOT / 'generated/cancel.fixture'))


def admission():
    rows = [dict(utc=stamp(NOW - timedelta(seconds=i)), aggregate=10, cpu2=20, cpu6=30) for i in (6, 5, 4)]
    return dict(eligible=True, clean=True, head=policy.FROZEN_HEAD, mask="0x44", priority="Normal",
                conflictsBefore=0, conflictsAfter=0, gamesBefore=0, gamesAfter=0,
                freeMemoryKiB=8 * 1024**2, freeDBytes=20 * 1024**3, samples=rows,
                utc=stamp(NOW - timedelta(seconds=3)))


class Controls(unittest.TestCase):
    def reject_session(self, value):
        with self.assertRaises(policy.Rejected):
            policy.validate_session(value, HEAD, "1280x720", FREQ)

    def test_normal_and_both_viewports(self):
        for viewport in ("1280x720", "960x540"):
            value = session(); value["requestedViewport"] = viewport
            self.assertFalse(policy.validate_session(value, HEAD, viewport, FREQ)["physicalInputVerified"])

    def test_exit_and_boolean_types(self):
        for key, values in {
            "exitCode": (None, False, True, "0", 0.0, 1, -1),
            "expired": (None, 0, "false", True),
            "sessionLimitSeconds": (None, True, "180", 180.0, 181),
            "affinityMask": (None, True, "68", 68.0, 3),
            "windowHandle": (None, False, "55", 0),
            "policyPolls": (None, False, "5", 0),
            "error": (False, 0, "", "failure"),
            "qpcFrequency": (None, True, FREQ + 1),
        }.items():
            for bad in values:
                with self.subTest(key=key, bad=bad):
                    value = session(); value[key] = bad; self.reject_session(value)
            value = session(); del value[key]; self.reject_session(value)

    def test_wrong_head_viewport_and_identity(self):
        for key, bad in (("head", "2" * 40), ("requestedViewport", "960x540"),
                         ("uePid", 0), ("ueCreationFileTime", False), ("priority", "High")):
            value = session(); value[key] = bad; self.reject_session(value)

    def test_launch_deadline(self):
        for offset in (-1, 0, 1):
            value = session(); value["exitObservedQpc"] = value["launchStartQpc"] + 180 * FREQ + offset
            if offset < 0:
                policy.validate_session(value, HEAD, "1280x720", FREQ)
            else:
                self.reject_session(value)
        value = session(); value["exitObservedQpc"] = 99; self.reject_session(value)

    def test_cleanup_failure_and_survivors(self):
        for key, bad in (("nativeExitConfirmed", False), ("terminationRequested", True),
                         ("elapsedSeconds", 5.001), ("elapsedSeconds", False), ("error", "failure")):
            value = session(); value["cleanup"][key] = bad; self.reject_session(value)
        value = session(); value.pop("cleanup"); self.reject_session(value)

    def test_strict_json(self):
        for raw in (b'', b'[]', b'{', b'{"exitCode":0,"exitCode":1}', b'{"x":NaN}',
                    b'{"x":Infinity}', b'\xff', b' ' * (1024 * 1024 + 1),
                    b'{"x":' * 18 + b'0' + b'}' * 18):
            with self.subTest(raw=raw[:40]), self.assertRaises(policy.Rejected):
                policy.loads(raw)
        self.assertEqual(policy.loads(b'{"x":0}'), {"x": 0})

    def test_outcome_negative_controls(self):
        for mode in policy.BOUNDS:
            policy.validate_outcome(outcome(mode), mode, FREQ)
        for key, bad in (("exitCode", 7), ("exitCode", False), ("passed", False),
                         ("jobAssigned", False), ("outputComplete", False), ("timedOut", True),
                         ("reservationLost", True), ("leaseCancelled", True), ("terminationRequested", True),
                         ("cleanupSucceeded", False), ("activeOwnedProcessesAfterCleanup", 1),
                         ("activeOwnedProcessesAfterCleanup", False), ("stderrBytes", 1024 * 1024 + 1),
                         ("frequency", FREQ + 1), ("frequency", True)):
            value = outcome(); value[key] = bad
            with self.subTest(key=key), self.assertRaises(policy.Rejected):
                policy.validate_outcome(value, "interactive", FREQ)
        for offset in (-1, 0, 1):
            value = outcome(); value["completedQpc"] = 100 + 220 * FREQ + offset
            if offset < 0:
                policy.validate_outcome(value, "interactive", FREQ)
            else:
                with self.assertRaises(policy.Rejected):
                    policy.validate_outcome(value, "interactive", FREQ)

    def test_full_quiet_window_and_thresholds(self):
        policy.validate_ready(ready(), NOW)
        for key, bad in (("aggregate", 80), ("cpu2", float('nan')), ("cpu6", True),
                         ("freeMemoryKiB", 8 * 1024**2 - 1), ("freeDBytes", 20 * 1024**3 - 1),
                         ("games", 1), ("conflicts", 1), ("otherValidationProcesses", 1)):
            value = ready(); value["samples"][60][key] = bad
            with self.subTest(key=key), self.assertRaises(policy.Rejected):
                policy.validate_ready(value, NOW)
        for edit in ("short", "gap", "future"):
            value = ready()
            if edit == "short": value["samples"] = value["samples"][1:]
            if edit == "gap": del value["samples"][30:32]
            if edit == "future": value["utc"] = stamp(NOW + timedelta(seconds=1))
            with self.subTest(edit=edit), self.assertRaises(policy.Rejected): policy.validate_ready(value, NOW)

    def test_qualification_before_reservation_and_fresh_dispatch(self):
        completed = policy.validate_ready(ready(), NOW)
        policy.validate_lease(lease(), completed, admission(), HEAD, NOW)
        for edit in ("before_qualification", "extended", "expired", "future", "no_drain", "stale", "early_admission"):
            value, dispatch = lease(), admission()
            if edit == "before_qualification": value["createdUtc"] = stamp(NOW - timedelta(seconds=20)); value["expiresUtc"] = stamp(NOW + timedelta(seconds=1180))
            if edit == "extended": value["expiresUtc"] = stamp(NOW + timedelta(seconds=1193))
            if edit == "expired": value["createdUtc"] = stamp(NOW - timedelta(seconds=1201)); value["expiresUtc"] = stamp(NOW - timedelta(seconds=1))
            if edit == "future": value["createdUtc"] = stamp(NOW + timedelta(seconds=1)); value["expiresUtc"] = stamp(NOW + timedelta(seconds=1201))
            if edit == "no_drain": value["ownedLanesDrained"] = False
            if edit == "stale": dispatch["utc"] = stamp(NOW - timedelta(seconds=61))
            if edit == "early_admission": dispatch["samples"][0]["utc"] = stamp(NOW - timedelta(seconds=9))
            with self.subTest(edit=edit), self.assertRaises(policy.Rejected):
                policy.validate_lease(value, completed, dispatch, HEAD, NOW)

    def temporary(self):
        directory = ROOT / "generated"
        directory.mkdir(exist_ok=True)
        context = tempfile.TemporaryDirectory(prefix="card-policy-", dir=directory)
        self.addCleanup(context.cleanup)
        return Path(context.name)

    def save_ref(self, root, name, value):
        path = root / name
        raw = json.dumps(value).encode()
        path.write_bytes(raw)
        return {"path": str(path), "sha256": hashlib.sha256(raw).hexdigest()}

    def test_receipt_files_missing_truncated_changed_and_oversized(self):
        root = self.temporary()
        reference = self.save_ref(root, "receipt.json", {"positive": True})
        self.assertTrue(policy.file_ref(reference)["positive"])
        for raw in (b'{', b'{}', b' ' * (1024 * 1024 + 1)):
            Path(reference["path"]).write_bytes(raw)
            with self.subTest(raw=raw[:5]), self.assertRaises(policy.Rejected): policy.file_ref(reference)
        Path(reference["path"]).unlink()
        with self.assertRaises(policy.Rejected): policy.file_ref(reference)

    def test_resource_receipts_bound_to_native_intervals(self):
        root = self.temporary()
        r, a = ready(), admission()
        r_outer, a_outer = outcome("qualification"), outcome("admission")
        r_outer.update(startedUtc=stamp(NOW - timedelta(seconds=611)), finishedUtc=r["utc"])
        a_outer.update(startedUtc=stamp(NOW - timedelta(seconds=7)), finishedUtc=a["utc"])
        values = dict(readiness=r, admission=a, lease=lease(), readinessOutcome=r_outer, admissionOutcome=a_outer)
        refs = {key: self.save_ref(root, key + ".json", value) for key, value in values.items()}
        policy.validate_prerequisites(refs, HEAD, NOW, FREQ)
        for key in refs:
            bad = dict(refs); bad.pop(key)
            with self.subTest(missing=key), self.assertRaises(policy.Rejected):
                policy.validate_prerequisites(bad, HEAD, NOW, FREQ)
        bad_outer = dict(r_outer, startedUtc=stamp(NOW - timedelta(seconds=10)))
        bad = dict(refs, readinessOutcome=self.save_ref(root, "late-native.json", bad_outer))
        with self.assertRaises(policy.Rejected): policy.validate_prerequisites(bad, HEAD, NOW, FREQ)
        bad_outer = dict(a_outer, terminationRequested=True)
        bad = dict(refs, admissionOutcome=self.save_ref(root, "forced-native.json", bad_outer))
        with self.assertRaises(policy.Rejected): policy.validate_prerequisites(bad, HEAD, NOW, FREQ)

    def test_actual_logs_and_captures_required(self):
        root = self.temporary()
        now = datetime.now(timezone.utc)
        started = stamp(now - timedelta(seconds=3))
        log = root / "interactive.log"
        log.write_text("PROOF_INTERACTIVE_READY owner=1 width=1280 height=720 maxFPS=30.000 physicalInput=unverified\n")
        value = session()
        value.update(runDirectory=str(root), log=str(log), startedUtc=started, finishedUtc=stamp(now))
        receipt = root / "session.json"
        receipt.write_text(json.dumps(value))
        policy.validate_artifacts(receipt, HEAD, "1280x720", started)
        for text in ("", "old log\n", log.read_text() * 2):
            log.write_text(text)
            with self.subTest(text=text[:20]), self.assertRaises(policy.Rejected):
                policy.validate_artifacts(receipt, HEAD, "1280x720", started)
        log.write_text("synthetic render log\n")
        image_dir = root / "screenshots"; image_dir.mkdir()
        raw = b'\x89PNG\r\n\x1a\n' + b'\0' * 8 + struct.pack('>II', 1280, 720) + b'\0' * 1100
        shots = []
        for name in ("01-base.png", "02-result.png", "03-left-edge.png", "04-right-edge.png",
                     "05-panel-edge.png", "06-camera-before.png", "07-camera-after.png"):
            path = image_dir / name; path.write_bytes(raw)
            shots.append(dict(name=name, path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        value = dict(head=HEAD, runDirectory=str(root), startedUtc=started, finishedUtc=stamp(now), screenshots=shots)
        receipt = root / "result.json"; receipt.write_text(json.dumps(value))
        policy.validate_artifacts(receipt, HEAD, "1280x720", started)
        with self.assertRaises(policy.Rejected): policy.validate_artifacts(receipt, HEAD, "960x540", started)
        Path(shots[0]["path"]).write_bytes(raw[:-1])
        with self.assertRaises(policy.Rejected): policy.validate_artifacts(receipt, HEAD, "1280x720", started)

    def test_build_requires_outer_outcome_and_actual_dll(self):
        root = self.temporary()
        binary = root / "unreal/Binaries/Win64"; binary.mkdir(parents=True)
        dll = binary / "UnrealEditor-RedalertProof.dll"; dll.write_bytes(b"INERT-DATA-NOT-EXECUTABLE")
        modules = binary / "UnrealEditor.modules"
        modules.write_text(json.dumps(dict(BuildId="synthetic", Modules=dict(RedalertProof=dll.name))))
        now = datetime.now(timezone.utc)
        start, end = stamp(now - timedelta(seconds=3)), stamp(now + timedelta(seconds=1))
        proof = dict(head=HEAD, case="Build", mode="build", sourceRoot=str(root), automatedStageValidated=True,
                     error=None, startedUtc=start, finishedUtc=end, buildId="synthetic",
                     dllSha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
                     modulesSha256=hashlib.sha256(modules.read_bytes()).hexdigest())
        outer = outcome("build"); outer.update(startedUtc=start, finishedUtc=end)
        refs = dict(build=self.save_ref(root, "PAYLOAD_RESULT.json", proof),
                    buildOutcome=self.save_ref(root, "RETURNED_OUTCOME.json", outer))
        policy.validate_build_reference(refs, HEAD, root, FREQ)
        for key in refs:
            bad = dict(refs); bad.pop(key)
            with self.subTest(missing=key), self.assertRaises(policy.Rejected):
                policy.validate_build_reference(bad, HEAD, root, FREQ)
        bad = dict(refs, buildOutcome=self.save_ref(root, "failed.json", dict(outer, exitCode=1)))
        with self.assertRaises(policy.Rejected): policy.validate_build_reference(bad, HEAD, root, FREQ)
        dll.write_bytes(b"CHANGED")
        with self.assertRaises(policy.Rejected): policy.validate_build_reference(refs, HEAD, root, FREQ)

    def test_exact_binding_review_and_source_changes(self):
        root = self.temporary()
        names = ["unreal/Scripts/CommandCardValidation/" + name for name in
                 ("policy.py", "run-stage.ps1", "stage-worker.ps1", "CommandCardStageSupervisor.cs")]
        names += ["unreal/Scripts/" + name for name in
                  ("build-native-proof.ps1", "test-hud-layout-model.ps1", "test-small-match.ps1",
                   "start-small-match.ps1", "start-local-interactive.ps1")]
        pins = []
        for name in names:
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"INERT SOURCE PIN")
            pins.append(dict(relative=name, bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        assets = []
        for index in range(14):
            relative = "inert/" + str(index) + ".uasset"
            path = root / "unreal/Content" / relative; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"INERT CONTENT")
            assets.append(dict(relative=relative, bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        exe = root / "INERT-NOT-EXECUTABLE"; exe.write_bytes(b"INERT")
        exe_ref = dict(path=str(exe), sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
        resource_pins = {key: exe_ref["sha256"] for key in policy.RESOURCE_PINS}
        value = dict(scope="PR72", sourceHead=HEAD, cppTree="2" * 40, sourceRoot=str(root),
                     engineRoot=str(root), storageRoot=str(root), files=pins, assets=assets,
                     executables={key: exe_ref for key in ("pwsh", "python", "git", "editor", "editorCmd")},
                     resourceTools={key: exe_ref for key in resource_pins})
        def check(candidate, approved=True):
            reference = self.save_ref(root, "BINDINGS.json", candidate)
            review = dict(approved=approved, bindingsSha256=reference["sha256"], sourceHead=HEAD,
                          reviewType="independent", reviewer="synthetic-control-only")
            review_ref = self.save_ref(root, "REVIEW.json", review)
            return policy.verify_bindings(reference["path"], reference["sha256"], review_ref["path"])
        # These are data-only fixtures. They cannot launch a real executable or
        # serve as an actual independent review, and the temporary tree is removed.
        with patch.dict(policy.RESOURCE_PINS, resource_pins, clear=True):
            check(value)
            with self.assertRaises(policy.Rejected): check(value, approved=False)
            for edit in ("missing_source", "duplicate_source", "traversal", "missing_asset", "wrong_scope", "wrong_head"):
                bad = copy.deepcopy(value)
                if edit == "missing_source": bad["files"].pop()
                if edit == "duplicate_source": bad["files"].append(bad["files"][0])
                if edit == "traversal": bad["files"][0]["relative"] = "../escaped"
                if edit == "missing_asset": bad["assets"].pop()
                if edit == "wrong_scope": bad["scope"] = "PR71"
                if edit == "wrong_head": bad["sourceHead"] = "3" * 40
                with self.subTest(edit=edit), self.assertRaises(policy.Rejected): check(bad)
            (root / names[0]).write_bytes(b"CHANGED SOURCE")
            with self.assertRaises(policy.Rejected): check(value)


if __name__ == "__main__":
    unittest.main(verbosity=2)
