"""Fail-closed, read-only check of one opt-in three-role base-expansion run."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path

MARKER = "PROOF_BASE_EXPANSION_OBSERVATION "
ORDINARY = re.compile(
    r"PROOF_SNAPSHOT role=(host|client) owner=(\d+) epoch=(\d+) revision=(\d+) digest=([0-9a-f]{32})"
)
SEND = re.compile(
    r"PROOF_BASE_EXPANSION_SEND owner=2 kind=(refinery|factory|rejected-refinery)"
    r" epoch=(\d+) sequence=(\d+) x=(-?[0-9.eE+]+) y=(-?[0-9.eE+]+) z=(-?[0-9.eE+]+)"
)
RECEIPT = re.compile(r"PROOF_RESULT owner=2 sequence=(\d+) accepted=([01]) reason=([^\s]+)")
HOST_RESULT = re.compile(r"PROOF_HOST_RESULT owner=2 sequence=(\d+) accepted=([01]) reason=([^\s]+)")
HEX32 = re.compile(r"^[0-9a-f]{32}$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
SOURCE_HEAD = "d1639667f520253e508b1192295e52e47f90399a"
RADIUS = 800.0
SEPARATION = 180.0


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
    return type(value) is int and value >= minimum


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def point(value):
    require(isinstance(value, dict) and set(value) == {"x", "y", "z"}, "invalid point")
    require(all(number(value[axis]) and abs(value[axis]) <= 100000 for axis in ("x", "y", "z")), "nonfinite/out-of-world point")
    return value


def distance(a, b):
    return math.sqrt(sum((a[axis] - b[axis]) ** 2 for axis in ("x", "y", "z")))


def state(value):
    require(isinstance(value, dict) and set(value) == {
        "epoch", "revision", "digest", "skirmish", "phase", "winner", "players", "units", "resources"
    }, "invalid state schema")
    require(integer(value["epoch"], 1) and integer(value["revision"], 1), "invalid epoch/revision")
    require(isinstance(value["digest"], str) and HEX32.fullmatch(value["digest"]), "invalid digest")
    require(value["skirmish"] is True and integer(value["phase"]) and type(value["winner"]) is int, "invalid match state")
    require(isinstance(value["players"], list) and isinstance(value["units"], list) and isinstance(value["resources"], list), "invalid arrays")
    owners, unit_ids, resource_ids = set(), set(), set()
    for player in value["players"]:
        require(isinstance(player, dict) and set(player) == {"owner", "team", "credits", "gathered"}, "invalid player")
        require(integer(player["owner"], 1) and type(player["team"]) is int and integer(player["credits"]) and integer(player["gathered"]), "invalid player values")
        require(player["owner"] not in owners, "duplicate player")
        owners.add(player["owner"])
    for unit in value["units"]:
        require(isinstance(unit, dict) and set(unit) == {
            "id", "owner", "kind", "hp", "position", "direction", "cargo", "rally", "rallyPoint",
            "guard", "post", "target", "returning"
        }, "invalid unit schema")
        require(integer(unit["id"], 1) and integer(unit["owner"], 1) and integer(unit["kind"]) and
                number(unit["hp"]) and 0 <= unit["hp"] <= 10000 and integer(unit["cargo"]) and
                type(unit["rally"]) is bool and type(unit["guard"]) is bool and
                type(unit["returning"]) is bool and type(unit["target"]) is int, "invalid unit values")
        point(unit["position"]); point(unit["direction"]); point(unit["rallyPoint"]); point(unit["post"])
        require(unit["id"] not in unit_ids, "duplicate unit")
        unit_ids.add(unit["id"])
    for resource in value["resources"]:
        require(isinstance(resource, dict) and set(resource) == {"id", "owner", "position", "remaining"}, "invalid resource")
        require(integer(resource["id"], 1) and type(resource["owner"]) is int and integer(resource["remaining"]), "invalid resource values")
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


def added(before, after):
    old_ids = {unit["id"] for unit in before["units"]}
    require(all(unit in after["units"] for unit in before["units"]), "old unit mutated by synchronous build")
    return [unit for unit in after["units"] if unit["id"] not in old_ids]


def read_role(path: Path, *, role: str, viewer: int, run: str, source: str):
    require(path.is_file() and path.stat().st_size <= 32 * 1024 * 1024, f"missing/oversized {role} log")
    text = path.read_text(encoding="utf-8", errors="replace")
    require("PROOF_BASE_EXPANSION_OBSERVATION_ERROR" not in text, f"{role} producer error")
    rows, snapshots, commands, ordinary = [], {}, [], {}
    expected_role = "host" if viewer == 0 else "client"
    for match in ORDINARY.finditer(text):
        if match.group(1) == expected_role and int(match.group(2)) == viewer:
            key = (int(match.group(3)), int(match.group(4)))
            digest = match.group(5)
            require(key not in ordinary or ordinary[key] == digest, f"{role} conflicting ordinary digest")
            ordinary[key] = digest
    for line in text.splitlines():
        if MARKER not in line:
            continue
        payload = line.split(MARKER, 1)[1]
        require(len(payload) <= 65536, f"{role} oversized row")
        row = json.loads(payload, object_pairs_hook=unique_object, parse_constant=reject_constant)
        require(isinstance(row, dict) and row.get("v") == 1 and row.get("run") == run and
                row.get("source") == source and row.get("role") == expected_role and
                row.get("viewer") == viewer and row.get("row") == len(rows) + 1, f"{role} unbound row")
        event = row.get("event")
        require(event in ("snapshot", "command"), f"{role} invalid event")
        if event == "snapshot":
            require(set(row) == {"v", "run", "source", "role", "viewer", "event", "row", "state"}, "snapshot schema")
            snap = state(row["state"])
            key = (snap["epoch"], snap["revision"])
            require(key not in snapshots and ordinary.get(key) == snap["digest"], f"{role} snapshot/digest mismatch")
            snapshots[key] = snap
        else:
            require(viewer == 0, "client command observation")
            require(set(row) == {"v", "run", "source", "role", "viewer", "event", "row",
                                "owner", "type", "sequence", "payloadEpoch", "unit", "target",
                                "destination", "accepted", "reason", "before", "after"}, "command schema")
            require(row["owner"] == 2 and integer(row["sequence"], 1) and
                    integer(row["payloadEpoch"], 1) and row["type"] in (3, 4) and
                    type(row["accepted"]) is bool and isinstance(row["reason"], str) and
                    type(row["unit"]) is int and type(row["target"]) is int, "invalid command")
            point(row["destination"])
            state(row["before"]); state(row["after"])
            require(row["before"]["epoch"] == row["after"]["epoch"] == row["payloadEpoch"], "command epoch")
            commands.append(row)
        rows.append(row)
    require(0 < len(rows) <= 8192 and len(snapshots) >= 5, f"{role} insufficient/too many rows")
    return text, snapshots, commands, len(rows)


def verify(run_dir: Path, result_path: Path, diagnostic_head: str, source_tree: str):
    require(HEX40.fullmatch(diagnostic_head) and HEX40.fullmatch(source_tree), "expected identity format")
    result = json.loads(result_path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object, parse_constant=reject_constant)
    require(result["passed"] is True and result["scenario"] == "Coop" and result["head"] == diagnostic_head and
            diagnostic_head != SOURCE_HEAD and result["sourceTree"] == source_tree and result["baseExpansionEvidenceEnabled"] is True and
            result["dirtyWorkingTree"] is False, "diagnostic result identity/status")
    run = result["baseExpansionRun"]
    require(isinstance(run, str) and HEX32.fullmatch(run), "run token")
    roles = {
        "host": read_role(run_dir / "host.log", role="host", viewer=0, run=run, source=source_tree),
        "rendered": read_role(run_dir / "rendered.log", role="rendered", viewer=1, run=run, source=source_tree),
        "observer": read_role(run_dir / "observer.log", role="observer", viewer=2, run=run, source=source_tree),
    }
    commands = roles["host"][2]
    require(len(commands) == 3, "expected exactly three owner-2 build submissions")
    first, second, rejected = commands
    require([item["type"] for item in commands] == [3, 4, 3] and
            [item["sequence"] for item in commands] == [1, 2, 3] and
            [item["accepted"] for item in commands] == [True, True, False] and
            rejected["reason"] == "placement", "command order/outcomes")
    require(first["before"]["revision"] < first["after"]["revision"] <=
            second["before"]["revision"] < second["after"]["revision"] <=
            rejected["before"]["revision"], "command revision chronology")
    hqs = units(first["before"], owner=2, kind=2, live=True)
    require(len(hqs) == 1, "owner-2 construction HQ")
    hq = hqs[0]
    require(distance(first["destination"], hq["position"]) <= RADIUS, "first refinery outside own HQ")
    refinery_added = added(first["before"], first["after"])
    require(len(refinery_added) == 2 and
            len([u for u in refinery_added if u["kind"] == 3 and u["owner"] == 2 and u["position"] == first["destination"] and u["hp"] == 200]) == 1 and
            len([u for u in refinery_added if u["kind"] == 1 and u["owner"] == 2 and u["hp"] == 100 and u["cargo"] == 0]) == 1,
            "initial refinery/grant")
    refinery = next(u for u in refinery_added if u["kind"] == 3)
    require(player(first["before"], 2)["credits"] - player(first["after"], 2)["credits"] == 100 and
            first["after"]["revision"] - first["before"]["revision"] == 3, "initial refinery economy/revision")
    hqs2 = units(second["before"], owner=2, kind=2, live=True)
    refs2 = [u for u in units(second["before"], owner=2, kind=3, live=True) if u["id"] == refinery["id"]]
    require(len(hqs2) == len(refs2) == 1, "live own HQ/refinery before expanded build")
    require(distance(second["destination"], hqs2[0]["position"]) > RADIUS and
            distance(second["destination"], refs2[0]["position"]) <= RADIUS, "factory not genuinely extended from owned refinery")
    require(all(distance(second["destination"], u["position"]) >= SEPARATION
                for u in units(second["before"], live=True) if u["kind"] in (2, 3, 4)),
            "factory overlap")
    factory_added = added(second["before"], second["after"])
    require(len(factory_added) == 1 and factory_added[0]["owner"] == 2 and
            factory_added[0]["kind"] == 4 and factory_added[0]["position"] == second["destination"] and
            factory_added[0]["hp"] == 200, "expanded factory record")
    factory = factory_added[0]
    require(player(second["before"], 2)["credits"] - player(second["after"], 2)["credits"] == 150 and
            second["after"]["revision"] - second["before"]["revision"] == 2, "expanded factory economy/revision")
    require(rejected["before"] == rejected["after"], "rejection mutated complete state")
    require(all(distance(rejected["destination"], u["position"]) > RADIUS for u in
                units(rejected["before"], owner=2, live=True) if u["kind"] in (2, 3, 4)),
            "rejected site reached own live structure")
    require(any(distance(rejected["destination"], u["position"]) <= RADIUS for u in
                units(rejected["before"], live=True) if u["owner"] != 2 and u["kind"] in (2, 3, 4)),
            "rejected site lacks foreign anchor")
    require(all(distance(rejected["destination"], u["position"]) >= SEPARATION for u in
                units(rejected["before"], live=True) if u["kind"] in (2, 3, 4)), "foreign test collided")
    observer_text = roles["observer"][0]
    sends = list(SEND.finditer(observer_text))
    require(len(sends) == 3, "three public RPC sends")
    for match, command, kind in zip(sends, commands, ("refinery", "factory", "rejected-refinery")):
        require(match.group(1) == kind and int(match.group(2)) == command["payloadEpoch"] and
                int(match.group(3)) == command["sequence"] and
                all(math.isclose(float(match.group(index)), command["destination"][axis], abs_tol=0.01)
                    for index, axis in zip((4, 5, 6), ("x", "y", "z"))), "client intent/host command mismatch")
    for pattern, text in ((RECEIPT, observer_text), (HOST_RESULT, roles["host"][0])):
        observed = [(int(m.group(1)), int(m.group(2)), m.group(3)) for m in pattern.finditer(text)
                    if int(m.group(1)) in (1, 2, 3)]
        require(len(observed) == 3 and observed == [(1, 1, "accepted"), (2, 1, "accepted"), (3, 0, "placement")],
                "host/client receipts mismatch")
    require("PROOF_BASE_EXPANSION_REJECTION_RECEIPT owner=2" in observer_text and
            "PROOF_BASE_EXPANSION_COMPLETE role=observer owner=2 status=pass rejectionVerified=1" in observer_text,
            "missing fixture rejection/completion")
    common = sorted(set(roles["host"][1]).intersection(roles["rendered"][1], roles["observer"][1]))
    require(len(common) >= 5, "insufficient three-role checkpoints")
    for key in common:
        require(roles["host"][1][key] == roles["rendered"][1][key] == roles["observer"][1][key],
                f"three-role semantic mismatch at {key}")
    accepted_common = [key for key in common if key[1] >= second["after"]["revision"] and any(
        u["id"] == factory["id"] and u["owner"] == 2 and u["kind"] == 4 and
        u["position"] == factory["position"] and u["hp"] > 0
        for u in roles["host"][1][key]["units"]
    )]
    require(accepted_common, "no published common expanded factory state")
    require("PROOF_SKIRMISH_COMPLETE role=host status=pass scenario=Coop winnerTeam=0" in roles["host"][0] and
            "PROOF_SKIRMISH_COMPLETE role=observer owner=2 status=pass scenario=Coop winnerTeam=0" in observer_text and
            "PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario=Coop winnerTeam=0" in roles["rendered"][0],
            "ordinary match regression failed")
    digest_lines = [f"{epoch}:{revision}:{roles['host'][1][(epoch, revision)]['digest']}"
                    for epoch, revision in common]
    return {
        "passed": True, "productionHead": SOURCE_HEAD, "diagnosticHead": result["head"],
        "diagnosticSourceTree": source_tree, "run": run,
        "commandSequences": [item["sequence"] for item in commands],
        "owner2HQ": {"id": hq["id"], "position": hq["position"]},
        "refinery": {"id": refinery["id"], "position": refinery["position"]},
        "factory": {"id": factory["id"], "position": factory["position"]},
        "factoryDistanceFromHQ": distance(second["destination"], hqs2[0]["position"]),
        "factoryDistanceFromRefinery": distance(second["destination"], refs2[0]["position"]),
        "rejectedDestination": rejected["destination"],
        "rejectedReason": rejected["reason"], "rejectedStateUnchanged": True,
        "commonCheckpoints": len(common),
        "commonDigestSeriesSha256": hashlib.sha256(("\n".join(digest_lines) + "\n").encode()).hexdigest().upper(),
        "firstCommonFactoryRevision": accepted_common[0][1],
        "proofScope": "synthetic owner-2 public RPC through host model and three-role loopback replication; physical R/F unverified",
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
