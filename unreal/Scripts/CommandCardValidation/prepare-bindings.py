"""Create a source-only review bundle; never launch a build or mark review approved."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from policy import RESOURCE_PINS, authored_assets, committed_manifest, release_own_console


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", required=True)
    p.add_argument("--runtime", required=True)
    p.add_argument("--engine", required=True)
    p.add_argument("--pwsh", required=True)
    p.add_argument("--python", required=True)
    p.add_argument("--git", required=True)
    p.add_argument("--assets", required=True)
    p.add_argument("--readiness", required=True)
    p.add_argument("--admission", required=True)
    p.add_argument("--watchdog", required=True)
    p.add_argument("--output", required=True)
    a = p.parse_args()
    root = Path(a.source).resolve()
    def git(*args):
        return subprocess.check_output([a.git, "--no-optional-locks", "-C", str(root), *args], timeout=10).decode().strip()
    monitor = subprocess.run([a.git, "-C", str(root), "config", "--get", "core.fsmonitor"], capture_output=True, text=True, timeout=10)
    if monitor.returncode not in (0, 1) or monitor.stdout.strip().lower() not in ("", "false", "no", "0"):
        raise ValueError("Unsupported fsmonitor configuration")
    if git("status", "--porcelain"):
        raise ValueError("Commit the exact candidate before preparing bindings")
    head = git("rev-parse", "HEAD")
    manifest = committed_manifest(root, a.git, head)
    pins = [{"relative": item["relative"], "bytes": (root / item["relative"]).stat().st_size,
             "sha256": digest(root / item["relative"]), "blobOid": item["blobOid"]}
            for item in manifest]
    assets = json.loads(Path(a.assets).read_text(encoding="utf-8-sig"))
    if assets != authored_assets():
        raise ValueError("Exact intended authored-content set required")
    prepared_path = Path(a.watchdog)
    prepared = json.loads(prepared_path.read_text(encoding="utf-8-sig"))
    if prepared.get("head") != head:
        raise ValueError("Prepare the watchdog from this exact committed candidate")
    binaries = {"pwsh": Path(a.pwsh), "python": Path(a.python), "git": Path(a.git),
                "editor": Path(a.engine) / "Engine/Binaries/Win64/UnrealEditor.exe",
                "editorCmd": Path(a.engine) / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"}
    resources = {}
    for key in RESOURCE_PINS:
        path = Path(getattr(a, key))
        if digest(path) != RESOURCE_PINS[key]:
            raise ValueError("Original resource-only sampler changed: " + key)
        resources[key] = {"path": str(path), "sha256": RESOURCE_PINS[key]}
    binding = dict(scope="PR72", sourceHead=head,
                   cppTree=git("rev-parse", "HEAD:unreal/Source/RedalertProof"),
                   sourceRoot=str(Path(a.runtime)), storageRoot=str(Path(a.runtime).parent),
                   engineRoot=a.engine, files=pins, committedManifest=manifest,
                   executables={k: {"path": str(v), "sha256": digest(v)} for k, v in binaries.items()},
                   resourceTools=resources,
                   assets=assets, preparedWatchdog={"path": str(prepared_path), "sha256": digest(prepared_path)},
                   preparationOnly=True, acceptanceEvidence=False)
    raw = json.dumps(binding, indent=2).encode()
    if len(raw) > 262144:
        raise ValueError("binding metadata cap")
    output = Path(a.output)
    with output.open("xb") as stream:
        stream.write(raw)
    review = dict(approved=False, bindingsSha256=hashlib.sha256(raw).hexdigest(),
                  sourceHead=binding["sourceHead"], reviewType="independent", reviewer="",
                  note="Pending independent review; this is not an approval receipt.")
    with output.with_name("REVIEW_REQUIRED.json").open("x", encoding="utf-8") as stream:
        json.dump(review, stream, indent=2)
    print(json.dumps({"bindings": str(output), "sha256": review["bindingsSha256"],
                      "reviewApproved": False, "runtimeMaterialized": False}))


if __name__ == "__main__":
    release_own_console()
    main()
