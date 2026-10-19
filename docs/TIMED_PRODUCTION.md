# Timed generic-unit production

## Scope and references

This first production state machine replaces instant **skirmish** generic Combat creation. Existing human `P` and bot producers use it through the ordinary authoritative command path. The non-skirmish proof remains instant. There is one active generic-unit job per owner, no backlog, and no parallel-factory bonus. Buildings remain instant.

The publisher-authored [RA2 English manual](https://static.wikia.nocookie.net/cnc_gamepedia_en/images/b/b5/Red_Alert_2_-_English.pdf/revision/latest?format=original), printed page 19, describes timed production, suspension when money runs out, automatic resumption, manual hold/resume, cancellation refunding spent money and category queues. This copy is third-party hosted; no proprietary document or game files are redistributed here. [Ares build-time documentation](https://ares-developers.github.io/Ares-docs/new/buildtime.html) describes cost-based timing in minutes per 1,000 credits and modifiers. It does not establish the original installation's values or a verified conversion to this prototype's seconds.

**Prototype calibration:** Combat still costs 50 credits. A fully funded job takes **5 simulated seconds**, using 100,000 microseconds of work per credit. This is a deliberate new calibration, not measured RA2 timing, a game-speed conversion, or a wall-clock/performance claim. Existing construction prices, movement, damage, bot grace and command budgets remain unchanged. No new units, faction rules, power system or assets are introduced.

## Authoritative job and commands

`FProofSnapshot.ProductionJobs` contains owner ID, accepted start sequence, factory ID, work microseconds, actual paid credits and a manual-held flag. Every field is replicated and committed to the diagnostic state digest. Jobs are owner-ID ordered. Clients display/consume server state rather than advancing their own production clocks.

- `Produce` at an owned live factory starts a job if an owner job does not already exist and capacity/ID admission allows it. It does not require the whole price or a currently clear exit. Start creates no Combat, allocates no unit ID and deducts no money. Another start rejects `production-busy`.
- Existing selected-factory routing and first eligible factory fallback in `P` remain unchanged. A job stays associated with that factory.
- `HoldProduction`, `ResumeProduction` and `CancelProduction` append enum values 11–13, preserving existing values. `UnitId` identifies the factory and `TargetUnitId` identifies the job's accepted start sequence; destination must be zero. Owner, sequence, rate and authoritative adapter epoch checks still apply. The direct command model does not itself validate match epochs.
- Hold/resume are idempotent accepted controls with ordinary revision changes. Hold prevents both work and release of a fully completed blocked job. Resume does not catch up elapsed held time.
- Cancel refunds exactly the actual paid credits once and removes the job. If an exact refund would overflow int32 credits, reject `refund-capacity` atomically. No saturated refund silently loses money.

The new public controls have model/native automation coverage prepared, but this slice adds no human bindings or HUD progress UI. Physical hold/resume/cancel and physical `P` acceptance are separate follow-up work. The unchanged Pawn request-status label still recognizes only its existing human-bound commands; new control input/status integration must update that adapter separately. A dedicated control diagnostic must use the actual controller RPC with an explicit control-identity observer, not claim that the current Pawn labels already support these new commands. Timed production itself is exercised by the existing real human-command adapter and bot producer; it is not unused model scaffolding.

## Advance ordering, payment and release

Positive advances run movement/combat and terminal evaluation first, production second, then harvesting/deposit. A terminal match freezes all jobs. Factory death on the same step therefore precedes production; a simultaneous terminal result takes precedence over even that refund.

Each finite positive step is rounded to the nearest microsecond and clamped before integer conversion. Fractions smaller than one microsecond are not accumulated between calls. Equal summed **rounded** work durations produce equivalent work, but arbitrary floating-point partition equivalence is not promised. Zero, negative and nonfinite durations do no work.

Work is bounded by paid work plus affordable remaining credits. The debit ledger is the ceiling of completed work divided by 100,000 microseconds. A partly used credit therefore continues funding its remaining work even if available cash is zero. Insufficient funds pause work with no catch-up; a later deposit resumes it on the next advance. Credits never become negative.

When fully paid and complete, the job tries the existing deterministic spawn policy at its associated factory using the factory's **current** rally. Blocked exits, exhausted IDs or capacity retain the fully paid job without another charge. Later positive advances retry. A newly released unit first moves or fires on the following advance. Manual hold suppresses release too.

[Original factory load sharing](https://ares-developers.github.io/Ares-docs/bugfixes/type2/factoryloadsharing.html) can use alternate exits of the same building type. That is a documented gap: this initial single-job prototype uses only the requested factory and does not claim complete factory/queue fidelity.

## Loss and lifecycle

A missing/dead associated factory attempts an exact paid-credit refund before hold or work. If there is insufficient int32 headroom, the visible job remains an addressable refund liability. It cannot progress or spawn; later advances retry after ordinary spending creates room. Controls are resolved before generic live-unit ownership checks so this liability can still be cancelled. This overflow/loss policy is an explicit bounded prototype decision; the cited references do not establish original behavior for that corner.

Owner removal removes that owner's job with their economy state. Surviving owners are unaffected. Reset/rematch clears all jobs. Terminal state retains them frozen. Late observers receive the same full job state and digest. There is no automatic takeover, reconstruction or transfer to another factory.

The bot retains its existing full-price planning check, priority order, attack grace, retry cadence and command budget. It avoids starting another job while one is active, including held or exit-blocked work. It does not automate hold/cancel.

## Validation boundaries

Seven new exact native test names are declared under `RedalertProof.Skirmish.TimedProduction`: `StartAndFunding`, `ControlsAndIdentity`, `QuantizedTimeAndDigest`, `BlockedReleaseAndRally`, `LossAndRefundLiability`, `OwnersAndLifecycle`, and `BotPlanningAndReplay`. The native Skirmish manifest retains all 62 previous names and adds these seven; existing resource/deadline limits are unchanged.

The portable runner compiles the actual native test bodies and production model against the existing test-only Unreal adapter and real MD5 bridge. It is not UHT, MSVC, Unreal, rendered, network or physical-input acceptance. Existing Produce-dependent fixture setups now wait for actual paid work rather than introducing an instant test mode or fabricated Combat. Their original ownership, rejection, economy, combat and lifecycle assertions remain applicable after the documented setup changes. Complete bot replay serialization includes every job field and economic conservation includes completed production plus active paid debits, independently of credit deltas.

Runtime economy evidence reconciles payment/refund independently of ore deposits, using a read-only oracle over job ledgers and retained Combat records rather than deriving production from the observed credit difference. Factory evidence distinguishes accepted start from paid completion, preserves ordered per-job Advance indices/counts, and binds the original job identity. All represented job fields and available exact published contexts are checked; the compact context does not claim to serialize every older unit display field. The authoritative digest still commits the complete snapshot contract.

ReturnCargo retains its existing 512-row bound: its live C++ observer validates every actual Advance, while the sparse offline parser checks absolute economic conservation and job continuity at observed boundaries plus exact deposit transitions. It does not claim offline replay of every paid tick. Detailed per-tick evidence belongs to FactoryProduction/FactoryRally and the separate control diagnostic. Existing overall runtime, log and row budgets remain unchanged; complete-context JSON rows have explicit bounded schema capacity.

Cloud execution passed the seven actual new automation bodies (4,611 assertions per normal/O2/ASan+UBSan mode), all 89 existing Produce-dependent/associated automation groups (10,938 per mode), the independent runtime-accounting header/public-model harness (1,573 per mode), and the unchanged complete Pawn Produce method through its test adapter (552 checks in normal/O2). All nine deliberately faulty production implementations were rejected by their intended assertions. Existing immediate-production fixture failures and the review-discovered zero-cash mutant survivor are retained in the private validation evidence; the final strengthened controls reject that mutant. These results are portable checks, not engine execution. LeakSanitizer was disabled because this cloud environment does not support it under ptrace; address/undefined-behavior checks passed, with no leak-check claim.

The six modified PowerShell evidence policies and their synthetic controls require actual local PowerShell execution before any UE build. Cloud source/lexical review is not a PowerShell pass. Required scripts are `test-refinery-miner-evidence-policy.ps1`, `test-group-gather-evidence.ps1`, `test-factory-production-evidence-policy.ps1`, `test-factory-rally-evidence-policy.ps1`, `test-attack-move-evidence-policy.ps1`, and `test-return-cargo-evidence-policy.ps1` under `unreal/Scripts`. Native and runtime execution results remain pending until separately recorded against the frozen exact source.

Required acceptance before merge:

1. Cloud focused, regression, deterministic replay, source-bound input-adapter and deliberately faulty-control checks; independent source/test review.
2. Exact clean production UHT/MSVC build; all 69 Skirmish names and affected FactoryRally/Idle/Stop/AttackMove/command suites; unchanged bounded execution policies.
3. Ordinary Victory, Defeat and three-role cooperative matches with exact common-revision state/digest agreement.
4. A separate reviewed diagnostic observing actual RPC starts, replicated progress/payment, a cash pause and resumption, completed unit/rally, hold/cancel/refund, and host/client common job states. Diagnostic source remains separate from exact production acceptance.

Queue backlog, alternative-factory exits, progress UI/control bindings, original timing/power modifiers, real input and broader scale/performance remain explicit gaps.

### Observed portable match changes

The unchanged six-setup complete-match matrix requires a valid HQ-derived terminal result and exact deterministic replay, not a prescribed winning team. The accepted immediate-production baseline and this candidate produce different outcomes; those differences are retained rather than normalized away:

| Setup | Accepted immediate baseline (team, simulated seconds) | Timed candidate (team, simulated seconds) |
| --- | --- | --- |
| Default seeds, ordinary submission | 1, 85.2 | 0, 80.1 |
| Default seeds, reversed submission | 1, 85.2 | 0, 80.1 |
| Horizontal symmetric | 1, 67.3 | 0, 78.5 |
| Vertical symmetric | 0, 72.0 | 0, 73.4 |
| Short horizontal | 1, 48.9 | 1, 45.8 |
| Long diagonal | 1, 89.1 | 0, 110.2 |

Baseline figures come from the retained PR70 accepted portable log, with its model/rules/bot/test input files rechecked against this slice's exact baseline. Candidate figures were freshly reproduced in normal, optimized and ASan+UBSan portable builds, with 880,446 replay/conservation assertions per mode. All six terminate inside the existing 220-simulated-second observation bound with zero rejected bot commands, exact command/state replay and independent ore/credit conservation. These are a small deterministic scenario matrix, not original-game balance measurements, network results or wall-clock performance. Scripted native Victory/Defeat/Coop gates retain their own required results and remain pending.
