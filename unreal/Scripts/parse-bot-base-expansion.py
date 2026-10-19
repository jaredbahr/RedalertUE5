"""Fail-closed gate for the isolated PR70 scripted recovery diagnostic.

The complete, explicitly closed observation streams establish public-state and
RPC/replication consistency. They do not prove autonomous gameplay, private model
state, native execution, or digest authenticity. Digests are native 32-hex opaque
identity tokens: matching full states must agree on them, and a token cannot
identify two different full states. Native ComputeDigest is NOT recomputed here.
JSON decimal values are compared exactly, without binary-float rounding/tolerance.
"""
from __future__ import annotations

import argparse
import copy
from decimal import Decimal, InvalidOperation, localcontext
from fractions import Fraction
import json
from pathlib import Path
import re

MARKER = "PROOF_BASE_EXPANSION_OBSERVATION "
COMPLETE = "PROOF_BOT_BASE_EXPANSION_COMPLETE "
BEFORE = "PROOF_BOT_BASE_EXPANSION_BEFORE "
READY = "PROOF_BOT_BASE_EXPANSION_READY "
SEND = "PROOF_BOT_BASE_EXPANSION_SEND "
RECEIPT = "PROOF_BOT_BASE_EXPANSION_RECEIPT "
OBSERVED = "PROOF_BOT_BASE_EXPANSION_OBSERVED "
ACK = "PROOF_BOT_BASE_EXPANSION_ACK "
COMMON = {"v", "run", "source", "role", "viewer", "event", "row"}
MAX_INT32 = 2147483647
MAX_ROWS = 8192
MAX_ROW_BYTES = 65536
MAX_OBSERVATION_BYTES = 32 * 1024 * 1024
MAX_LOG_BYTES = 64 * 1024 * 1024
MAX_LINE_BYTES = 128 * 1024
MAX_DOUBLE = Decimal("1.7976931348623157e308")
HEX32 = re.compile(r"[0-9a-f]{32}\Z")
TOKEN32 = re.compile(r"[0-9a-fA-F]{32}\Z")
TOKEN40 = re.compile(r"[0-9a-fA-F]{40}\Z")
LOG_PREFIX = re.compile(r"(?:\[[^\]\r\n]*\])*\s*LogTemp: Display: \Z")
RESERVED = re.compile(r"PROOF_(?:BOT_)?BASE_EXPANSION")
OFFSETS = ((350, 0), (-350, 0), (0, 350), (0, -350), (350, 350),
           (-350, 350), (350, -350), (-350, -350), (600, 0),
           (-600, 0), (0, 600), (0, -600))
DIRECTIONS = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1),
              (-1, 1), (1, -1), (-1, -1))
ZERO = {"x": 0, "y": 0, "z": 0}
SITE = {"x": -250, "y": -400, "z": 0}
TEAMS = {1: 0, 2: 0, 100: 1}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def schema(value, keys, label):
    require(type(value) is dict and set(value) == set(keys), f"{label}: field schema")


def integer(value, minimum=0):
    return type(value) is int and minimum <= value <= MAX_INT32


def magnitude(value):
    return value.copy_abs() if type(value) is Decimal else abs(value)


def number(value):
    return (type(value) is int and abs(value) <= MAX_DOUBLE) or (
        type(value) is Decimal and value.is_finite() and value.copy_abs() <= MAX_DOUBLE)


def digest(value):
    return type(value) is str and HEX32.fullmatch(value) is not None


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON/marker key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"nonfinite JSON constant: {value}")


def parse_decimal(token):
    require(len(token) <= 128, "numeric token too long")
    value = Decimal(token)
    require(value.is_finite() and abs(value.as_tuple().exponent) <= 400
            and value.copy_abs() <= MAX_DOUBLE, "invalid/out-of-range decimal")
    return value


def parse_integer(token):
    require(len(token) <= 512, "integer token too long")
    return int(token)


def point(value):
    schema(value, ("x", "y", "z"), "point")
    require(all(number(v) for v in value.values()), "nonfinite/invalid point number")
    return value


def in_world(value):
    return all(number(v) and magnitude(v) <= 10000 for v in value.values())


def distance_squared(a, b):
    # Fraction avoids Decimal context rounding at strict placement boundaries.
    return sum((Fraction(a[k]) - Fraction(b[k])) ** 2 for k in ("x", "y", "z"))


def validate_state(value):
    schema(value, {"epoch", "revision", "digest", "skirmish", "phase", "winner",
                   "players", "units", "resources"}, "state")
    require(integer(value["epoch"], 1) and value["epoch"] == 1
            and integer(value["revision"], 1) and digest(value["digest"]), "state identity")
    require(value["skirmish"] is True and integer(value["phase"])
            and value["phase"] == 1 and integer(value["winner"], -1)
            and value["winner"] == -1, "state: unfinished Playing match")
    require(all(type(value[k]) is list for k in ("players", "units", "resources")), "state arrays")
    require(len(value["players"]) <= 3 and len(value["units"]) <= 128
            and len(value["resources"]) <= 3, "state record bounds")
    owners, unit_ids, resource_ids = set(), set(), set()
    for p in value["players"]:
        schema(p, ("owner", "team", "credits", "gathered"), "player")
        require(integer(p["owner"], 1) and p["owner"] in TEAMS
                and integer(p["team"]) and p["team"] == TEAMS[p["owner"]]
                and integer(p["credits"]) and integer(p["gathered"]), "player values")
        require(p["owner"] not in owners, "duplicate player owner")
        owners.add(p["owner"])
    require(owners in ({100}, {100, 1}, {100, 1, 2}), "state: impossible startup owner membership")
    for u in value["units"]:
        schema(u, {"id", "owner", "kind", "hp", "position", "direction", "cargo",
                   "rally", "rallyPoint", "guard", "post", "target", "returning"}, "unit")
        require(integer(u["id"], 1) and integer(u["owner"], 1) and u["owner"] in owners
                and integer(u["kind"]) and u["kind"] in range(5)
                and number(u["hp"]) and 0 <= u["hp"] <= 10000
                and integer(u["cargo"]) and integer(u["target"], -1)
                and all(type(u[k]) is bool for k in ("rally", "guard", "returning")), "unit values")
        for k in ("position", "direction", "rallyPoint", "post"):
            point(u[k])
        require(in_world(u["position"]) and in_world(u["rallyPoint"]) and in_world(u["post"]), "unit outside world")
        require(u["id"] not in unit_ids, "duplicate unit ID")
        unit_ids.add(u["id"])
    resource_owners = set()
    for r in value["resources"]:
        schema(r, ("id", "owner", "position", "remaining"), "resource")
        require(integer(r["id"], 1) and integer(r["owner"], 1) and r["owner"] in owners
                and integer(r["remaining"]), "resource values")
        point(r["position"])
        require(in_world(r["position"]), "resource outside world")
        require(r["id"] not in resource_ids and r["owner"] not in resource_owners,
                "duplicate resource ID/owner")
        resource_ids.add(r["id"])
        resource_owners.add(r["owner"])
    require(resource_owners == owners, "seeded owner/resource membership")
    return value


def freeze(value):
    if type(value) is dict:
        return tuple((key, freeze(val)) for key, val in sorted(value.items()) if key != "digest")
    if type(value) is list:
        return tuple(freeze(item) for item in value)
    return value


class StateIdentities:
    def __init__(self):
        self.revisions = {}
        self.digests = {}
        self.states = {}

    def check(self, state):
        validate_state(state)
        key = (state["epoch"], state["revision"])
        frozen = freeze(state)
        token = state["digest"]
        require(key not in self.revisions or self.revisions[key] == state, "same revision has conflicting state/digest")
        require(token not in self.digests or self.digests[token] == frozen, "digest reused for different full state")
        require(frozen not in self.states or self.states[frozen] == token, "same full state has conflicting digest")
        self.revisions[key] = state
        self.digests[token] = frozen
        self.states[frozen] = token


def marker_fields(payload, keys):
    words = payload.split()
    require(words and all(word.count("=") == 1 and all(word.split("=")) for word in words), "malformed marker fields")
    values = unique_object(word.split("=", 1) for word in words)
    schema(values, keys, "marker")
    return values


def marker_int(values, key, minimum=0):
    token = values[key]
    require(re.fullmatch(r"0|[1-9][0-9]*", token) is not None and len(token) <= 10, f"marker {key}: integer syntax")
    value = int(token)
    require(integer(value, minimum), f"marker {key}: integer range")
    return value


def read_role(path, role, viewer, run, source, identities):
    require(path.is_file() and path.stat().st_size <= MAX_LOG_BYTES, f"{role}: missing/overlarge log")
    rows, snapshots, commands, markers = [], [], [], {}
    finish = None
    total_bytes = observation_bytes = 0
    with path.open("rb") as handle:
        line_no = 0
        while True:
            raw = handle.readline(MAX_LINE_BYTES + 1)
            if not raw:
                break
            line_no += 1
            total_bytes += len(raw)
            require(total_bytes <= MAX_LOG_BYTES and len(raw) <= MAX_LINE_BYTES, f"{role}: log/line cap")
            require(raw.endswith(b"\n"), f"{role}: unterminated/truncated log line")
            line = raw.decode("utf-8", errors="strict").rstrip("\r\n")
            hits = list(RESERVED.finditer(line))
            if not hits:
                continue
            require(len(hits) == 1, f"{role}: multiple reserved markers on line")
            prefix, payload = line[:hits[0].start()], line[hits[0].start():]
            require(not prefix or LOG_PREFIX.fullmatch(prefix), f"{role}: malformed observation log prefix")
            name, separator, rest = payload.partition(" ")
            require(separator and rest and name + " " in {MARKER, BEFORE, READY, SEND, RECEIPT, OBSERVED, ACK, COMPLETE},
                    f"{role}: malformed/error/unknown reserved marker")
            require(COMPLETE not in markers, f"{role}: evidence after completion")
            if name + " " != MARKER:
                key = name + " "
                require(key == ACK or key not in markers, f"{role}: duplicate {name}")
                allowed = {BEFORE, ACK, COMPLETE} if viewer == 0 else {READY, SEND, RECEIPT, OBSERVED, COMPLETE}
                require(key in allowed, f"{role}: foreign-role marker")
                keys = ({"epoch", "revision", "digest", "ownerSequence", "grantX", "grantY"} if key == BEFORE else
                        {"owner", "epoch", "revision", "sequence", "positive"} if key in (READY, SEND) else
                        {"owner", "epoch", "sequence", "accepted", "reason"} if key == RECEIPT else
                        {"owner", "epoch", "revision", "digest"} if key in (OBSERVED, ACK) else
                        {"role", "status", "epoch", "beforeRevision", "afterRevision", "afterDigest"} if viewer == 0 else
                        {"role", "status", "owner", "epoch", "revision", "digest"})
                values = marker_fields(rest, keys)
                require((key in (ACK, OBSERVED, COMPLETE)) == (finish is not None), f"{role}: marker/finish ordering")
                if key == ACK:
                    markers.setdefault(key, []).append((line_no, values))
                    require(len(markers[key]) <= 2, "host: excess acknowledgements")
                else:
                    markers[key] = (line_no, values)
                continue
            require(finish is None, f"{role}: event after finish")
            observation_bytes += len(rest.encode("utf-8"))
            require(len(rest.encode("utf-8")) <= MAX_ROW_BYTES and observation_bytes <= MAX_OBSERVATION_BYTES
                    and len(rows) < MAX_ROWS, f"{role}: observation cap")
            row = json.loads(rest, object_pairs_hook=unique_object, parse_constant=reject_constant,
                             parse_float=parse_decimal, parse_int=parse_integer)
            require(type(row) is dict and integer(row.get("v"), 1) and row["v"] == 1
                    and row.get("run") == run and row.get("source") == source
                    and row.get("role") == ("host" if viewer == 0 else "client")
                    and integer(row.get("viewer")) and row["viewer"] == viewer
                    and integer(row.get("row"), 1) and row["row"] == len(rows) + 1, f"{role}: row identity/continuity")
            event = row.get("event")
            if event == "snapshot":
                schema(row, COMMON | {"state"}, "snapshot row")
                identities.check(row["state"])
                require(not snapshots or row["state"]["revision"] > snapshots[-1][1]["state"]["revision"],
                        f"{role}: nonmonotone snapshots")
                snapshots.append((line_no, row))
            elif event == "command":
                require(viewer == 0, f"{role}: command row on client")
                schema(row, COMMON | {"owner", "type", "sequence", "payloadEpoch", "unit", "target",
                                     "destination", "accepted", "reason", "resultEpoch", "resultSequence",
                                     "before", "after"}, "command row")
                require(integer(row["owner"], 1) and row["owner"] in (1, 2)
                        and integer(row["type"]) and row["type"] == 3
                        and integer(row["sequence"], 1) and integer(row["payloadEpoch"], 1)
                        and integer(row["resultEpoch"], 1) and integer(row["resultSequence"], 1)
                        and integer(row["unit"], -1) and row["unit"] == -1
                        and integer(row["target"], -1) and row["target"] == -1
                        and type(row["accepted"]) is bool and type(row["reason"]) is str,
                        "command types/identity")
                point(row["destination"])
                identities.check(row["before"])
                identities.check(row["after"])
                require(row["payloadEpoch"] == row["resultEpoch"] == row["before"]["epoch"] == row["after"]["epoch"]
                        and row["resultSequence"] == row["sequence"], "command/result identity")
                commands.append((line_no, row))
            elif event == "finish":
                schema(row, COMMON | {"epoch", "revision", "digest", "snapshots", "commands"}, "finish row")
                require(integer(row["epoch"], 1) and integer(row["revision"], 1) and digest(row["digest"])
                        and integer(row["snapshots"], 1) and row["snapshots"] == len(snapshots)
                        and integer(row["commands"]) and row["commands"] == len(commands), "finish identity/counts")
                require(rows and rows[-1][1]["event"] == "snapshot", "finish must immediately follow final observed snapshot")
                require(all(row[k] == snapshots[-1][1]["state"][k] for k in ("epoch", "revision", "digest")), "finish/state mismatch")
                finish = (line_no, row)
            else:
                raise ValueError(f"{role}: unknown event")
            rows.append((line_no, row))
    require(finish is not None and len(snapshots) >= 2, f"{role}: incomplete stream")
    require(set(markers) == ({BEFORE, ACK, COMPLETE} if viewer == 0 else {READY, SEND, RECEIPT, OBSERVED, COMPLETE}), f"{role}: missing markers")
    require(finish[0] < markers[COMPLETE][0], f"{role}: completion before finish")
    return {"rows": rows, "snapshots": snapshots, "commands": commands, "finish": finish, "markers": markers}


def player(state, owner):
    matches = [p for p in state["players"] if p["owner"] == owner]
    require(len(matches) == 1, f"owner {owner}: missing/duplicate")
    return matches[0]


def units(state, owner=None, kind=None, live=False):
    return [u for u in state["units"] if (owner is None or u["owner"] == owner)
            and (kind is None or u["kind"] == kind) and (not live or u["hp"] > 0)]


def structures(state, owner=None):
    return [u for u in units(state, owner, live=True) if u["kind"] in (2, 3, 4)]


def expected_spawn(state, anchor):
    """Independent pre-build fixed-order three-ring policy; 3D separation >=260."""
    point(anchor)
    require(in_world(anchor), "spawn anchor outside world")
    for ring in (1, 2, 3):
        for dx, dy in DIRECTIONS:
            # Input decimals have <=128 characters and exponent magnitude <=400.
            # This precision preserves their exact sum with the integer offset.
            with localcontext() as context:
                context.prec = 1024
                candidate = {"x": anchor["x"] + dx * 300 * ring,
                             "y": anchor["y"] + dy * 300 * ring, "z": anchor["z"]}
            if in_world(candidate) and all(distance_squared(candidate, u["position"]) >= 260 ** 2
                                           for u in units(state, live=True)):
                return candidate
    raise ValueError("no legal grant spawn")


def new_unit(unit_id, kind, position):
    return {"id": unit_id, "owner": 1, "kind": kind, "hp": 100 if kind == 1 else 200,
            "position": position, "direction": dict(ZERO), "cargo": 0, "rally": False,
            "rallyPoint": dict(ZERO), "guard": False, "post": dict(ZERO), "target": -1, "returning": False}


def check_before(state):
    require({p["owner"] for p in state["players"]} == set(TEAMS), "before: exact owner membership")
    require(player(state, 1)["credits"] == 100 and player(state, 1)["gathered"] == 450,
            "before: paid income/debit state")
    require(player(state, 2)["credits"] == 350 and player(state, 2)["gathered"] == 0
            and player(state, 100)["credits"] == 150 and player(state, 100)["gathered"] == 0,
            "before: unrelated owner economy / bot paid setup")
    # Three public seeds contain 500 ore each. A snapshot checks conservation,
    # not the temporal provenance of every gather/build/damage operation.
    require(sum(r["remaining"] for r in state["resources"])
            + sum(u["cargo"] for u in state["units"])
            + sum(p["gathered"] for p in state["players"]) == 1500,
            "before: seeded ore conservation")
    require(len(units(state, 1, live=True)) == 6 and not units(state, 1, 1, True)
            and len(units(state, 1, 1)) == 2 and all(u["hp"] == 0 for u in units(state, 1, 1)),
            "before: two retained dead miners and six live records")
    require(len(units(state, 1, 2, True)) == len(units(state, 1, 3, True)) == 1
            and len(units(state, 1, 4, True)) == 4 and units(state, 100, 0, True),
            "before: HQ/committed anchor/blockers/attacker")
    hq = units(state, 1, 2, True)[0]["position"]
    anchor = units(state, 1, 3, True)[0]["position"]
    require(hq == {"x": -1400, "y": -400, "z": 0}
            and anchor == {"x": -600, "y": -400, "z": 0}, "before: original anchors")
    for dx, dy in OFFSETS:
        candidate = {"x": hq["x"] + dx, "y": hq["y"] + dy, "z": hq["z"]}
        require(any(distance_squared(candidate, u["position"]) < 250 ** 2 for u in structures(state)),
                f"before: HQ offset {(dx, dy)} open")
    require(distance_squared(SITE, hq) > 800 ** 2 and distance_squared(SITE, anchor) <= 800 ** 2,
            "before: fallback must extend own committed anchor")
    require(all(distance_squared(SITE, u["position"]) >= 180 ** 2 for u in structures(state)),
            "before: fallback occupied")
    require(all(distance_squared(SITE, u["position"]) > 800 ** 2 for u in structures(state, 2)),
            "before: negative owner has its own reachable anchor")
    require(player(state, 2)["credits"] >= 100 and len(units(state, 2, live=True)) <= 10
            and units(state, 2, 2, True), "before: negative control's unrelated build prerequisites")


def check_after(before, after):
    next_id = max(u["id"] for u in before["units"]) + 1
    require(next_id <= MAX_INT32 - 2, "grant ID capacity")
    expected = copy.deepcopy(before)
    expected["revision"] += 3
    player(expected, 1)["credits"] -= 100
    expected["units"] += [new_unit(next_id, 3, dict(SITE)), new_unit(next_id + 1, 1, expected_spawn(before, SITE))]
    expected["digest"] = after["digest"]
    require(expected == after, "after: exact paid full-state delta/default metadata/spawn")


def selected_snapshot(record, revision, label):
    matches = [(line, row["state"]) for line, row in record["snapshots"] if row["state"]["revision"] == revision]
    require(len(matches) == 1, f"{label}: missing/duplicate selected snapshot")
    return matches[0]


def verify(host: Path, owner1: Path, owner2: Path, run: str, source: str):
    require(type(run) is str and TOKEN32.fullmatch(run), "run token")
    require(type(source) is str and TOKEN40.fullmatch(source), "source token")
    identities = StateIdentities()
    logs = {role: read_role(path, role, viewer, run, source, identities)
            for role, path, viewer in (("host", host, 0), ("owner1", owner1, 1), ("owner2", owner2, 2))}
    h = logs["host"]
    before_line, before_marker = h["markers"][BEFORE]
    _, complete = h["markers"][COMPLETE]
    before_rev = marker_int(before_marker, "revision", 1)
    owner_sequence = marker_int(before_marker, "ownerSequence", 1)
    require(owner_sequence < MAX_INT32, "owner sequence exhausted")
    after_rev = marker_int(complete, "afterRevision", 1)
    require(marker_int(before_marker, "epoch", 1) == marker_int(complete, "epoch", 1) == 1
            and marker_int(complete, "beforeRevision", 1) == before_rev
            and complete["role"] == "host" and complete["status"] == "pass", "host completion/marker identity")
    pre_line, before = selected_snapshot(h, before_rev, "host-before")
    post_line, after = selected_snapshot(h, after_rev, "host-after")
    require(before_marker["digest"] == before["digest"] and complete["afterDigest"] == after["digest"], "host marker digests")
    check_before(before)
    check_after(before, after)
    spawn = expected_spawn(before, SITE)
    require(all(re.fullmatch(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?", before_marker[k])
                and parse_decimal(before_marker[k]) == spawn[axis]
                for k, axis in (("grantX", "x"), ("grantY", "y"))), "preflight grant marker/policy mismatch")
    require(len(h["commands"]) == 2, "host: exactly two post-setup observed commands")
    (neg_line, negative), (pos_line, positive) = h["commands"]
    for command, owner, sequence, accepted, reason in (
            (negative, 2, 1, False, "placement"), (positive, 1, owner_sequence + 1, True, "accepted")):
        require(command["owner"] == owner and command["sequence"] == sequence
                and command["accepted"] is accepted and command["reason"] == reason
                and command["destination"] == SITE and command["before"] == before
                and command["after"] == (after if accepted else before), "host command payload/result/full-state binding")
    require(pre_line < before_line < neg_line < pos_line < post_line < h["finish"][0], "host causal ordering")
    require(h["snapshots"][-1][1]["state"] == after
            and [r["event"] for line, r in h["rows"] if line >= pre_line]
            == ["snapshot", "command", "command", "snapshot", "finish"], "host: unexpected recovery observation")
    acknowledgements = h["markers"][ACK]
    require(len(acknowledgements) == 2, "host: both client acknowledgements required")
    acknowledged = set()
    for ack_line, ack in acknowledgements:
        owner = marker_int(ack, "owner", 1)
        require(owner in (1, 2) and owner not in acknowledged
                and marker_int(ack, "epoch", 1) == 1 and marker_int(ack, "revision", 1) == after_rev
                and ack["digest"] == after["digest"] and h["finish"][0] < ack_line < h["markers"][COMPLETE][0],
                "host: acknowledgement owner/state/order binding")
        acknowledged.add(owner)
    for role, owner, command, positive_send in (("owner1", 1, positive, 1), ("owner2", 2, negative, 0)):
        record = logs[role]
        pre_line, pre = selected_snapshot(record, before_rev, role + "-before")
        post_line, post = selected_snapshot(record, after_rev, role + "-after")
        require(pre == before and post == after, f"{role}: full replicated states differ")
        ready_line, ready = record["markers"][READY]
        send_line, send = record["markers"][SEND]
        receipt_line, receipt = record["markers"][RECEIPT]
        observed_line, observed = record["markers"][OBSERVED]
        complete_line, done = record["markers"][COMPLETE]
        require(marker_int(receipt, "owner", 1) == owner and marker_int(receipt, "epoch", 1) == 1
                and marker_int(receipt, "sequence", 1) == command["resultSequence"]
                and marker_int(receipt, "accepted") == positive_send and receipt["reason"] == command["reason"],
                f"{role}: receipt/authoritative result binding")
        require(marker_int(observed, "owner", 1) == owner and marker_int(observed, "epoch", 1) == 1
                and marker_int(observed, "revision", 1) == after_rev and observed["digest"] == after["digest"],
                f"{role}: observed state identity")
        for cue in (ready, send):
            require(marker_int(cue, "owner", 1) == owner and marker_int(cue, "epoch", 1) == 1
                    and marker_int(cue, "revision", 1) == before_rev
                    and marker_int(cue, "sequence", 1) == command["sequence"]
                    and marker_int(cue, "positive") == positive_send, f"{role}: READY/SEND/host command binding")
        require(done["role"] == "client" and done["status"] == "pass"
                and marker_int(done, "owner", 1) == owner and marker_int(done, "epoch", 1) == 1
                and marker_int(done, "revision", 1) == after_rev and done["digest"] == after["digest"],
                f"{role}: completion identity")
        require(pre_line < send_line < post_line < receipt_line < record["finish"][0] < observed_line < complete_line
                and ready_line < send_line,
                f"{role}: READY/pre-state/SEND/post-state ordering")
        require(record["snapshots"][-1][1]["state"] == after
                and [r["event"] for line, r in record["rows"] if line >= pre_line]
                == ["snapshot", "snapshot", "finish"], f"{role}: unexpected recovery observation")
    return {"status": "pass", "run": run, "source": source, "epoch": 1,
            "before_revision": before_rev, "after_revision": after_rev, "after_digest": after["digest"],
            "observed_host_commands": 2, "replicated_clients": 2,
            "role_counts": {role: {k: record["finish"][1][k] for k in ("snapshots", "commands")}
                            for role, record in logs.items()},
            "digest_validation": "opaque native tokens bound to complete observed states; not recomputed",
            "proof_scope": "scripted public paid recovery and three-role consistency through explicit Playing-state close"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("host", "owner1", "owner2"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--source", required=True)
    args = parser.parse_args()
    try:
        result = verify(args.host, args.owner1, args.owner2, args.run, args.source)
    except (ValueError, OSError, UnicodeError, InvalidOperation, RecursionError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
