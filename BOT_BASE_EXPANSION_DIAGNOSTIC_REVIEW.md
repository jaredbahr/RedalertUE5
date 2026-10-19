# PR70 isolated fallback-recovery observation

This is a local-only diagnostic composition on production PR70 head
`a9c07db7f897d6b4c12866eca753a533e1cf1912`, full tree
`093db7cf62d2afde755adea49492fad954942d84`, Source tree
`9c8f66321b0590e5f4da9b97a5dc41269a67562f`. It is not a production PR change.
The delivery manifest binds the complete corrected source and both patches.
Native build and runtime validation of this composition remain pending.

## Bounded question and qualifications

Can a fresh planner, operating on the actual replicated owner-1 state, recover
one refinery through the normal client command RPC after a scripted paid
history blocks all twelve HQ search offsets? Does owner 2's identical location
reject without visible mutation, and do both clients observe the exact accepted
revision before teardown?

The setup is deliberately scripted. It does not show an autonomous default-map
stall, hot takeover, factory recovery at runtime, human input, internet behavior,
or a performance target. Production native tests separately cover the factory
branch and negative planner cases. Snapshots expose public state, not private
orders or the entire setup command history. The observer records every command
reaching SubmitForOwner after setup, including stale-epoch attempts; ingress and
inactive-controller rejection occur earlier in the ordinary authority path.
The diagnostic is not a hostile-network ingress test.

## Actual source path

1. Dedicated host waits for both human seats. Only the explicit host flag pauses
   its ordinary bot/0.1-second simulation loop. Setup uses public SubmitForOwner
   and Model.Advance calls, without state mutation or private-order access.
2. Owner 1 pays for its anchor refinery, earns 450 through real mining/deposit,
   stops the miners, pays for four HQ-ring factories, and loses both miners to
   produced/moved enemy combat. Setup asserts six live owned structures, two
   lost miners, 100 credits, 450 gathered, twelve blocked HQ sites and a clear
   fallback at (-250,-400,0), 1150 cm from HQ and 350 cm from the refinery.
3. Host checks a fresh production planner's single recovery intent and publishes
   the pre-recovery state. It cues owner 2 to send the foreign-anchor negative
   command, then owner 1 to plan from its replicated state and adapt only that
   intent's established owner sequence and match epoch. Both use the unchanged
   ServerSubmitCommand -> AdmitIngress -> SubmitForController -> SubmitForOwner
   -> Model.Submit path and ordinary reliable command receipt.
4. All post-setup model commands are observed. The expected owner-2 command
   rejects for placement, preserving visible state. Owner 1's accepted command
   spends exactly 100, increments revision by 3 and allocates a default refinery
   plus default granted miner. No Advance occurs during this measured pair.
5. Host seals its observation at the accepted publication. Each client waits for
   its receipt and the post-state, seals its own observation, and acknowledges
   the captured epoch/revision/digest through a diagnostic-only RPC. Host derives
   the acknowledgement owner from the current controller binding. It releases
   clients only after both unique matching acknowledgements. This prevents early
   client Logout from erasing state before the peer observes the shared revision.
   Host waits the existing six-second exit grace after release. Later Logout
   publications lie outside the explicitly sealed observation scope.

## Producer/parser contract

- Model snapshots remain epoch zero. Evidence copies receive the trusted host
  epoch before ComputeDigest; the actual model and digest function are unchanged.
  Host command-result metadata is observed after the ordinary epoch stamp.
- READY carries an explicit epoch. A reliable cue can arrive before replicated
  OwnerId; READY records the expected seat and Tick waits for/checks the actual
  owner before SEND. No action is taken from the expected seat alone.
- Every snapshot serializes every current public field: identity, match state,
  credits/gathered, positions/HP/kinds/cargo, rally, travel direction, AreaGuard
  metadata and resources. JSON comparisons retain decimal precision.
- The parser independently checks the existing ordered three-ring spawn search,
  all twelve blocked HQ candidates, retained dead miners, exact new-record
  defaults, full permitted state delta, all role identities and matching complete
  before/after snapshots. Digest strings are checked for format, consistency and
  identical-state agreement; the parser does not claim a separate native MD5
  recomputation or authentication.
- Explicit Finish records work while the match is Playing. Every row, counter,
  marker and within-process order is checked; failures, duplicate keys, missing
  fields, nonfinite/bool numeric identities, overflow, truncation, extra commands
  and observation events after Finish invalidate the evidence. Cross-process
  clock order is not inferred.
- Observation limits remain 8192 rows, 65536 characters per JSON row and 32 MiB
  cumulative JSON text. Fixture stages retain 20 seconds, overall 90 seconds,
  with the existing six-second exit grace. Existing external process/resource
  limits remain unchanged and independently required.

## Validation and execution boundary

The archive includes source-bound portable setup/digest controls and synthetic
parser/barrier controls, with exact commands/results/hashes. Portable controls
use the real model/planner/digest body plus narrowly disclosed transport and
publication stubs. They are not a UHT/MSVC build, actual RPC or replication run.
Original delivered defects and their independent failing controls are retained.

After independent source approval, the existing local owner may apply either
reviewed patch, verify the complete tree and Source tree, and build in the
isolated diagnostic worktree. Reuse only the established hash-checked assets and
unchanged resource admission, per-process, timeout and cleanup policies. Python
must be supplied as an explicit executable parameter; no machine-specific path
is part of this diagnostic.

Use a fresh port/evidence directory and fresh 32-hex run token. All roles use
`-ProofSkirmish -ProofBaseExpansionObserve -ProofBaseExpansionRun=<run>
-ProofBaseExpansionSource=<exact-diagnostic-commit>`. Dedicated host additionally
uses `-ProofBotBaseExpansionFixtureHost`; both clients use
`-ProofBotBaseExpansionPassive`. Do not mix other scenario/input/lifecycle flags.
Run the supplied parser with exact host/owner1/owner2 log paths, run token and
source commit. Require all three process exits, parser success, source/DLL/asset
bindings, unchanged tracked files, raw logs, retained failures and bounded
cleanup receipts. No local diagnostic files or DLL belong in production.
