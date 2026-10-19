"""Permanent bounded synthetic controls. These are NOT Unreal runtime evidence.

Run: python test_parse_bot_base_expansion.py [--parser PATH] [--output RESULTS.json]
Fixtures use native field/order contracts and realistic seeded startup snapshots.
Synthetic digest tokens deliberately do not claim native ComputeDigest parity.
"""
from __future__ import annotations

import argparse
import copy
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
RUN = "0123456789abcdef0123456789abcdef"
SOURCE = "87131b3f8bb7df1d3e26931aa0e006ab30406c8a"
M = runpy.run_path(str(ROOT / "parse-bot-base-expansion.py"))
PREFIX = "[2026.10.19-13.00.00:123][ 42]LogTemp: Display: "

# Frozen output of the actual setup/model/planner source-extracted portable export.
# This fixture is not Unreal, transport, replication, or gameplay-runtime evidence.
SOURCE_BOUND_SHA256 = '54b19ebe7e46693b5cf788d2fb4da91fed837bfbe046fbd1a7bfa88bca50ab13'
SOURCE_BOUND_JSON = '{"ownerSequence":11,"simulatedSeconds":146,"rawBeforeDigest":"1a7ef4608d77434a5cc6ef00efd8c2c3","rawAfterDigest":"cf89161192a5617299c2bf050974a190","states":[{"epoch":1,"revision":4,"digest":"6f2b9ac1ddd7489cbefa6a152ee518b0","skirmish":true,"phase":1,"winner":-1,"players":[{"owner":100,"team":1,"credits":350,"gathered":0}],"units":[{"id":1,"owner":100,"kind":2,"hp":300,"position":{"x":1400,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":2,"owner":100,"kind":1,"hp":100,"position":{"x":1500,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false}],"resources":[{"id":1,"owner":100,"position":{"x":1950,"y":0,"z":0},"remaining":500}]},{"epoch":1,"revision":7,"digest":"a751474e4d1f240cf7dc04593b5a261f","skirmish":true,"phase":1,"winner":-1,"players":[{"owner":100,"team":1,"credits":350,"gathered":0},{"owner":1,"team":0,"credits":350,"gathered":0}],"units":[{"id":1,"owner":100,"kind":2,"hp":300,"position":{"x":1400,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":2,"owner":100,"kind":1,"hp":100,"position":{"x":1500,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":3,"owner":1,"kind":2,"hp":300,"position":{"x":-1400,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":4,"owner":1,"kind":1,"hp":100,"position":{"x":-1300,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false}],"resources":[{"id":1,"owner":100,"position":{"x":1950,"y":0,"z":0},"remaining":500},{"id":2,"owner":1,"position":{"x":-850,"y":-400,"z":0},"remaining":500}]},{"epoch":1,"revision":10,"digest":"c2621f14833eb503465ea1518c366d2c","skirmish":true,"phase":1,"winner":-1,"players":[{"owner":100,"team":1,"credits":350,"gathered":0},{"owner":1,"team":0,"credits":350,"gathered":0},{"owner":2,"team":0,"credits":350,"gathered":0}],"units":[{"id":1,"owner":100,"kind":2,"hp":300,"position":{"x":1400,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":2,"owner":100,"kind":1,"hp":100,"position":{"x":1500,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":3,"owner":1,"kind":2,"hp":300,"position":{"x":-1400,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":4,"owner":1,"kind":1,"hp":100,"position":{"x":-1300,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":5,"owner":2,"kind":2,"hp":300,"position":{"x":-1400,"y":600,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":6,"owner":2,"kind":1,"hp":100,"position":{"x":-1300,"y":600,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false}],"resources":[{"id":1,"owner":100,"position":{"x":1950,"y":0,"z":0},"remaining":500},{"id":2,"owner":1,"position":{"x":-850,"y":-400,"z":0},"remaining":500},{"id":3,"owner":2,"position":{"x":-850,"y":600,"z":0},"remaining":500}]},{"epoch":1,"revision":183,"digest":"6d5d1c04888fb7ccd6eb5701c348c1b6","skirmish":true,"phase":1,"winner":-1,"players":[{"owner":100,"team":1,"credits":150,"gathered":0},{"owner":1,"team":0,"credits":100,"gathered":450},{"owner":2,"team":0,"credits":350,"gathered":0}],"units":[{"id":1,"owner":100,"kind":2,"hp":300,"position":{"x":1400,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":2,"owner":100,"kind":1,"hp":100,"position":{"x":1500,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":3,"owner":1,"kind":2,"hp":300,"position":{"x":-1400,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":4,"owner":1,"kind":1,"hp":0,"position":{"x":-1400,"y":1400,"z":0},"direction":{"x":-0.055470019622541056,"y":0.99846035320541138,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":5,"owner":2,"kind":2,"hp":300,"position":{"x":-1400,"y":600,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":6,"owner":2,"kind":1,"hp":100,"position":{"x":-1300,"y":600,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":7,"owner":1,"kind":3,"hp":200,"position":{"x":-600,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":8,"owner":1,"kind":1,"hp":0,"position":{"x":-1400,"y":1400,"z":0},"direction":{"x":-0.40613846605344656,"y":0.91381154862025771,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":9,"owner":1,"kind":4,"hp":200,"position":{"x":-975,"y":-225,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":10,"owner":1,"kind":4,"hp":200,"position":{"x":-1575,"y":25,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":11,"owner":1,"kind":4,"hp":200,"position":{"x":-1825,"y":-575,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":12,"owner":1,"kind":4,"hp":200,"position":{"x":-1225,"y":-825,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":13,"owner":100,"kind":4,"hp":200,"position":{"x":1050,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":14,"owner":100,"kind":0,"hp":100,"position":{"x":-1150.3849116986471,"y":1566.4100588675685,"z":0},"direction":{"x":-0.83205029433784061,"y":-0.5547001962252337,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false}],"resources":[{"id":1,"owner":100,"position":{"x":1950,"y":0,"z":0},"remaining":500},{"id":2,"owner":1,"position":{"x":-850,"y":-400,"z":0},"remaining":50},{"id":3,"owner":2,"position":{"x":-850,"y":600,"z":0},"remaining":500}]},{"epoch":1,"revision":186,"digest":"4159980e0d2c8b24b859215e5adfcefc","skirmish":true,"phase":1,"winner":-1,"players":[{"owner":100,"team":1,"credits":150,"gathered":0},{"owner":1,"team":0,"credits":0,"gathered":450},{"owner":2,"team":0,"credits":350,"gathered":0}],"units":[{"id":1,"owner":100,"kind":2,"hp":300,"position":{"x":1400,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":2,"owner":100,"kind":1,"hp":100,"position":{"x":1500,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":3,"owner":1,"kind":2,"hp":300,"position":{"x":-1400,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":4,"owner":1,"kind":1,"hp":0,"position":{"x":-1400,"y":1400,"z":0},"direction":{"x":-0.055470019622541056,"y":0.99846035320541138,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":5,"owner":2,"kind":2,"hp":300,"position":{"x":-1400,"y":600,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":6,"owner":2,"kind":1,"hp":100,"position":{"x":-1300,"y":600,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":7,"owner":1,"kind":3,"hp":200,"position":{"x":-600,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":8,"owner":1,"kind":1,"hp":0,"position":{"x":-1400,"y":1400,"z":0},"direction":{"x":-0.40613846605344656,"y":0.91381154862025771,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":9,"owner":1,"kind":4,"hp":200,"position":{"x":-975,"y":-225,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":10,"owner":1,"kind":4,"hp":200,"position":{"x":-1575,"y":25,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":11,"owner":1,"kind":4,"hp":200,"position":{"x":-1825,"y":-575,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":12,"owner":1,"kind":4,"hp":200,"position":{"x":-1225,"y":-825,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":13,"owner":100,"kind":4,"hp":200,"position":{"x":1050,"y":0,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":14,"owner":100,"kind":0,"hp":100,"position":{"x":-1150.3849116986471,"y":1566.4100588675685,"z":0},"direction":{"x":-0.83205029433784061,"y":-0.5547001962252337,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":15,"owner":1,"kind":3,"hp":200,"position":{"x":-250,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false},{"id":16,"owner":1,"kind":1,"hp":100,"position":{"x":50,"y":-400,"z":0},"direction":{"x":0,"y":0,"z":0},"cargo":0,"rally":false,"rallyPoint":{"x":0,"y":0,"z":0},"guard":false,"post":{"x":0,"y":0,"z":0},"target":-1,"returning":false}],"resources":[{"id":1,"owner":100,"position":{"x":1950,"y":0,"z":0},"remaining":500},{"id":2,"owner":1,"position":{"x":-850,"y":-400,"z":0},"remaining":50},{"id":3,"owner":2,"position":{"x":-850,"y":600,"z":0},"remaining":500}]}]}'
SOURCE_BOUND = json.loads(SOURCE_BOUND_JSON, parse_float=Decimal)


def pt(x=0, y=0, z=0):
    return {"x": x, "y": y, "z": z}


def unit(i, owner, kind, x, y, hp=200):
    return dict(id=i, owner=owner, kind=kind, hp=hp, position=pt(x, y), direction=pt(),
                cargo=0, rally=False, rallyPoint=pt(), guard=False, post=pt(), target=-1, returning=False)


def token(state):
    """Synthetic identity only, not the native canonical digest algorithm."""
    body = {k: v for k, v in state.items() if k != "digest"}
    return hashlib.md5(json.dumps(body, sort_keys=True, separators=(",", ":"), allow_nan=True).encode()).hexdigest()


def states():
    before = dict(epoch=1, revision=200, skirmish=True, phase=1, winner=-1,
        players=[dict(owner=100, team=1, credits=150, gathered=0),
                 dict(owner=1, team=0, credits=100, gathered=450),
                 dict(owner=2, team=0, credits=350, gathered=0)],
        units=[unit(1, 100, 2, 1400, 0, 300), unit(2, 100, 1, 1500, 0, 100),
               unit(3, 1, 2, -1400, -400, 300), unit(4, 1, 1, -1400, 1400, 0),
               unit(5, 2, 2, -1400, 600, 300), unit(6, 2, 1, -1300, 600, 100),
               unit(7, 1, 3, -600, -400), unit(8, 1, 1, -1400, 1400, 0),
               unit(9, 1, 4, -975, -225), unit(10, 1, 4, -1575, 25),
               unit(11, 1, 4, -1825, -575), unit(12, 1, 4, -1225, -825),
               unit(13, 100, 4, 1050, 0), unit(14, 100, 0, -1400, 1100, 100)],
        resources=[dict(id=1, owner=100, position=pt(1950, 0), remaining=500),
                   dict(id=2, owner=1, position=pt(-850, -400), remaining=50),
                   dict(id=3, owner=2, position=pt(-850, 600), remaining=500)])
    for u in before["units"]:
        if u["id"] in (4, 8, 14):
            u["direction"] = pt(0, 1)
    after = copy.deepcopy(before)
    after["revision"] += 3
    next(p for p in after["players"] if p["owner"] == 1)["credits"] = 0
    after["units"] += [unit(15, 1, 3, -250, -400), unit(16, 1, 1, 50, -400, 100)]
    return before, after


def startup_states():
    b, _ = states()
    result = []
    for revision, owners in ((4, {100}), (7, {100, 1}), (10, {100, 1, 2})):
        s = copy.deepcopy(b)
        s["revision"] = revision
        s["players"] = [p for p in s["players"] if p["owner"] in owners]
        for p in s["players"]:
            p.update(credits=350, gathered=0)
        s["units"] = [u for u in s["units"] if u["id"] <= 6 and u["owner"] in owners]
        for u in s["units"]:
            if u["id"] == 4:
                u.update(hp=100, position=pt(-1300, -400), direction=pt())
        s["resources"] = [r for r in s["resources"] if r["owner"] in owners]
        for r in s["resources"]:
            r["remaining"] = 500
        s["digest"] = token(s)
        result.append(s)
    return result


def logs(state_change=None, source_bound=False):
    b, a = copy.deepcopy(SOURCE_BOUND["states"][-2:]) if source_bound else states()
    if state_change:
        state_change(b, a)
    if not source_bound:
        b["digest"], a["digest"] = token(b), token(a)
    before_rev, after_rev = b["revision"], a["revision"]
    owner_seq = SOURCE_BOUND["ownerSequence"] if source_bound else 30
    def row(role, viewer, event, **kw):
        return dict(v=1, run=RUN, source=SOURCE, role=role, viewer=viewer, event=event, **kw)
    def snapshot(role, viewer, s):
        return row(role, viewer, "snapshot", state=copy.deepcopy(s))
    def finish(role, viewer, snapshots, commands):
        return row(role, viewer, "finish", epoch=1, revision=a["revision"], digest=a["digest"], snapshots=snapshots, commands=commands)
    command = dict(type=3, payloadEpoch=1, unit=-1, target=-1, destination=pt(-250, -400), resultEpoch=1)
    neg = row("host", 0, "command", **command, owner=2, sequence=1, resultSequence=1,
              accepted=False, reason="placement", before=copy.deepcopy(b), after=copy.deepcopy(b))
    pos = row("host", 0, "command", **command, owner=1, sequence=owner_seq + 1, resultSequence=owner_seq + 1,
              accepted=True, reason="accepted", before=copy.deepcopy(b), after=copy.deepcopy(a))
    start = copy.deepcopy(SOURCE_BOUND["states"][:3]) if source_bound else startup_states()
    result = {"host": [snapshot("host", 0, s) for s in start] + [
        snapshot("host", 0, b),
        f"{M['BEFORE']}epoch=1 revision={before_rev} digest={b['digest']} ownerSequence={owner_seq} grantX=50 grantY=-400",
        neg, pos, snapshot("host", 0, a), finish("host", 0, 5, 2),
        f"{M['ACK']}owner=2 epoch=1 revision={after_rev} digest={a['digest']}",
        f"{M['ACK']}owner=1 epoch=1 revision={after_rev} digest={a['digest']}",
        f"{M['COMPLETE']}role=host status=pass epoch=1 beforeRevision={before_rev} afterRevision={after_rev} afterDigest={a['digest']}"]}
    for owner, seq in ((1, owner_seq + 1), (2, 1)):
        role = f"owner{owner}"
        positive = int(owner == 1)
        result[role] = [snapshot("client", owner, start[-1]), snapshot("client", owner, b),
            f"{M['READY']}owner={owner} epoch=1 revision={before_rev} sequence={seq} positive={positive}",
            f"{M['SEND']}owner={owner} epoch=1 revision={before_rev} sequence={seq} positive={positive}",
            snapshot("client", owner, a),
            f"{M['RECEIPT']}owner={owner} epoch=1 sequence={seq} accepted={positive} reason={'accepted' if positive else 'placement'}",
            finish("client", owner, 3, 0),
            f"{M['OBSERVED']}owner={owner} epoch=1 revision={after_rev} digest={a['digest']}",
            f"{M['COMPLETE']}role=client status=pass owner={owner} epoch=1 revision={after_rev} digest={a['digest']}"]
    return result


def events(data, role, event):
    return [r for r in data[role] if type(r) is dict and r["event"] == event]


def marker_index(data, role, marker):
    return next(i for i, item in enumerate(data[role]) if type(item) is str and item.startswith(M[marker]))


def alter_marker(data, role, marker, old, new):
    i = marker_index(data, role, marker)
    assert old in data[role][i]
    data[role][i] = data[role][i].replace(old, new)


def remove_marker(data, role, marker):
    data[role].pop(marker_index(data, role, marker))


def snapshot(data, role, index):
    return events(data, role, "snapshot")[index]["state"]


def exact_json(value):
    if type(value) is Decimal:
        return str(value)
    if type(value) is dict:
        return "{" + ",".join(json.dumps(k) + ":" + exact_json(v) for k, v in value.items()) + "}"
    if type(value) is list:
        return "[" + ",".join(exact_json(v) for v in value) + "]"
    return json.dumps(value, separators=(",", ":"), allow_nan=True)


def serialize(data, prefix="", newline="\n"):
    result = {}
    for role, entries in data.items():
        lines = ["LogInit: Synthetic parser control, not a runtime log"]
        count = 0
        for item in entries:
            if type(item) is dict:
                count += 1
                item = dict(item, row=count)
                line = M["MARKER"] + exact_json(item)
            else:
                line = item
            lines.append(prefix + line)
        result[role] = newline.join(lines) + newline
    return result


def both_states(b, a, action):
    action(b)
    action(a)


def owner(state, which):
    return next(p for p in state["players"] if p["owner"] == which)


def new_records(a):
    return a["units"][-2:]


CASES = []

def case(name, accepted=False, state=None, log=None, text=None, source_bound=False):
    CASES.append((name, accepted, state, log, text, source_bound))


case("valid_complete_startup", True)
case("valid_real_ue_prefix_crlf", True, text=lambda d: {r: t.replace("\n", "\r\n").replace("PROOF_", PREFIX + "PROOF_") for r, t in d.items()})
case("valid_ready_before_replication", True, log=lambda d: d["owner2"].insert(1, d["owner2"].pop(marker_index(d, "owner2", "READY"))))
case("valid_ack_reverse_order", True, log=lambda d: d["host"].insert(marker_index(d, "host", "ACK"), d["host"].pop(marker_index(d, "host", "ACK") + 1)))
case("valid_fractional_nondefault_old_metadata", True, state=lambda b, a: both_states(b, a, lambda s: s["units"][3].update(direction=pt(0.6, 0.8))))
case("valid_numeric_equivalent_vectors", True, text=lambda d: {r: t.replace('"x":-1400,', '"x":-1400.0,') for r, t in d.items()})
case("valid_ready_before_first_snapshot", True, log=lambda d: d["owner1"].insert(0, d["owner1"].pop(marker_index(d, "owner1", "READY"))))
case("valid_source_bound_fixture", True, source_bound=True)
case("valid_source_bound_ue_prefix", True, source_bound=True, text=lambda d: {r: t.replace("\n", "\r\n").replace("PROOF_", PREFIX + "PROOF_") for r, t in d.items()})
case("source_bound_raw_epoch_before_digest", source_bound=True, log=lambda d: alter_marker(d, "host", "BEFORE", "digest=" + SOURCE_BOUND["states"][-2]["digest"], "digest=" + SOURCE_BOUND["rawBeforeDigest"]))
case("source_bound_raw_epoch_after_digest", source_bound=True, log=lambda d: alter_marker(d, "host", "COMPLETE", "afterDigest=" + SOURCE_BOUND["states"][-1]["digest"], "afterDigest=" + SOURCE_BOUND["rawAfterDigest"]))
case("source_bound_wrong_full_state", source_bound=True, log=lambda d: snapshot(d, "owner1", -2)["units"][-1]["direction"].update(x=Decimal("-0.8320502943378406000000001")))
case("missing_finish", log=lambda d: d["host"].remove(events(d, "host", "finish")[0]))
case("duplicate_finish", log=lambda d: d["host"].insert(-3, copy.deepcopy(events(d, "host", "finish")[0])))
case("empty_finish", log=lambda d: events(d, "host", "finish")[0].update(epoch=None, revision=None, snapshots=None, commands=None, digest=None))
case("finish_snapshot_count", log=lambda d: events(d, "owner1", "finish")[0].update(snapshots=2))
case("finish_command_count", log=lambda d: events(d, "host", "finish")[0].update(commands=0))
case("finish_wrong_epoch", log=lambda d: events(d, "owner2", "finish")[0].update(epoch=2))
case("finish_wrong_revision", log=lambda d: events(d, "host", "finish")[0].update(revision=204))
case("finish_wrong_digest", log=lambda d: events(d, "host", "finish")[0].update(digest="f" * 32))
case("finish_unknown_field", log=lambda d: events(d, "host", "finish")[0].update(padding="x"))
case("event_after_finish", log=lambda d: d["host"].insert(-3, copy.deepcopy(events(d, "host", "snapshot")[-1])))
case("command_after_finish", log=lambda d: d["host"].insert(-3, d["host"].pop(d["host"].index(events(d, "host", "command")[0]))))
case("extra_command", log=lambda d: d["host"].insert(6, copy.deepcopy(events(d, "host", "command")[0])))
case("unobserved_command_type", log=lambda d: events(d, "host", "command")[0].update(type=6))
case("client_command", log=lambda d: d["owner1"].insert(2, copy.deepcopy(events(d, "host", "command")[0])))
case("unknown_event", log=lambda d: events(d, "host", "snapshot")[0].update(event="heartbeat"))
case("malformed_startup_snapshot", log=lambda d: events(d, "host", "snapshot")[0].update(state=dict(epoch=99, revision=1)))
case("startup_epoch_conflict", log=lambda d: snapshot(d, "host", 0).update(epoch=2))
case("startup_duplicate_revision", log=lambda d: snapshot(d, "host", 1).update(revision=4))
case("startup_conflicting_same_revision", log=lambda d: snapshot(d, "owner1", 0)["players"][0].update(credits=349))
case("startup_unknown_owner", log=lambda d: snapshot(d, "host", 0)["players"][0].update(owner=999))
case("after_before_snapshot_order", log=lambda d: d["host"].insert(0, d["host"].pop(d["host"].index(events(d, "host", "snapshot")[-1]))))
case("positive_before_negative", log=lambda d: d["host"].insert(5, d["host"].pop(6)))
case("completion_before_all_rows", log=lambda d: d["host"].insert(0, d["host"].pop()))
case("observer_error", log=lambda d: d["host"].append("PROOF_BASE_EXPANSION_OBSERVATION_ERROR reason=stream-cap"))
case("fixture_error", log=lambda d: d["owner1"].append("PROOF_BOT_BASE_EXPANSION_ERROR reason=bad"))
case("explicit_failure", log=lambda d: alter_marker(d, "host", "COMPLETE", "status=pass", "status=fail"))
case("missing_reserved_separator", log=lambda d: d["host"].append("PROOF_BASE_EXPANSION_OBSERVATION{malformed"))
case("unknown_reserved_family", log=lambda d: d["host"].append("PROOF_BOT_BASE_EXPANSION_FUTURE value=1"))
case("row_over_65536", log=lambda d: events(d, "host", "finish")[0].update(padding="x" * 65537))
case("log_line_over_cap", text=lambda d: {r: t + ("x" * (M["MAX_LINE_BYTES"] + 1) + "\n" if r == "host" else "") for r, t in d.items()})
case("truncated_last_line", text=lambda d: {r: t[:-1] if r == "owner2" else t for r, t in d.items()})
case("truncated_json", text=lambda d: {r: t.replace('"row":1}', '"row":1') if r == "host" else t for r, t in d.items()})
case("duplicate_json_key", text=lambda d: {r: t.replace('"owner":1,', '"owner":999,"owner":1,') for r, t in d.items()})
case("duplicate_marker_key", log=lambda d: alter_marker(d, "owner1", "SEND", "sequence=31", "sequence=999 sequence=31"))
case("marker_unparsed_junk", log=lambda d: alter_marker(d, "owner1", "SEND", "sequence=31", "junk sequence=31"))
case("multiple_markers_one_line", text=lambda d: {r: t.replace("PROOF_BOT_BASE_EXPANSION_SEND", "PROOF_BOT_BASE_EXPANSION_READY PROOF_BOT_BASE_EXPANSION_SEND") if r == "owner1" else t for r, t in d.items()})
case("wrong_run", log=lambda d: events(d, "host", "snapshot")[0].update(run="f" * 32))
case("wrong_source", log=lambda d: events(d, "owner2", "snapshot")[0].update(source="f" * 40))
case("wrong_role", log=lambda d: events(d, "host", "snapshot")[0].update(role="client"))
case("wrong_viewer", log=lambda d: events(d, "owner1", "snapshot")[0].update(viewer=2))
case("bool_version", log=lambda d: events(d, "host", "snapshot")[0].update(v=True))
case("bool_viewer", log=lambda d: events(d, "owner1", "snapshot")[0].update(viewer=True))
case("row_discontinuity", text=lambda d: {r: t.replace('"row":2}', '"row":5}') if r == "host" else t for r, t in d.items()})
case("after_skirmish", state=lambda b, a: a.update(skirmish=False))
case("after_winner", state=lambda b, a: a.update(winner=1))
case("after_phase", state=lambda b, a: a.update(phase=2))
case("owner1_team", state=lambda b, a: owner(a, 1).update(team=1))
case("unexpected_player", state=lambda b, a: a["players"].append(dict(owner=999, team=9, credits=999999, gathered=0)))
case("negative_owner_team", state=lambda b, a: both_states(b, a, lambda s: owner(s, 2).update(team=1)))
case("foreign_unit_owner", state=lambda b, a: a["units"][-1].update(owner=2))
case("prestate_minted_ore", state=lambda b, a: both_states(b, a, lambda s: s["resources"][0].update(remaining=501)))
case("prestate_minted_cargo", state=lambda b, a: both_states(b, a, lambda s: s["units"][1].update(cargo=1)))
case("prestate_owner2_debit", state=lambda b, a: both_states(b, a, lambda s: owner(s, 2).update(credits=349)))
case("prestate_bot_debit", state=lambda b, a: both_states(b, a, lambda s: owner(s, 100).update(credits=149)))
case("prestate_unrelated_gather", state=lambda b, a: both_states(b, a, lambda s: (owner(s, 2).update(gathered=1), s["resources"][2].update(remaining=499))))
case("resource_mutation", state=lambda b, a: a["resources"][1].update(remaining=49))
case("unrelated_credits", state=lambda b, a: owner(a, 100).update(credits=151))
case("negative_owner_changed", state=lambda b, a: owner(a, 2).update(gathered=1))
case("wrong_debit", state=lambda b, a: owner(a, 1).update(credits=1))
case("minted_ore", state=lambda b, a: owner(a, 1).update(gathered=451))
case("revision_delta", state=lambda b, a: a.update(revision=204))
case("changed_old_unit", state=lambda b, a: a["units"][0].update(hp=299))
case("new_refinery_cargo", state=lambda b, a: new_records(a)[0].update(cargo=1))
case("new_miner_hp", state=lambda b, a: new_records(a)[1].update(hp=101))
case("new_miner_direction", state=lambda b, a: new_records(a)[1].update(direction=pt(1, 0)))
case("new_refinery_disabled_rally_point", state=lambda b, a: new_records(a)[0].update(rallyPoint=pt(1, 0)))
case("new_miner_guard_metadata", state=lambda b, a: new_records(a)[1].update(guard=True, post=pt(1, 2), target=14, returning=True))
case("new_refinery_rally", state=lambda b, a: new_records(a)[0].update(rally=True))
case("new_miner_nondefault_target", state=lambda b, a: new_records(a)[1].update(target=14))
case("new_miner_wrong_id", state=lambda b, a: new_records(a)[1].update(id=17))
case("missing_dead_miners", state=lambda b, a: both_states(b, a, lambda s: s.update(units=[u for u in s["units"] if u["id"] not in (4, 8)])))
case("missing_winner", state=lambda b, a: both_states(b, a, lambda s: s.pop("winner")))
case("missing_unit_metadata", state=lambda b, a: both_states(b, a, lambda s: [u.pop("guard") for u in s["units"]]))
case("unknown_state_field", state=lambda b, a: both_states(b, a, lambda s: s.update(hidden=1)))
case("duplicate_unit_id", state=lambda b, a: both_states(b, a, lambda s: s["units"][1].update(id=1)))
case("duplicate_resource_id", state=lambda b, a: both_states(b, a, lambda s: s["resources"][1].update(id=1)))
case("duplicate_owner", state=lambda b, a: both_states(b, a, lambda s: s["players"].append(copy.deepcopy(s["players"][0]))))
case("boolean_epoch_owner", state=lambda b, a: both_states(b, a, lambda s: (s.update(epoch=True), owner(s, 1).update(owner=True))))
case("float_unit_id", state=lambda b, a: both_states(b, a, lambda s: s["units"][0].update(id=1.0)))
case("bool_sequence_epoch", log=lambda d: events(d, "host", "command")[0].update(sequence=True, payloadEpoch=True))
case("int32_overflow", log=lambda d: events(d, "host", "command")[1].update(sequence=2147483648))
case("decimal_outside_world_below_float_precision", text=lambda d: {r: t.replace('"x":1950,', '"x":10000.00000000000000000000000000000001,', 1) if r == "host" else t for r, t in d.items()})
case("impossible_startup_owner_membership", log=lambda d: snapshot(d, "host", 0).update(players=[], units=[], resources=[]))
case("nan_vector", state=lambda b, a: a["units"][-1].update(direction=pt(float("nan"), 0)))
case("infinity_vector", state=lambda b, a: a["units"][-1].update(rallyPoint=pt(float("inf"), 0)))
case("nonfinite_exponent", text=lambda d: {r: t.replace('"x":-1400,', '"x":1e9999,', 1) if r == "owner1" else t for r, t in d.items()})
case("decimal_subprecision_difference", text=lambda d: {r: t.replace('"x":-1400,', '"x":-1400.00000000000001,', 1) if r == "owner1" else t for r, t in d.items()})
case("colliding_spawn_matching_marker", state=lambda b, a: a["units"][-1].update(position=pt(-250, -400)), log=lambda d: alter_marker(d, "host", "BEFORE", "grantX=50", "grantX=-250"))
case("wrong_policy_preference", state=lambda b, a: a["units"][-1].update(position=pt(-250, -100)), log=lambda d: (alter_marker(d, "host", "BEFORE", "grantX=50", "grantX=-250"), alter_marker(d, "host", "BEFORE", "grantY=-400", "grantY=-100")))
case("negative_command_mutation", log=lambda d: events(d, "host", "command")[0]["after"]["players"][0].update(credits=999))
case("positive_result_reason", log=lambda d: events(d, "host", "command")[1].update(reason="placement"))
case("wrong_result_epoch", log=lambda d: events(d, "host", "command")[0].update(resultEpoch=2))
case("wrong_result_sequence", log=lambda d: events(d, "host", "command")[1].update(resultSequence=30))
case("missing_result_epoch", log=lambda d: events(d, "host", "command")[1].pop("resultEpoch"))
case("wrong_payload_destination", log=lambda d: events(d, "host", "command")[1].update(destination=pt(-251, -400)))
case("wrong_send_sequence", log=lambda d: alter_marker(d, "owner1", "SEND", "sequence=31", "sequence=99"))
case("missing_send_sequence", log=lambda d: alter_marker(d, "owner2", "SEND", "sequence=1 ", ""))
case("wrong_ready_epoch", log=lambda d: alter_marker(d, "owner2", "READY", "epoch=1", "epoch=2"))
case("wrong_before_epoch", log=lambda d: alter_marker(d, "host", "BEFORE", "epoch=1", "epoch=2"))
case("wrong_complete_epoch", log=lambda d: alter_marker(d, "owner1", "COMPLETE", "epoch=1", "epoch=2"))
case("wrong_host_complete_role", log=lambda d: alter_marker(d, "host", "COMPLETE", "role=host", "role=client"))
case("wrong_before_sequence", log=lambda d: alter_marker(d, "host", "BEFORE", "ownerSequence=30", "ownerSequence=29"))
case("ready_after_send", log=lambda d: d["owner1"].insert(4, d["owner1"].pop(marker_index(d, "owner1", "READY"))))
case("send_before_snapshot", log=lambda d: d["owner1"].insert(0, d["owner1"].pop(marker_index(d, "owner1", "SEND"))))
case("missing_ready", log=lambda d: remove_marker(d, "owner1", "READY"))
case("missing_ack", log=lambda d: remove_marker(d, "host", "ACK"))
case("duplicate_ack_owner", log=lambda d: alter_marker(d, "host", "ACK", "owner=2", "owner=1"))
case("wrong_ack_state", log=lambda d: alter_marker(d, "host", "ACK", "revision=203", "revision=200"))
case("ack_before_finish", log=lambda d: d["host"].insert(0, d["host"].pop(marker_index(d, "host", "ACK"))))
case("missing_receipt", log=lambda d: remove_marker(d, "owner2", "RECEIPT"))
case("wrong_receipt_sequence", log=lambda d: alter_marker(d, "owner1", "RECEIPT", "sequence=31", "sequence=1"))
case("wrong_receipt_outcome", log=lambda d: alter_marker(d, "owner2", "RECEIPT", "accepted=0", "accepted=1"))
case("wrong_receipt_epoch", log=lambda d: alter_marker(d, "owner2", "RECEIPT", "epoch=1", "epoch=2"))
case("wrong_observed_digest", log=lambda d: alter_marker(d, "owner1", "OBSERVED", "digest=" + token(states()[1]), "digest=" + "f" * 32))
case("observed_before_finish", log=lambda d: d["owner1"].insert(0, d["owner1"].pop(marker_index(d, "owner1", "OBSERVED"))))
case("invalid_digest_token", text=lambda d: {r: t.replace(token(states()[0]), "not-a-digest") for r, t in d.items()})
case("epoch_zero_marker_digest", log=lambda d: alter_marker(d, "host", "BEFORE", "digest=" + token(states()[0]), "digest=" + token(states()[0] | {"epoch": 0})))
case("same_digest_different_states", text=lambda d: {r: t.replace(token(states()[1]), token(states()[0])) for r, t in d.items()})
case("same_state_conflicting_digest", log=lambda d: snapshot(d, "owner1", -2).update(digest="a" * 32))
case("missing_post_snapshot", log=lambda d: d["owner2"].remove(events(d, "owner2", "snapshot")[-1]))


def policy_controls():
    results = []
    def check(name, action):
        try:
            action()
            results.append(dict(case=name, expected_accept=True, parser_accepted=True, matched=True))
        except Exception as error:
            results.append(dict(case=name, expected_accept=True, parser_accepted=False, matched=False, detail=str(error)))
    def expect_spawn(anchor, blockers, expected):
        state = {"units": [dict(position=p, hp=hp) for p, hp in blockers]}
        assert M["expected_spawn"](state, anchor) == expected
    check("policy_clear_first_direction", lambda: expect_spawn(pt(), [], pt(300, 0)))
    check("policy_axis_preference", lambda: expect_spawn(pt(), [(pt(300, 0), 1)], pt(-300, 0)))
    check("policy_3d_clearance", lambda: expect_spawn(pt(), [(pt(300, 0, 260), 1)], pt(300, 0)))
    check("policy_subprecision_inside_boundary", lambda: expect_spawn(pt(), [(pt(Decimal("559.99999999999999999999999999999999"), 0), 1)], pt(-300, 0)))
    check("policy_exact_boundary_260", lambda: expect_spawn(pt(), [(pt(560, 0), 1)], pt(300, 0)))
    check("policy_ignores_dead_records", lambda: expect_spawn(pt(), [(pt(300, 0), 0)], pt(300, 0)))
    check("policy_world_bounds", lambda: expect_spawn(pt(9900, 0), [], pt(9600, 0)))
    directions = M["DIRECTIONS"]
    check("policy_second_ring", lambda: expect_spawn(pt(), [(pt(dx * 300, dy * 300), 1) for dx, dy in directions], pt(600, 0)))
    check("policy_third_ring", lambda: expect_spawn(pt(), [(pt(dx * 300 * ring, dy * 300 * ring), 1) for ring in (1, 2) for dx, dy in directions], pt(900, 0)))
    def exhausted():
        try:
            expect_spawn(pt(), [(pt(dx * 300 * ring, dy * 300 * ring), 1) for ring in (1, 2, 3) for dx, dy in directions], None)
        except ValueError:
            return
        raise AssertionError("exhausted spawn accepted")
    check("policy_all_rings_exhausted", exhausted)
    return results


def main():
    global M
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--parser", type=Path, default=ROOT / "parse-bot-base-expansion.py")
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    M = runpy.run_path(str(args.parser))
    results = []
    with tempfile.TemporaryDirectory(prefix="pr70-parser-controls-") as directory:
        root = Path(directory)
        for name, expected, state_change, log_change, text_change, source_bound in CASES:
            data = logs(state_change, source_bound)
            if log_change:
                log_change(data)
            texts = serialize(data)
            if text_change:
                texts = text_change(texts)
            for role, value in texts.items():
                (root / (role + ".log")).write_bytes(value.encode("utf-8"))
            try:
                result = M["verify"](*(root / (r + ".log") for r in ("host", "owner1", "owner2")), RUN, SOURCE)
                accepted, detail = True, result["status"]
            except (ValueError, OSError, UnicodeError, RecursionError) as error:
                accepted, detail = False, str(error)
            results.append(dict(case=name, expected_accept=expected, parser_accepted=accepted,
                                matched=accepted == expected, detail=detail))
            print(f"{'PASS' if accepted == expected else 'FAIL'} {name}: {detail}")
        # Exercise each actual guard on small fixtures, scaling only its threshold.
        # This avoids materializing 32/64 MiB files or thousands of rows per run.
        namespace = M["read_role"].__globals__
        expected_caps = {"MAX_ROWS": 8192, "MAX_ROW_BYTES": 65536,
                         "MAX_OBSERVATION_BYTES": 32 * 1024 * 1024,
                         "MAX_LOG_BYTES": 64 * 1024 * 1024, "MAX_LINE_BYTES": 128 * 1024}
        assert all(namespace[k] == value for k, value in expected_caps.items())
        for role, value in serialize(logs()).items():
            (root / (role + ".log")).write_bytes(value.encode("utf-8"))
        for key, limit, expected_message in (("MAX_ROWS", 1, "observation cap"),
                ("MAX_ROW_BYTES", 128, "observation cap"),
                ("MAX_OBSERVATION_BYTES", 2048, "observation cap"),
                ("MAX_LOG_BYTES", 128, "missing/overlarge log"),
                ("MAX_LINE_BYTES", 128, "log/line cap")):
            old = namespace[key]
            namespace[key] = limit
            try:
                M["verify"](*(root / (r + ".log") for r in ("host", "owner1", "owner2")), RUN, SOURCE)
                rejected, detail = False, "accepted"
            except ValueError as error:
                detail = str(error)
                rejected = expected_message in detail
            finally:
                namespace[key] = old
            results.append(dict(case="scaled_" + key.lower(), expected_accept=False,
                                parser_accepted=not rejected, matched=rejected, detail=detail))
        command = [sys.executable, str(args.parser), "--host", str(root / "host.log"),
                   "--owner1", str(root / "owner1.log"), "--owner2", str(root / "owner2.log"),
                   "--run", RUN, "--source", SOURCE]
        valid = subprocess.run(command, capture_output=True, text=True, timeout=15)
        valid_ok = valid.returncode == 0 and json.loads(valid.stdout)["status"] == "pass"
        results.append(dict(case="cli_valid_exit_zero", expected_accept=True, parser_accepted=valid_ok, matched=valid_ok))
        with (root / "host.log").open("a") as handle:
            handle.write("PROOF_BASE_EXPANSION_OBSERVATION_ERROR reason=stream-cap\n")
        invalid = subprocess.run(command, capture_output=True, text=True, timeout=15)
        invalid_ok = invalid.returncode != 0 and not invalid.stdout and invalid.stderr.startswith("FAIL:")
        results.append(dict(case="cli_invalid_exit_nonzero_no_pass", expected_accept=False,
                            parser_accepted=not invalid_ok, matched=invalid_ok))
    results.extend(policy_controls())
    report = dict(label="Synthetic parser and independent spawn-policy controls only; no Unreal/native execution",
                  parser_sha256=hashlib.sha256(args.parser.read_bytes()).hexdigest(),
                  source_bound_fixture_sha256=SOURCE_BOUND_SHA256, count=len(results), matched=sum(r["matched"] for r in results), results=results)
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['matched']}/{report['count']} controls matched expected behavior")
    return 0 if all(r["matched"] for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
