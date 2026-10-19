# PR70 bot base expansion: isolated runtime diagnostic source review

Status: **source prepared; no diagnostic build or execution performed.** This
detached D: worktree starts at PR70 head
`a9c07db7f897d6b4c12866eca753a533e1cf1912`. These changes are not in
the production checkout or PR. Review the complete diff before authorizing a
diagnostic build/run. The production evidence is in a separate Library archive.

## Question

Does a three-role Unreal run accept a freshly planned owner-1 refinery recovery
through the usual client command RPC, after an actual paid, earned-income
history has blocked all twelve HQ offsets, while rejecting the same location
for owner 2 and publishing identical before/after states to both clients?

## Opt-in flow

1. A dedicated host uses `-ProofSkirmish -ProofBotBaseExpansionFixtureHost`
   and waits for both human seats. Its normal skirmish bot and 0.1-second game
   loop are paused only in this fixture. The host's own setup issues public
   `SubmitForOwner` commands and `Advance` calls; it has no private model-order
   or snapshot mutation.
2. Owner 1 pays for a refinery 800 cm from HQ, earns 450 credits through real
   extraction/deposit, stops its miners, pays for four factories that block
   the HQ search ring, and loses both miners to moved/produced enemy combat.
   Setup checks the live six-record state, zero miners, 100 credits, 450
   gathered, all twelve blocked HQ candidates, and a clear fallback 350 cm
   from the committed refinery / 1150 cm from HQ. Host independently calls
   the fresh PR70 planner and requires exactly that one recovery intent.
3. Host publishes the pre-recovery snapshot. Owner 2 is cued to send a
   refinery at the fallback position by its ordinary `ServerSubmitCommand`
   RPC. The host requires `placement` rejection and an unchanged visible
   digest. Owner 2 receives the ordinary command receipt.
4. Owner 1 is cued with the established next sequence. It plans from its
   replicated pre-recovery snapshot, requires the same one-command fallback,
   adapts only the sequence/epoch, and sends the ordinary RPC. The host
   requires acceptance, exact 100-credit debit, +3 revision, one new refinery
   and one granted miner, then publishes the post-recovery snapshot. Both
   clients require their receipts and the resulting state.
5. `FProofBaseExpansionObservation` emits bounded full-state JSON for host and
   client publications, plus exact host command before/after rows. The parser
   checks row/run/source continuity, paid setup, both RPC results, exact new
   records, no preexisting-unit changes, and equality of before/after states
   across all three roles. A pass requires all three process completion logs.

The observer is copied from the previously reviewed PR69 isolated diagnostic
and is read-only. The host setup is **scripted** and deliberately does not
represent an autonomous bot stall or hot takeover. This covers the miner
recovery branch only; PR70's native PublicRecovery test separately covers
factory recovery, later production, reservation and negative planner cases.
It also does not establish internet behavior or a performance target.

## Files for review

- `ProofGameMode.cpp/.h`: opt-in setup, admission, RPC result checks, and host
  snapshot publication. Review that the fixture branch cannot affect an
  ordinary run without its explicit flag.
- `ProofPlayerController.cpp/.h`: two passive clients, planner intent on owner
  1, normal RPC and receipt, and replicated end-state checks. The cue RPC
  carries only the established sequence/revision, not a model state or order.
- `ProofBaseExpansionObservation.cpp/.h`: bounded read-only event stream.
- `unreal/Scripts/parse-bot-base-expansion.py`: fail-closed log parser.

## After source approval only

Use the same three-role launch/resource controls as the PR70 production
validation, but build this exact diagnostic commit into its own worktree/DLL
and run a separate port, D: cache/temp, and evidence directory. Reuse only the
14 hash-checked map/assets. Launch host and two passive clients with
`-ProofBaseExpansionObserve -ProofBaseExpansionRun=<fresh-32-hex>
-ProofBaseExpansionSource=<diagnostic-commit-SHA>` on each process. Give host
`-ProofBotBaseExpansionFixtureHost`, both clients
`-ProofBotBaseExpansionPassive`, and all three `-ProofSkirmish`.
Record exact source HEAD/tree, diagnostic DLL SHA before/after, command lines,
process exit codes, resource-admission/cleanup receipts, all raw logs, parser
result, and any failures. The fixture has explicit 20-second RPC stage limits
and a 90-second overall timeout; the external runner must also have a bound.
Run the parser with `--host`, `--owner1`, `--owner2`, `--run`, and `--source`.
Require no unexpected modified tracked files. Do not copy diagnostic files or
DLL into the production checkout or claim a PR70 production change from this
experiment.
