"""Fail-closed parser for the isolated PR70 three-role recovery diagnostic.

The host fixture is scripted; this validates its public paid state and the
client RPC/replication trail, not autonomous default-map stall frequency.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

MARKER = "PROOF_BASE_EXPANSION_OBSERVATION "
COMPLETE = "PROOF_BOT_BASE_EXPANSION_COMPLETE "
BEFORE = "PROOF_BOT_BASE_EXPANSION_BEFORE "
SEND = "PROOF_BOT_BASE_EXPANSION_SEND "
OFFSETS = ((350, 0), (-350, 0), (0, 350), (0, -350), (350, 350),
           (-350, 350), (350, -350), (-350, -350), (600, 0),
           (-600, 0), (0, 600), (0, -600))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def fields(line: str, prefix: str) -> dict[str, str]:
    require(prefix in line, f"missing {prefix}")
    return dict(re.findall(r"([A-Za-z][A-Za-z0-9]*)=([^\s]+)", line.split(prefix, 1)[1]))


def lines_with(lines: list[str], prefix: str) -> list[str]:
    return [line for line in lines if prefix in line]


def stream(path: Path, role: str, viewer: int, run: str, source: str):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    require(not any(COMPLETE in line and "status=fail" in line for line in lines), f"{role}: failure marker")
    rows = []
    for line in lines_with(lines, MARKER):
        row = json.loads(line.split(MARKER, 1)[1])
        require(row.get("v") == 1 and row.get("run") == run and row.get("source") == source,
                f"{role}: run/source/version")
        require(row.get("role") == ("host" if role == "host" else "client")
                and row.get("viewer") == viewer and row.get("row") == len(rows) + 1,
                f"{role}: role/viewer/row continuity")
        rows.append(row)
    require(rows and len(rows) <= 8192, f"{role}: empty or overlarge stream")
    events = [row.get("event") for row in rows]
    require(set(events) <= {"snapshot", "command", "finish"}, f"{role}: unknown event")
    if role != "host":
        require("command" not in events, f"{role}: host command row on client")
    return lines, rows


def one(lines: list[str], prefix: str, label: str):
    hits = lines_with(lines, prefix)
    require(len(hits) == 1, f"{label}: expected one {prefix}, saw {len(hits)}")
    return fields(hits[0], prefix)


def state_at(rows: list[dict], revision: int, label: str) -> dict:
    matches = [row["state"] for row in rows if row["event"] == "snapshot"
               and row["state"]["revision"] == revision]
    require(len(matches) == 1, f"{label}: missing/duplicate snapshot revision {revision}")
    return matches[0]


def player(state: dict, owner: int) -> dict:
    matches = [item for item in state["players"] if item["owner"] == owner]
    require(len(matches) == 1, f"owner {owner} count")
    return matches[0]


def live(state: dict, owner: int, kind: int | None = None) -> list[dict]:
    return [unit for unit in state["units"] if unit["owner"] == owner and unit["hp"] > 0
            and (kind is None or unit["kind"] == kind)]


def point(unit: dict) -> tuple[float, float, float]:
    pos = unit["position"]
    return pos["x"], pos["y"], pos["z"]


def check_before(before: dict) -> None:
    require(before["epoch"] == 1 and before["skirmish"] is True
            and before["phase"] == 1, "before: match identity/phase")
    p = player(before, 1)
    require(p["team"] == 0 and p["credits"] == 100 and p["gathered"] == 450,
            "before: paid income")
    require(player(before, 2)["team"] == 0 and player(before, 100)["team"] == 1,
            "before: three owners/teams")
    require(len(live(before, 1)) == 6 and not live(before, 1, 1), "before: two actual miner losses")
    require(len(live(before, 1, 2)) == 1 and len(live(before, 1, 3)) == 1
            and len(live(before, 1, 4)) == 4 and bool(live(before, 100, 0)),
            "before: HQ, committed anchor, paid blockers, attacker")
    require(point(live(before, 1, 2)[0]) == (-1400, -400, 0)
            and point(live(before, 1, 3)[0]) == (-600, -400, 0), "before: original anchors")
    structures = [u for u in before["units"] if u["hp"] > 0 and u["kind"] in (2, 3, 4)]
    for dx, dy in OFFSETS:
        x, y = -1400 + dx, -400 + dy
        require(any((point(unit)[0] - x) ** 2 + (point(unit)[1] - y) ** 2 < 250 ** 2
                    for unit in structures), f"before: HQ offset {(dx, dy)} open")
    require(not any(point(unit) == (-250, -400, 0) for unit in structures),
            "before: fallback already built")


def check_after(before: dict, after: dict) -> None:
    require(after["epoch"] == before["epoch"] and after["phase"] == 1
            and after["revision"] == before["revision"] + 3, "after: epoch/phase/revision")
    p = player(after, 1)
    require(p["credits"] == 0 and p["gathered"] == 450, "after: exact paid debit/no minted ore")
    require(len(after["units"]) == len(before["units"]) + 2,
            "after: refinery plus granted miner")
    old = {u["id"]: u for u in before["units"]}
    new = {u["id"]: u for u in after["units"]}
    require(len(old) == len(before["units"]) and len(new) == len(after["units"]),
            "after: duplicate unit IDs")
    for unit_id, prior in old.items():
        require(new.get(unit_id) == prior, f"after: existing unit {unit_id} changed")
    max_id = max(old)
    require(set(new) - set(old) == {max_id + 1, max_id + 2}, "after: grant IDs")
    ref, miner = new[max_id + 1], new[max_id + 2]
    require(ref["owner"] == 1 and ref["kind"] == 3 and ref["hp"] == 200
            and point(ref) == (-250, -400, 0), "after: fallback refinery")
    require(miner["owner"] == 1 and miner["kind"] == 1 and miner["hp"] == 100
            and miner["cargo"] == 0, "after: replacement miner")
    require(len(live(after, 1, 1)) == 1, "after: exact live replacement count")


def verify(host: Path, owner1: Path, owner2: Path, run: str, source: str) -> dict:
    logs = {}
    for role, path, viewer in (("host", host, 0), ("owner1", owner1, 1), ("owner2", owner2, 2)):
        logs[role] = stream(path, role, viewer, run, source)
    host_lines, host_rows = logs["host"]
    before_marker = one(host_lines, BEFORE, "host")
    host_complete = one(host_lines, COMPLETE, "host")
    require(host_complete.get("status") == "pass", "host: incomplete")
    before_rev = int(before_marker["revision"])
    after_rev = int(host_complete["afterRevision"])
    require(int(host_complete["beforeRevision"]) == before_rev, "host: before marker mismatch")
    before = state_at(host_rows, before_rev, "host-before")
    after = state_at(host_rows, after_rev, "host-after")
    require(before["digest"] == before_marker["digest"]
            and after["digest"] == host_complete["afterDigest"], "host: digest markers")
    check_before(before)
    check_after(before, after)
    new_miner = next(u for u in after["units"] if u["id"] == max(u["id"] for u in before["units"]) + 2)
    require(point(new_miner) == (float(before_marker["grantX"]), float(before_marker["grantY"]), 0),
            "after: replacement spawn differs from public preflight")
    require(after["resources"] == before["resources"]
            and player(after, 2) == player(before, 2)
            and player(after, 100) == player(before, 100),
            "after: unrelated state changed")
    commands = [row for row in host_rows if row["event"] == "command"]
    require(len(commands) == 2, "host: exact two observed client RPC commands")
    negative, positive = commands
    for command in commands:
        require(command["type"] == 3 and command["payloadEpoch"] == 1
                and command["destination"] == {"x": -250, "y": -400, "z": 0}
                and command["unit"] == -1 and command["target"] == -1,
                "host: RPC payload")
    require(negative["owner"] == 2 and negative["sequence"] == 1
            and negative["accepted"] is False and negative["reason"] == "placement"
            and negative["before"] == before and negative["after"] == before,
            "host: foreign-anchor rejection without visible mutation")
    require(positive["owner"] == 1
            and positive["sequence"] == int(before_marker["ownerSequence"]) + 1
            and positive["accepted"] is True and positive["before"] == before
            and positive["after"] == after, "host: adapted planner intent/accepted recovery")
    for role, owner, positive_send in (("owner1", 1, 1), ("owner2", 2, 0)):
        lines, rows = logs[role]
        require(state_at(rows, before_rev, role + "-before") == before,
                f"{role}: before replication mismatch")
        require(state_at(rows, after_rev, role + "-after") == after,
                f"{role}: after replication mismatch")
        send = one(lines, SEND, role)
        done = one(lines, COMPLETE, role)
        require(int(send["owner"]) == owner and int(send["revision"]) == before_rev
                and int(send["positive"]) == positive_send and int(send["epoch"]) == 1,
                f"{role}: command source/epoch")
        require(done.get("role") == "client" and done.get("status") == "pass"
                and int(done["owner"]) == owner and int(done["revision"]) == after_rev
                and done["digest"] == after["digest"], f"{role}: receipt and final state")
    return {"status": "pass", "run": run, "source": source, "before_revision": before_rev,
            "after_revision": after_rev, "after_digest": after["digest"],
            "observed_host_commands": len(commands), "replicated_clients": 2}


def main() -> None:
    ap = argparse.ArgumentParser()
    for name in ("host", "owner1", "owner2"):
        ap.add_argument("--" + name, required=True, type=Path)
    ap.add_argument("--run", required=True)
    ap.add_argument("--source", required=True)
    args = ap.parse_args()
    require(re.fullmatch(r"[0-9a-fA-F]{32}", args.run) is not None, "run token")
    require(re.fullmatch(r"[0-9a-fA-F]{40}", args.source) is not None, "source token")
    print(json.dumps(verify(args.host, args.owner1, args.owner2, args.run, args.source), sort_keys=True))


if __name__ == "__main__":
    main()
