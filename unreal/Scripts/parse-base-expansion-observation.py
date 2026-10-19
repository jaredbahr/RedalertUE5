"""Fail-closed, read-only check of one opt-in three-role base-expansion run.

Visible-state equality is checked wherever revisions overlap. Replication may
coalesce revisions: command pre/post states need not each become a checkpoint.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
from pathlib import Path

MARKER = "PROOF_BASE_EXPANSION_OBSERVATION "
ORDINARY = re.compile(
    r"PROOF_SNAPSHOT role=(host|client) owner=(\d+) epoch=(\d+) revision=(\d+) digest=([0-9a-f]{32})"
)
HEX32 = re.compile(r"^[0-9a-f]{32}$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
SOURCE_HEAD = "d1639667f520253e508b1192295e52e47f90399a"
RADIUS = 800.0
SEPARATION = 180.0
WORLD_LIMIT = 10000.0
MAX_INT32 = 2147483647
COMMON_FIELDS = {"v", "run", "source", "role", "viewer", "event", "row"}
BASE_MARKERS = {
    "PROOF_BASE_EXPANSION_READY", "PROOF_BASE_EXPANSION_SEND",
    "PROOF_BASE_EXPANSION_REJECTION_RECEIPT", "PROOF_BASE_EXPANSION_COMPLETE",
}


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"nonfinite JSON constant: {value}")


def integer(value, minimum=0):
    return type(value) is int and minimum <= value <= MAX_INT32


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def point(value):
    require(isinstance(value, dict) and set(value) == {"x", "y", "z"}, "invalid point")
    require(all(number(value[axis]) and abs(value[axis]) <= WORLD_LIMIT
                for axis in ("x", "y", "z")), "nonfinite/out-of-world point")
    return value


def distance_squared(a, b):
    return sum((a[axis] - b[axis]) ** 2 for axis in ("x", "y", "z"))


def distance(a, b):
    return math.sqrt(distance_squared(a, b))


def state(value):
    require(isinstance(value, dict) and set(value) == {
        "epoch", "revision", "digest", "skirmish", "phase", "winner", "players", "units", "resources"
    }, "invalid state schema")
    require(integer(value["epoch"], 1) and integer(value["revision"], 1), "invalid epoch/revision")
    require(isinstance(value["digest"], str) and HEX32.fullmatch(value["digest"]), "invalid digest")
    require(value["skirmish"] is True and integer(value["phase"]) and value["phase"] in (0, 1, 2)
            and integer(value["winner"], -1), "invalid match state")
    require(isinstance(value["players"], list) and isinstance(value["units"], list)
            and isinstance(value["resources"], list), "invalid arrays")
    owners, unit_ids, resource_ids = set(), set(), set()
    for record in value["players"]:
        require(isinstance(record, dict) and set(record) == {"owner", "team", "credits", "gathered"}, "invalid player")
        require(integer(record["owner"], 1) and integer(record["team"], -1)
                and integer(record["credits"]) and integer(record["gathered"]), "invalid player values")
        require(record["owner"] not in owners, "duplicate player")
        owners.add(record["owner"])
    for unit in value["units"]:
        require(isinstance(unit, dict) and set(unit) == {
            "id", "owner", "kind", "hp", "position", "direction", "cargo", "rally", "rallyPoint",
            "guard", "post", "target", "returning"
        }, "invalid unit schema")
        require(integer(unit["id"], 1) and integer(unit["owner"], 1) and integer(unit["kind"])
                and unit["kind"] in (0, 1, 2, 3, 4) and number(unit["hp"]) and 0 <= unit["hp"] <= 10000
                and integer(unit["cargo"]) and type(unit["rally"]) is bool and type(unit["guard"]) is bool
                and type(unit["returning"]) is bool and integer(unit["target"], -1), "invalid unit values")
        for field in ("position", "direction", "rallyPoint", "post"):
            point(unit[field])
        require(unit["id"] not in unit_ids, "duplicate unit")
        unit_ids.add(unit["id"])
    for resource in value["resources"]:
        require(isinstance(resource, dict) and set(resource) == {"id", "owner", "position", "remaining"}, "invalid resource")
        require(integer(resource["id"], 1) and integer(resource["owner"], -1)
                and integer(resource["remaining"]), "invalid resource values")
        point(resource["position"])
        require(resource["id"] not in resource_ids, "duplicate resource")
        resource_ids.add(resource["id"])
    return value


def player(snapshot, owner):
    matches = [record for record in snapshot["players"] if record["owner"] == owner]
    require(len(matches) == 1, f"player {owner} missing/duplicate")
    return matches[0]


def units(snapshot, *, owner=None, kind=None, live=False):
    return [record for record in snapshot["units"] if
            (owner is None or record["owner"] == owner) and
            (kind is None or record["kind"] == kind) and
            (not live or record["hp"] > 0)]


def structures(snapshot, owner=None):
    return [u for u in units(snapshot, owner=owner, live=True) if u["kind"] in (2, 3, 4)]


def expected_spawn(snapshot, anchor):
    """Independent reproduction of the existing fixed ordered search, pre-build."""
    for ring in (1, 2, 3):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)):
            candidate = {"x": anchor["x"] + dx * 300 * ring,
                         "y": anchor["y"] + dy * 300 * ring, "z": anchor["z"]}
            if all(abs(v) <= WORLD_LIMIT for v in candidate.values()) and all(
                    distance_squared(candidate, u["position"]) >= 260 ** 2 for u in units(snapshot, live=True)):
                return candidate
    raise ValueError("refinery grant has no legal spawn")


def new_unit(unit_id, kind, position):
    zero = {"x": 0, "y": 0, "z": 0}
    return {"id": unit_id, "owner": 2, "kind": kind, "hp": 100 if kind == 1 else 200,
            "position": position, "direction": zero, "cargo": 0, "rally": False,
            "rallyPoint": zero, "guard": False, "post": zero, "target": -1, "returning": False}


def build_preconditions(command, records, cost):
    before = command["before"]
    require(before["phase"] == 1 and before["winner"] == -1, "build requires unfinished Playing match")
    require(units(before, owner=2, kind=2, live=True), "build requires live own HQ")
    require(player(before, 2)["credits"] >= cost, "insufficient build credits")
    require(len(units(before, owner=2, live=True)) <= 12 - records, "owner record capacity")
    # This bounded fixture has no teardown; allocated IDs remain visible, even when dead.
    next_id = max((u["id"] for u in before["units"]), default=0) + 1
    require(next_id <= MAX_INT32 - records, "build ID capacity")
    require(all(distance_squared(command["destination"], u["position"]) >= SEPARATION ** 2
                for u in structures(before)), "build structure overlap")
    return next_id


def accepted_build(command, kind, cost):
    before, after = command["before"], command["after"]
    next_id = build_preconditions(command, 2 if kind == 3 else 1, cost)
    created = [new_unit(next_id, kind, command["destination"])]
    if kind == 3:
        # Auto-Gather stores a hidden order; direction stays zero until an Advance moves it.
        created.append(new_unit(next_id + 1, 1, expected_spawn(before, command["destination"])))
    expected = copy.deepcopy(before)
    player(expected, 2)["credits"] -= cost
    expected["units"].extend(created)
    expected["revision"] += len(created) + 1
    expected["digest"] = after["digest"]
    require(expected == after, "accepted build changed unexpected state/defaults/IDs/spawn/economy")
    return created[0]


def fields(payload):
    words = payload.split()
    require(words and all(word.count("=") == 1 and all(word.split("=")) for word in words), "malformed proof marker")
    return unique_object(word.split("=", 1) for word in words)


def read_role(path: Path, *, role: str, viewer: int, run: str, source: str):
    require(path.is_file() and path.stat().st_size <= 32 * 1024 * 1024, f"missing/oversized {role} log")
    text = path.read_text(encoding="utf-8", errors="strict")
    rows, snapshots, commands, ordinary, markers = [], {}, [], {}, {}
    expected_role = "host" if viewer == 0 else "client"
    finish = None
    last_snapshot = None
    host_revision = 0
    epoch = None
    for line in text.splitlines():
        if "PROOF_SNAPSHOT" in line:
            payload = line[line.index("PROOF_SNAPSHOT"):].strip()
            match = ORDINARY.fullmatch(payload)
            require(match, f"{role} malformed ordinary snapshot")
            if match.group(1) == expected_role and int(match.group(2)) == viewer:
                key = (int(match.group(3)), int(match.group(4)))
                digest = match.group(5)
                require(key not in ordinary or ordinary[key] == digest, f"{role} conflicting ordinary digest")
                ordinary[key] = digest
        if "PROOF_BASE_EXPANSION" in line:
            payload = line[line.index("PROOF_BASE_EXPANSION"):]
            name, separator, rest = payload.partition(" ")
            require(separator and name in BASE_MARKERS | {MARKER.strip()}, f"{role} malformed/error expansion marker")
            if name != MARKER.strip():
                require(viewer == 2, f"{role} unexpected observer fixture marker")
                markers.setdefault(name, []).append(fields(rest))
                continue
            require(finish is None, f"{role} observation after finish")
            require(len(rest) <= 65536, f"{role} oversized row")
            row = json.loads(rest, object_pairs_hook=unique_object, parse_constant=reject_constant)
            require(isinstance(row, dict) and integer(row.get("v"), 1) and row["v"] == 1
                    and row.get("run") == run and row.get("source") == source and row.get("role") == expected_role
                    and integer(row.get("viewer")) and row["viewer"] == viewer
                    and integer(row.get("row"), 1) and row["row"] == len(rows) + 1, f"{role} unbound row")
            event = row.get("event")
            require(event in ("snapshot", "command", "finish"), f"{role} invalid event")
            if last_snapshot is not None and last_snapshot["phase"] == 2:
                require(event == "finish", f"{role} missing immediate terminal finish")
            if event == "snapshot":
                require(set(row) == COMMON_FIELDS | {"state"}, "snapshot schema")
                snap = state(row["state"])
                key = (snap["epoch"], snap["revision"])
                require(last_snapshot is None or snap["revision"] > last_snapshot["revision"], f"{role} nonmonotone snapshots")
                if viewer == 0:
                    require(snap["revision"] >= host_revision, "host Publish regressed behind command")
                    host_revision = snap["revision"]
                snapshots[key] = snap
                last_snapshot = snap
                row_epoch = snap["epoch"]
            elif event == "command":
                require(viewer == 0, "client command observation")
                require(set(row) == COMMON_FIELDS | {
                    "owner", "type", "sequence", "payloadEpoch", "unit", "target", "destination",
                    "accepted", "reason", "before", "after"}, "command schema")
                require(integer(row["owner"], 1) and row["owner"] == 2 and integer(row["sequence"], 1)
                        and integer(row["payloadEpoch"], 1) and integer(row["type"]) and row["type"] in (3, 4)
                        and type(row["accepted"]) is bool and isinstance(row["reason"], str)
                        and integer(row["unit"], -1) and row["unit"] == -1
                        and integer(row["target"], -1) and row["target"] == -1, "invalid command")
                point(row["destination"])
                state(row["before"]); state(row["after"])
                require(row["before"]["epoch"] == row["after"]["epoch"] == row["payloadEpoch"], "command epoch")
                require(host_revision <= row["before"]["revision"] <= row["after"]["revision"], "host command chronology")
                host_revision = row["after"]["revision"]
                row_epoch = row["payloadEpoch"]
                commands.append(row)
            else:
                require(set(row) == COMMON_FIELDS | {"epoch", "revision", "digest", "snapshots", "commands"}, "finish schema")
                require(integer(row["epoch"], 1) and integer(row["revision"], 1)
                        and isinstance(row["digest"], str) and HEX32.fullmatch(row["digest"])
                        and integer(row["snapshots"]) and row["snapshots"] == len(snapshots)
                        and integer(row["commands"]) and row["commands"] == len(commands), "invalid finish identity/counts")
                require(rows and rows[-1]["event"] == "snapshot" and last_snapshot["phase"] == 2
                        and last_snapshot["winner"] == 0, "finish needs last terminal winning snapshot")
                require(all(row[key] == last_snapshot[key] for key in ("epoch", "revision", "digest")), "finish/snapshot mismatch")
                row_epoch = row["epoch"]
                finish = row
            require(epoch is None or epoch == row_epoch, f"{role} mixed epochs")
            epoch = row_epoch
            rows.append(row)
        elif any(name in line for name in ("PROOF_SKIRMISH_COMPLETE", "PROOF_HOST_RESULT", "PROOF_RESULT")):
            for name in ("PROOF_SKIRMISH_COMPLETE", "PROOF_HOST_RESULT", "PROOF_RESULT"):
                if name in line:
                    payload = line[line.index(name) + len(name):]
                    require(payload.startswith(" "), f"{role} malformed {name}")
                    parsed = fields(payload)
                    if name in ("PROOF_HOST_RESULT", "PROOF_RESULT"):
                        require(set(parsed) == {"owner", "sequence", "accepted", "reason"}
                                and all(re.fullmatch(r"[1-9][0-9]*", parsed[key])
                                        and int(parsed[key]) <= MAX_INT32 for key in ("owner", "sequence"))
                                and parsed["accepted"] in ("0", "1"), f"{role} malformed receipt")
                    markers.setdefault(name, []).append(parsed)
                    break
    require(0 < len(rows) <= 8192 and len(snapshots) >= 5, f"{role} insufficient/too many rows")
    require(finish is not None, f"{role} missing finish")
    require(all(ordinary.get(key) == snap["digest"] for key, snap in snapshots.items()), f"{role} snapshot/digest mismatch")
    return {"snapshots": snapshots, "commands": commands, "rows": rows, "finish": finish, "markers": markers}


def verify_fixture_markers(roles, commands, epoch):
    for role, record in roles.items():
        completions = record["markers"].get("PROOF_SKIRMISH_COMPLETE", [])
        require(len(completions) == 1, f"{role} missing/duplicate ordinary completion")
        completion = completions[0]
        expected = {"role": "host" if role == "host" else "client" if role == "rendered" else "observer",
                    "status": "pass", "scenario": "Coop", "winnerTeam": "0"}
        if role != "host":
            expected["owner"] = "1" if role == "rendered" else "2"
        require(all(completion.get(k) == v for k, v in expected.items()), "ordinary match regression failed")
    markers = roles["observer"]["markers"]
    require(markers.get("PROOF_BASE_EXPANSION_COMPLETE") == [{
        "role": "observer", "owner": "2", "status": "pass", "rejectionVerified": "1"}], "missing/duplicate/failed expansion completion")
    require(markers.get("PROOF_BASE_EXPANSION_REJECTION_RECEIPT") == [{
        "owner": "2", "epoch": str(epoch), "sequence": "3", "accepted": "0", "reason": "placement"}], "fixture rejection receipt mismatch")
    ready = markers.get("PROOF_BASE_EXPANSION_READY", [])
    require(len(ready) <= 1, "duplicate fixture readiness")
    if ready:
        require(set(ready[0]) == {"owner", "epoch", "revision"} and ready[0]["owner"] == "2"
                and ready[0]["epoch"] == str(epoch) and ready[0]["revision"].isdigit()
                and 0 < int(ready[0]["revision"]) <= commands[0]["before"]["revision"], "invalid fixture readiness")
    sends = markers.get("PROOF_BASE_EXPANSION_SEND", [])
    require(len(sends) == 3, "three public RPC sends")
    for send, command, kind in zip(sends, commands, ("refinery", "factory", "rejected-refinery")):
        require(set(send) == {"owner", "kind", "epoch", "sequence", "x", "y", "z"}
                and send["owner"] == "2" and send["kind"] == kind and send["epoch"] == str(epoch)
                and send["sequence"] == str(command["sequence"]), "client intent/host command mismatch")
        require(all(math.isfinite(float(send[axis])) and math.isclose(float(send[axis]), command["destination"][axis],
                    rel_tol=0, abs_tol=0.01) for axis in ("x", "y", "z")), "client destination mismatch")
    for role, name in (("observer", "PROOF_RESULT"), ("host", "PROOF_HOST_RESULT")):
        receipts = roles[role]["markers"].get(name, [])
        # Other owners and the observer's subsequent Gather/Produce receipts are ordinary play.
        relevant = [r for r in receipts if r.get("owner") == "2" and r.get("sequence") in ("1", "2", "3")]
        require(relevant == [{"owner": "2", "sequence": str(i), "accepted": str(int(i != 3)),
                              "reason": "placement" if i == 3 else "accepted"} for i in (1, 2, 3)], "host/client receipts mismatch")


def verify(run_dir: Path, result_path: Path, diagnostic_head: str, source_tree: str):
    require(HEX40.fullmatch(diagnostic_head) and HEX40.fullmatch(source_tree), "expected identity format")
    result = json.loads(result_path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object, parse_constant=reject_constant)
    require(isinstance(result, dict) and result.get("ordinaryPassed") is True and type(result.get("passed")) is bool
            and result.get("scenario") == "Coop" and result.get("head") == diagnostic_head
            and diagnostic_head != SOURCE_HEAD and result.get("sourceTree") == source_tree
            and result.get("baseExpansionEvidenceEnabled") is True and result.get("dirtyWorkingTree") is False,
            "diagnostic result identity/status")
    run = result.get("baseExpansionRun")
    require(isinstance(run, str) and HEX32.fullmatch(run), "run token")
    roles = {name: read_role(run_dir / (name + ".log"), role=name, viewer=viewer, run=run, source=source_tree)
             for name, viewer in (("host", 0), ("rendered", 1), ("observer", 2))}
    commands = roles["host"]["commands"]
    require(len(commands) == 3, "expected exactly three owner-2 build submissions")
    first, second, rejected = commands
    require([item["type"] for item in commands] == [3, 4, 3]
            and [item["sequence"] for item in commands] == [1, 2, 3]
            and [item["accepted"] for item in commands] == [True, True, False]
            and [item["reason"] for item in commands] == ["accepted", "accepted", "placement"], "command order/outcomes")
    epoch = first["payloadEpoch"]
    by_revision = {}
    for record in roles.values():
        require(record["finish"]["epoch"] == epoch, "cross-role epoch mismatch")
        for row in record["rows"]:
            observed = [row["state"]] if row["event"] == "snapshot" else [row["before"], row["after"]] if row["event"] == "command" else []
            for snapshot in observed:
                key = (snapshot["epoch"], snapshot["revision"])
                require(snapshot["epoch"] == epoch, "noncanonical state epoch")
                require(key not in by_revision or by_revision[key] == snapshot, f"observed state conflict at {key}")
                by_revision[key] = snapshot
    hqs = units(first["before"], owner=2, kind=2, live=True)
    require(len(hqs) == 1, "owner-2 construction HQ")
    hq = hqs[0]
    require(distance_squared(first["destination"], hq["position"]) <= RADIUS ** 2, "first refinery outside own HQ")
    refinery = accepted_build(first, 3, 100)
    hqs2 = units(second["before"], owner=2, kind=2, live=True)
    refs2 = [u for u in units(second["before"], owner=2, kind=3, live=True) if u["id"] == refinery["id"]]
    require(len(hqs2) == len(refs2) == 1, "live own HQ/refinery before expanded build")
    require(refs2[0]["position"] == refinery["position"], "refinery anchor moved")
    require(distance_squared(second["destination"], hqs2[0]["position"]) > RADIUS ** 2
            and distance_squared(second["destination"], refs2[0]["position"]) <= RADIUS ** 2,
            "factory not genuinely extended from owned refinery")
    factory = accepted_build(second, 4, 150)
    build_preconditions(rejected, 2, 100)
    require(rejected["before"] == rejected["after"], "rejection mutated complete state")
    require(all(distance_squared(rejected["destination"], u["position"]) > RADIUS ** 2
                for u in structures(rejected["before"], owner=2)), "rejected site reached own live structure")
    require(any(distance_squared(rejected["destination"], u["position"]) <= RADIUS ** 2
                for u in structures(rejected["before"]) if u["owner"] != 2), "rejected site lacks foreign anchor")
    # Isolate reach from a hypothetical grant failure as well as the earlier preconditions.
    expected_spawn(rejected["before"], rejected["destination"])
    verify_fixture_markers(roles, commands, epoch)
    common = sorted(set(roles["host"]["snapshots"]).intersection(roles["rendered"]["snapshots"], roles["observer"]["snapshots"]))
    require(len(common) >= 5, "insufficient three-role checkpoints")
    accepted_common = [key for key in common if key[1] >= second["after"]["revision"]
                       and roles["host"]["snapshots"][key]["phase"] == 1 and any(
        u["id"] == factory["id"] and u["owner"] == 2 and u["kind"] == 4
        and u["position"] == factory["position"] and u["hp"] > 0
        for u in roles["host"]["snapshots"][key]["units"])]
    require(accepted_common, "no published common expanded factory state")
    digest_lines = [f"{e}:{revision}:{roles['host']['snapshots'][(e, revision)]['digest']}" for e, revision in common]
    return {
        "passed": True, "productionHead": SOURCE_HEAD, "diagnosticHead": result["head"],
        "diagnosticSourceTree": source_tree, "run": run, "epoch": epoch,
        "commandSequences": [item["sequence"] for item in commands],
        "owner2HQ": {"id": hq["id"], "position": hq["position"]},
        "refinery": {"id": refinery["id"], "position": refinery["position"]},
        "factory": {"id": factory["id"], "position": factory["position"]},
        "factoryDistanceFromHQ": distance(second["destination"], hqs2[0]["position"]),
        "factoryDistanceFromRefinery": distance(second["destination"], refs2[0]["position"]),
        "rejectedDestination": rejected["destination"], "rejectedReason": rejected["reason"],
        "rejectedStateUnchanged": True, "commonCheckpoints": len(common),
        "commonDigestSeriesSha256": hashlib.sha256(("\n".join(digest_lines) + "\n").encode()).hexdigest().upper(),
        "firstCommonFactoryRevision": accepted_common[0][1],
        "terminals": {role: {key: record["finish"][key] for key in ("epoch", "revision", "digest")}
                      for role, record in roles.items()},
        "roleCounts": {role: {key: record["finish"][key] for key in ("snapshots", "commands")}
                       for role, record in roles.items()},
        "proofScope": "synthetic owner-2 public RPC through host model and three-role loopback replication through each role's first-terminal boundary; lifecycle/teardown and physical R/F unverified",
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--diagnostic-head", required=True)
    parser.add_argument("--source-tree", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    evidence = verify(args.run_dir, args.result, args.diagnostic_head, args.source_tree)
    payload = json.dumps(evidence, indent=2) + "\n"
    if args.output:
        args.output.write_text(payload, encoding="utf-8")
    print(payload, end="")


if __name__ == "__main__":
    main()
