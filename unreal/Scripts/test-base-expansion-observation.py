"""Pure-Python parser controls. Synthetic streams are NOT Unreal runtime evidence.

Run with: python test-base-expansion-observation.py
The fixtures use actual UE log prefixes/field spellings, including first-terminal
finish rows and ordinary completion suffixes. No engine or production files run.
"""
from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

PATH = Path(__file__).with_name("parse-base-expansion-observation.py")
SPEC = importlib.util.spec_from_file_location("expansion_parser", PATH)
PARSER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PARSER)
HEAD = "88701b60a665c72fbef03deaa61fc50b87e53581"
SOURCE = "acd09f416f773c34ecb2f731809981646c1e8d64"
RUN = "1" * 32
PREFIX = "[2026.10.19-11.00.00:000][ 42]LogTemp: Display: "
VIEWERS = {"host": 0, "rendered": 1, "observer": 2}


def point(x=0, y=0, z=0):
    return {"x": x, "y": y, "z": z}


def unit(unit_id, owner, kind, x, y=0, z=0):
    return dict(id=unit_id, owner=owner, kind=kind, hp=300 if kind == 2 else 200 if kind in (3, 4) else 100,
                position=point(x, y, z), direction=point(), cargo=0, rally=False, rallyPoint=point(),
                guard=False, post=point(), target=-1, returning=False)


def snapshot(revision, credits=350, extra=()):
    return dict(epoch=1, revision=revision, digest=f"{revision:032x}", skirmish=True, phase=1, winner=-1,
                players=[dict(owner=1, team=0, credits=350, gathered=0),
                         dict(owner=2, team=0, credits=credits, gathered=0),
                         dict(owner=100, team=1, credits=350, gathered=0)],
                units=[unit(1, 2, 2, 0), unit(2, 1, 2, 3000), unit(3, 2, 1, -300),
                       unit(4, 100, 2, -3000), unit(5, 100, 1, -3300)] + copy.deepcopy(list(extra)),
                resources=[dict(id=1, owner=2, position=point(700), remaining=500)])


def command(sequence, kind, x, before, after, accepted=True):
    return dict(event="command", owner=2, type=kind, sequence=sequence, payloadEpoch=1, unit=-1,
                target=-1, destination=point(x), accepted=accepted,
                reason="accepted" if accepted else "placement", before=copy.deepcopy(before), after=copy.deepcopy(after))


def snap(state):
    return {"event": "snapshot", "state": copy.deepcopy(state)}


def recount(fixture, role):
    """Explicit helper for constructing valid variant streams, never during verification."""
    events = fixture["events"][role]
    for i, row in enumerate(events, 1):
        row.update(v=1, run=RUN, source=SOURCE, role="host" if role == "host" else "client",
                   viewer=VIEWERS[role], row=i)
        if row["event"] == "finish":
            prior = events[:i - 1]
            last = prior[-1]["state"]
            row.update(epoch=last["epoch"], revision=last["revision"], digest=last["digest"],
                       snapshots=sum(e["event"] == "snapshot" for e in prior),
                       commands=sum(e["event"] == "command" for e in prior))


def fixture():
    refinery, miner, factory = unit(6, 2, 3, 350), unit(7, 2, 1, 650), unit(8, 2, 4, 1050)
    start = snapshot(9)
    before = snapshot(10)
    built = snapshot(13, 250, [refinery, miner])
    factory_before = snapshot(14, 250, [refinery, miner])
    expanded = snapshot(16, 100, [refinery, miner, factory])
    reject = snapshot(17, 100, [refinery, miner, factory])
    later = snapshot(18, 100, [refinery, miner, factory])
    terminal = snapshot(20, 100, [refinery, miner, factory])
    terminal.update(phase=2, winner=0)
    terminal["units"][3]["hp"] = 0
    commands = [command(1, 3, 350, before, built), command(2, 4, 1050, factory_before, expanded),
                command(3, 3, 2300, reject, reject, False)]
    snapshots = [start, built, expanded, reject, later, terminal]
    result = dict(ordinaryPassed=True, passed=False, scenario="Coop", head=HEAD, sourceTree=SOURCE,
                  baseExpansionEvidenceEnabled=True, dirtyWorkingTree=False, baseExpansionRun=RUN)
    events = {role: [snap(s) for s in snapshots] for role in VIEWERS}
    events["host"] = [snap(start), commands[0], snap(built), commands[1], snap(expanded),
                      commands[2], snap(reject), snap(later), snap(terminal)]
    annotations = {}
    for role in VIEWERS:
        lines = []
        if role in ("host", "observer"):
            receipt = "PROOF_HOST_RESULT" if role == "host" else "PROOF_RESULT"
            lines.extend(f"{receipt} owner=2 sequence={i} accepted={int(i != 3)} reason={'placement' if i == 3 else 'accepted'}"
                         for i in (1, 2, 3))
            # These are real subsequent ordinary commands, not extra build observations.
            lines.append(f"{receipt} owner=2 sequence=4 accepted=1 reason=accepted")
            lines.append(f"{receipt} owner=2 sequence=5 accepted=1 reason=accepted")
        if role == "observer":
            lines.append("PROOF_BASE_EXPANSION_READY owner=2 epoch=1 revision=9")
            lines.extend(f"PROOF_BASE_EXPANSION_SEND owner=2 kind={kind} epoch=1 sequence={i} x={x} y=0 z=0"
                         for i, kind, x in ((1, "refinery", 350), (2, "factory", 1050), (3, "rejected-refinery", 2300)))
            lines.extend(["PROOF_BASE_EXPANSION_REJECTION_RECEIPT owner=2 epoch=1 sequence=3 accepted=0 reason=placement",
                          "PROOF_BASE_EXPANSION_COMPLETE role=observer owner=2 status=pass rejectionVerified=1"])
        if role == "host":
            lines.append("PROOF_SKIRMISH_COMPLETE role=host status=pass scenario=Coop winnerTeam=0 humanBuilt=1 humanGathered=1 humanProduced=1 botBuilt=1 botProduced=1 botAttack=1")
        elif role == "rendered":
            lines.append("PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario=Coop winnerTeam=0 built=1 gathered=1 produced=1 botBuilt=1 botProduced=1 botAttacked=1 reason=complete physicalInput=unverified")
        else:
            lines.append("PROOF_SKIRMISH_COMPLETE role=observer owner=2 status=pass scenario=Coop winnerTeam=0 reason=complete refinery=1 factory=1 gathered=1 produced=1 step=4")
        annotations[role] = lines
        events[role].append({"event": "finish"})
    f = {"result": result, "events": events, "annotations": annotations, "tails": {role: [] for role in VIEWERS}}
    for role in VIEWERS:
        recount(f, role)
    return f


def commands(f):
    return [e for e in f["events"]["host"] if e["event"] == "command"]


def states(f, change, predicate=lambda s: True, roles=VIEWERS):
    for role in roles:
        for event in f["events"][role]:
            observed = [event["state"]] if event["event"] == "snapshot" else [event["before"], event["after"]] if event["event"] == "command" else []
            for state in observed:
                if predicate(state):
                    change(state)


def change_new_unit(f, unit_id, field, value):
    def change(s):
        for u in s["units"]:
            if u["id"] == unit_id:
                u[field] = copy.deepcopy(value)
    states(f, change)


def change_rejection(f, change):
    states(f, change, lambda s: s["revision"] == 17)


def change_destination(f, command_index, value):
    cmd = commands(f)[command_index]
    cmd["destination"] = copy.deepcopy(value)
    kind = ("refinery", "factory", "rejected-refinery")[command_index]
    for i, text in enumerate(f["annotations"]["observer"]):
        if text.startswith("PROOF_BASE_EXPANSION_SEND") and f"kind={kind} " in text:
            f["annotations"]["observer"][i] = (
                f"PROOF_BASE_EXPANSION_SEND owner=2 kind={kind} epoch=1 sequence={command_index + 1} "
                + " ".join(f"{axis}={value[axis]}" for axis in ("x", "y", "z")))


def add_pairwise_checkpoint(f, conflict=False):
    for role in ("host", "rendered"):
        record = copy.deepcopy(next(e["state"] for e in f["events"][role] if e.get("state", {}).get("revision") == 18))
        record.update(revision=19, digest=f"{19:032x}")
        if conflict and role == "rendered":
            record["players"][0]["credits"] += 1
        f["events"][role].insert(-2, snap(record))
        recount(f, role)


def verify(f, transform=None):
    with tempfile.TemporaryDirectory(prefix="base-expansion-parser-") as temp:
        root = Path(temp)
        (root / "result.json").write_text(json.dumps(f["result"]), encoding="utf-8")
        for role, events in f["events"].items():
            lines = []
            deferred_ordinary = None
            for event in events:
                observation = PARSER.MARKER + json.dumps(event, separators=(",", ":"))
                if event["event"] == "snapshot":
                    s = event["state"]
                    ordinary = (f"PROOF_SNAPSHOT role={'host' if role == 'host' else 'client'} owner={VIEWERS[role]} "
                                f"epoch={s['epoch']} revision={s['revision']} digest={s['digest']}")
                    # Actual host Publish logs ordinary first; controller Tick logs observation first.
                    if role != "host" and s["phase"] == 2:
                        lines.append(observation)
                        deferred_ordinary = ordinary
                    else:
                        lines.extend([ordinary, observation] if role == "host" else [observation, ordinary])
                else:
                    lines.append(observation)
                    if event["event"] == "finish" and deferred_ordinary:
                        lines.append(deferred_ordinary)
                        deferred_ordinary = None
            lines.extend(f["annotations"][role])
            lines.extend(f["tails"][role])
            text = "\n".join(PREFIX + line for line in lines) + "\n"
            if transform:
                text = transform(role, text)
            (root / (role + ".log")).write_text(text, encoding="utf-8")
        return PARSER.verify(root, root / "result.json", HEAD, SOURCE)


class ParserTests(unittest.TestCase):
    controls = 0

    def rejects(self, name, mutate, transform=None):
        with self.subTest(control=name):
            f = fixture()
            mutate(f)
            type(self).controls += 1
            with self.assertRaises((ValueError, json.JSONDecodeError)):
                verify(f, transform)

    def test_valid_unreal_prefix_completion_and_pending_result(self):
        proof = verify(fixture())
        self.assertTrue(proof["passed"])
        self.assertEqual(proof["epoch"], 1)
        self.assertEqual(proof["commonCheckpoints"], 6)
        self.assertEqual(proof["roleCounts"]["host"], {"snapshots": 6, "commands": 3})
        self.assertIn("first-terminal", proof["proofScope"])

    def test_valid_final_result_and_pairwise_only_checkpoint(self):
        f = fixture()
        f["result"]["passed"] = True
        add_pairwise_checkpoint(f)
        self.assertTrue(verify(f)["passed"])

    def test_valid_coalesced_revisions_delayed_publish_and_hidden_gather(self):
        f = fixture()
        # Submit results 13/16 need not be published immediately, or at all.
        for role in VIEWERS:
            f["events"][role] = [e for e in f["events"][role]
                                 if e.get("state", {}).get("revision") not in (13, 16)]
        # Keep five common checkpoints without inventing intermediate Submit publications.
        for role in VIEWERS:
            s = copy.deepcopy(next(e["state"] for e in f["events"][role] if e.get("state", {}).get("revision") == 18))
            s.update(revision=19, digest=f"{19:032x}")
            f["events"][role].insert(-2, snap(s))
            recount(f, role)
        proof = verify(f)
        self.assertEqual(proof["commonCheckpoints"], 5)
        self.assertEqual(proof["firstCommonFactoryRevision"], 17)

    def test_valid_first_terminal_boundaries_and_ordinary_teardown(self):
        f = fixture()
        # Observer exits at 20; a later client may first see a terminal teardown revision.
        terminal = f["events"]["rendered"][-2]["state"]
        terminal.update(revision=21, digest=f"{21:032x}")
        terminal["players"] = [p for p in terminal["players"] if p["owner"] != 2]
        terminal["units"] = [u for u in terminal["units"] if u["owner"] != 2]
        terminal["resources"] = []
        recount(f, "rendered")
        f["tails"]["host"].append(f"PROOF_SNAPSHOT role=host owner=0 epoch=1 revision=22 digest={22:032x}")
        proof = verify(f)
        self.assertEqual(proof["terminals"]["rendered"]["revision"], 21)
        self.assertEqual(proof["commonCheckpoints"], 5)

    def test_supplied_false_pass_controls(self):
        self.rejects("ally credits mutation", lambda f: commands(f)[0]["after"]["players"][0].update(credits=0))
        def foreign_epoch(f):
            states(f, lambda s: s.update(epoch=2), roles=("rendered", "observer"))
            for role in ("rendered", "observer"):
                f["events"][role][-1]["epoch"] = 2
        self.rejects("common snapshots use foreign epoch", foreign_epoch)
        self.rejects("same revision conflicts with command poststate", lambda f: next(e["state"] for e in f["events"]["observer"] if e.get("state", {}).get("revision") == 16)["players"][1].update(credits=999))
        self.rejects("failure after pass", lambda f: f["tails"]["observer"].append("PROOF_BASE_EXPANSION_COMPLETE role=observer owner=2 status=fail rejectionVerified=0"))
        self.rejects("truncated observation tail", lambda f: f["tails"]["host"].append("PROOF_BASE_EXPANSION_OBSERVATION"))
        self.rejects("grant overlaps refinery", lambda f: change_new_unit(f, 7, "position", point(350)))
        self.rejects("grant outside actual world", lambda f: change_new_unit(f, 7, "position", point(50000)))
        self.rejects("rejection without live HQ", lambda f: change_rejection(f, lambda s: s["units"][0].update(hp=0)))

    def test_complete_accepted_state_and_default_records(self):
        for name, change in (
            ("ally credits", lambda s: s["players"][0].update(credits=349)),
            ("owner gathered", lambda s: s["players"][1].update(gathered=1)),
            ("owner team", lambda s: s["players"][1].update(team=1)),
            ("owner cost", lambda s: s["players"][1].update(credits=s["players"][1]["credits"] + 1)),
            ("resource amount", lambda s: s["resources"][0].update(remaining=499)),
            ("resource owner", lambda s: s["resources"][0].update(owner=1)),
            ("old unit direction", lambda s: s["units"][2].update(direction=point(1))),
            ("old unit position", lambda s: s["units"][2].update(position=point(-301))),
            ("old unit hp", lambda s: s["units"][2].update(hp=99)),
            ("match winner", lambda s: s.update(winner=0)),
        ):
            self.rejects(name, lambda f, change=change: states(f, change, lambda s: s["revision"] >= 13))
        for unit_id in (6, 7, 8):
            for field, value in (("direction", point(1)), ("cargo", 1), ("rally", True),
                                 ("rallyPoint", point(2)), ("guard", True), ("post", point(3)),
                                 ("target", 1), ("returning", True), ("hp", 99), ("owner", 1), ("id", 25)):
                self.rejects(f"unit {unit_id} nondefault {field}",
                             lambda f, unit_id=unit_id, field=field, value=value: change_new_unit(f, unit_id, field, value))
        self.rejects("new unit omitted", lambda f: states(f, lambda s: s["units"].__setitem__(slice(None), [u for u in s["units"] if u["id"] != 7]), lambda s: s["revision"] >= 13))
        self.rejects("new unrelated record", lambda f: states(f, lambda s: s["units"].append(unit(9, 1, 0, 5000)), lambda s: s["revision"] >= 13))

    def test_spawn_order_boundaries_clearance_and_ring_search(self):
        self.rejects("legal but wrong first spawn", lambda f: change_new_unit(f, 7, "position", point(350, 300)))
        self.rejects("nonzero grant z", lambda f: change_new_unit(f, 7, "position", point(650, 0, 1)))
        s = snapshot(10)
        self.assertEqual(PARSER.expected_spawn(s, point(350)), point(650))
        # Test fixed offset order independently, with live and dead blockers.
        s["units"] = [unit(1, 1, 0, 300)]
        self.assertEqual(PARSER.expected_spawn(s, point()), point(-300))
        s["units"][0]["hp"] = 0
        self.assertEqual(PARSER.expected_spawn(s, point()), point(300))
        s["units"] = [unit(1, 1, 0, 560)]
        self.assertEqual(PARSER.expected_spawn(s, point()), point(300))  # Exactly 260 is clear.
        s["units"][0]["position"]["x"] = 559.999
        self.assertEqual(PARSER.expected_spawn(s, point()), point(-300))
        s["units"] = [unit(i, 1, 0, dx * 300, dy * 300) for i, (dx, dy) in enumerate(
            ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)), 1)]
        self.assertEqual(PARSER.expected_spawn(s, point()), point(600))
        s["units"] += [unit(i + 8, 1, 0, dx * 600, dy * 600) for i, (dx, dy) in enumerate(
            ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)), 1)]
        self.assertEqual(PARSER.expected_spawn(s, point()), point(900))
        s["units"] += [unit(i + 16, 1, 0, dx * 900, dy * 900) for i, (dx, dy) in enumerate(
            ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)), 1)]
        with self.assertRaisesRegex(ValueError, "no legal spawn"):
            PARSER.expected_spawn(s, point())
        s["units"] = []
        self.assertEqual(PARSER.expected_spawn(s, point(10000)), point(9700))
        self.assertEqual(PARSER.point(point(10000, -10000, 10000)), point(10000, -10000, 10000))
        with self.assertRaises(ValueError):
            PARSER.point(point(10000.001))

    def test_isolated_foreign_anchor_rejection(self):
        for name, change in (
            ("insufficient credits", lambda s: s["players"][1].update(credits=99)),
            ("inactive match", lambda s: s.update(phase=0)),
            ("winner already set", lambda s: s.update(winner=0)),
            ("missing owner record", lambda s: s["players"].pop(1)),
            ("capacity exhausted", lambda s: s["units"].extend(unit(i, 2, 0, -8000, i * 20) for i in range(9, 16))),
            ("ID capacity exhausted", lambda s: s["units"].append(unit(PARSER.MAX_INT32 - 2, 1, 0, -8000))),
            ("foreign anchor dead", lambda s: s["units"][1].update(hp=0)),
            ("foreign anchor absent", lambda s: s["units"][1].update(kind=0)),
        ):
            self.rejects(name, lambda f, change=change: change_rejection(f, change))
        for name, position in (("inside own factory reach", point(1800)), ("foreign overlap", point(2999)),
                               ("no foreign reach", point(5000)), ("outside world", point(10001))):
            self.rejects(name, lambda f, position=position: change_destination(f, 2, position))
        self.rejects("rejection changes state", lambda f: commands(f)[2]["after"]["players"][1].update(gathered=1))

    def test_epoch_chronology_and_pairwise_equality(self):
        self.rejects("pairwise-only checkpoint conflict", lambda f: add_pairwise_checkpoint(f, conflict=True))
        self.rejects("command prestate conflicts with prior poststate", lambda f: commands(f)[1]["before"].update(revision=13))
        self.rejects("payload epoch mismatch", lambda f: commands(f)[1].update(payloadEpoch=2))
        self.rejects("zero epoch", lambda f: commands(f)[0]["before"].update(epoch=0))
        def reordered(f, role):
            indexes = [i for i, e in enumerate(f["events"][role]) if e["event"] == "snapshot"]
            i, j = indexes[1:3]
            f["events"][role][i], f["events"][role][j] = f["events"][role][j], f["events"][role][i]
            recount(f, role)
        for role in VIEWERS:
            self.rejects(f"nonmonotone {role}", lambda f, role=role: reordered(f, role))
        def stale_host_publish(f):
            events = f["events"]["host"]
            published = events.pop(2)  # revision 13 is moved after the revision-16 command.
            events.insert(3, published)
            recount(f, "host")
        self.rejects("host Publish older than latest command", stale_host_publish)
        self.rejects("later command older than published state", lambda f: commands(f)[2]["before"].update(revision=15))

    def test_strict_scalars_identity_and_json(self):
        for field, value in (("v", True), ("v", 1.0), ("viewer", False), ("viewer", 0.0),
                             ("row", True), ("row", 1.0), ("run", "2" * 32), ("source", "b" * 40),
                             ("role", "client"), ("extra", 0)):
            self.rejects(f"row metadata {field}={value}", lambda f, field=field, value=value: f["events"]["host"][0].update({field: value}))
        for field, value in (("owner", 2.0), ("type", 3.0), ("sequence", True), ("payloadEpoch", True),
                             ("unit", False), ("target", 0), ("accepted", 1), ("reason", "other")):
            self.rejects(f"command {field}", lambda f, field=field, value=value: commands(f)[0].update({field: value}))
        for field, value in (("ordinaryPassed", False), ("ordinaryPassed", 1), ("passed", 1),
                             ("head", PARSER.SOURCE_HEAD), ("sourceTree", "c" * 40), ("dirtyWorkingTree", True),
                             ("baseExpansionEvidenceEnabled", 1), ("baseExpansionRun", "bad"), ("scenario", "Solo")):
            self.rejects(f"result {field}", lambda f, field=field, value=value: f["result"].update({field: value}))
        for field, value in (("epoch", True), ("revision", 10.0), ("phase", True), ("winner", False), ("skirmish", 1)):
            self.rejects(f"state scalar {field}", lambda f, field=field, value=value: commands(f)[0]["before"].update({field: value}))
        self.rejects("unit kind bool", lambda f: commands(f)[0]["before"]["units"][0].update(kind=True))
        self.rejects("coordinate bool", lambda f: commands(f)[0]["destination"].update(x=True))
        self.rejects("NaN coordinate", lambda f: commands(f)[0]["destination"].update(x=float("nan")))
        self.rejects("duplicate JSON key", lambda f: None,
                     lambda role, text: text.replace('"v":1', '"v":1,"v":1', 1) if role == "host" else text)
        self.rejects("duplicate player", lambda f: commands(f)[0]["before"]["players"].append(copy.deepcopy(commands(f)[0]["before"]["players"][0])))
        self.rejects("duplicate unit", lambda f: commands(f)[0]["before"]["units"].append(copy.deepcopy(commands(f)[0]["before"]["units"][0])))

    def test_terminal_counts_tail_and_all_completion_markers(self):
        for role in VIEWERS:
            self.rejects(f"{role} missing finish", lambda f, role=role: f["events"][role].pop())
            self.rejects(f"{role} duplicate finish", lambda f, role=role: f["events"][role].append(copy.deepcopy(f["events"][role][-1])))
            self.rejects(f"{role} truncated stream", lambda f, role=role: f["events"][role].__delitem__(slice(-2, None)))
            self.rejects(f"{role} snapshot after finish", lambda f, role=role: f["events"][role].append(copy.deepcopy(f["events"][role][-2])))
            for field, value in (("epoch", 2), ("revision", 21), ("digest", "f" * 32),
                                 ("snapshots", 0), ("commands", 4), ("row", 999),
                                 ("snapshots", True), ("commands", False)):
                self.rejects(f"{role} finish {field}", lambda f, role=role, field=field, value=value: f["events"][role][-1].update({field: value}))
            self.rejects(f"{role} ordinary fail after pass", lambda f, role=role: f["tails"][role].append("PROOF_SKIRMISH_COMPLETE role=host status=fail scenario=Coop winnerTeam=0"))
            self.rejects(f"{role} duplicate ordinary completion", lambda f, role=role: f["tails"][role].append(f["annotations"][role][-1]))
        self.rejects("finish with Playing snapshot", lambda f: states(f, lambda s: s.update(phase=1), lambda s: s["revision"] == 20))
        self.rejects("finish with losing snapshot", lambda f: states(f, lambda s: s.update(winner=1), lambda s: s["revision"] == 20))
        self.rejects("duplicate expansion pass", lambda f: f["tails"]["observer"].append("PROOF_BASE_EXPANSION_COMPLETE role=observer owner=2 status=pass rejectionVerified=1"))
        for marker in ("PROOF_BASE_EXPANSION", "PROOF_BASE_EXPANSION_OBSERVATIO", "PROOF_BASE_EXPANSION_COMPLETE",
                       "PROOF_BASE_EXPANSION_REJECTION_RECEIPT", "PROOF_BASE_EXPANSION_OBSERVATION_ERROR reason=test",
                       "PROOF_BASE_EXPANSION_UNKNOWN x=1", "PROOF_SKIRMISH_COMPLETE"):
            self.rejects(f"truncated/error marker {marker}", lambda f, marker=marker: f["tails"]["observer"].append(marker))
        self.rejects("receipt fail after accepted", lambda f: f["tails"]["host"].append("PROOF_HOST_RESULT owner=2 sequence=1 accepted=0 reason=placement"))
        self.rejects("duplicate send", lambda f: f["tails"]["observer"].append("PROOF_BASE_EXPANSION_SEND owner=2 kind=refinery epoch=1 sequence=1 x=350 y=0 z=0"))
        self.rejects("wrong rejection receipt epoch", lambda f: f["annotations"]["observer"].__setitem__(9, "PROOF_BASE_EXPANSION_REJECTION_RECEIPT owner=2 epoch=2 sequence=3 accepted=0 reason=placement"))
        self.rejects("ordinary digest conflict", lambda f: f["tails"]["host"].append("PROOF_SNAPSHOT role=host owner=0 epoch=1 revision=13 digest=" + "f" * 32))
        self.rejects("malformed extra receipt", lambda f: f["tails"]["host"].append("PROOF_HOST_RESULT owner=2 sequence=1.0 accepted=0 reason=placement"))
        def terminal_only_factory(f):
            for role in VIEWERS:
                f["events"][role] = ([snap(snapshot(i)) for i in (1, 2, 3)]
                    + [e for e in f["events"][role] if e.get("state", {}).get("revision") not in (16, 17, 18)])
                recount(f, role)
        self.rejects("factory replicated only at terminal", terminal_only_factory)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ParserTests)
    outcome = unittest.TextTestRunner(verbosity=2).run(suite)
    print(f"Synthetic rejection controls exercised: {ParserTests.controls}")
    raise SystemExit(0 if outcome.wasSuccessful() else 1)
