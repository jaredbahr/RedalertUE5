# Admission CPU attribution candidate

This is a **diagnostic-only, source-only candidate**. It has not been compiled or
executed. It creates no new acceptance gate and grants no work, game or merge
acceptance. The original first 100% readings remain valid failed admissions.

Three retained admissions of game source `02e08abd` first reported CPU-2 at 100%,
then lower values. The existing source order has also passed. Current receipts
cannot identify the counter interval or the workload responsible. This candidate
collects bounded evidence to investigate that gap; it does not implement a repair.

## Fixed scope

The only observed operation is the byte-identical original admission sampler,
SHA256 `f891ebaf39f09a738578ce0da9d94fa47352b27e84d8ce0fb56d447e49c5c353`.
The existing full game-binding checker runs first inside the same owned job. No
sampler shim, warm-up, row exclusion, threshold change or uninstrumented pass is
inferred. All three CPU rows, conflict/game checks and memory/disk floors remain.

The clone accepts only native `admission`, 20 seconds, and rejects whole-controller
mode. The fixed caller cannot invoke Attack Move, ReturnCargo, final-route controls,
600-second qualification, an engine, build or another experiment. It differs from:

- The consumed PR71 source-225 timing profile: instrumented Attack Move policy
  cases/reader controls under 120 seconds, not this original resource admission.
- The consumed PR72 source-355 final-route identity observation: the dispatcher/
  Python invalid-approval rejection and console teardown under 120 seconds, not
  this Admission-only resource operation on game source 02e.

Neither consumed profile may be rerun. The comparison is an entrypoint/source-scope
argument, not permission to execute this new candidate. Exact source review and a
new coordinated runtime window are still required.

## Evidence and limits

`AdmissionCpuSupervisor.cs` derives from unchanged C92 source
`c92ef422b3519b122a405bb5edbc95184c53ce42da768115cf22d64ccdd466ee`.
It preserves suspended launch, job assignment, exact inherited handles, mask
`0x44` (decimal 68), Normal priority, original deadline/cancellation, output caps,
survivor rejection and one aggregate five-second cleanup. Termination precedes
optional diagnostics. The original `passed` predicate remains supervision only.

Read-only job counters include cumulative CPU of associated active and exited
processes. Root CPU comes from the retained creation-bound process handle; observer
CPU comes from the parent's self handle. There is no new process enumeration,
per-child attribution, command-line collection, thread, monitor or global trace.
At most 81 non-atomic frames are read, with at least 250 ms after each prior read.
Terminal job/root counters reuse mandatory cleanup reads. Four owned phase files
bracket preflight and the sampler. Marker/read/cap failures make evidence incomplete.
Optional telemetry serialization is capped at 192 KiB; native metadata stays capped
at 256 KiB. Only optional frames may be discarded on telemetry failure, with an
explicit count and incomplete flag. Sampler rows are never discarded.

Job accounting semantics are documented in Microsoft's
[job accounting reference](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information).

This cannot recover provider interval starts, assign CPU to logical core 2 or 6,
identify an unowned workload, prove causation, or subtract observer overhead.
Markers, API queries and serialization alter scheduling. Unmarked payload startup
is covered by job frames but not by a dedicated phase marker. Counter brackets
are read intervals, not atomic snapshots. Final observer CPU does not include all
receipt serialization. Synchronous OS/API stalls remain non-preemptible; late
returns fail existing clocks. No universal wall-clock or host-ancestry guarantee.

## Preparation and execution contract

`InvokeProbe.ps1` defaults to a non-executing plan. Source bindings live outside the
checkout at the fixed probe root. No review, DLL or runtime receipt is fabricated.

`Prepare` requires genuine exact source approval, a newly coordinated diagnostic
preparation window, confirmed drained lanes and a real fresh read-only host/process
inspection. Its explicit `DiagnosticPreparationAuthorized` and
`CurrentHostInspectionConfirmed` switches attest those current facts; this source
package does not assert them. The source review must explicitly cover diagnostic
preparation and fresh inspection. It uses the already reviewed actual C92 component
as the 20-second owner, with shared controller-entry origin and the same cleanup
budget. `CompileObserver.ps1` compiles only this reviewed source into a fresh
separate diagnostic DLL, verifies loaded location/MVID and records its real hash.
Native Git work stays inside the job; only its own console is released after work.
This preparation has **not run** and grants no capacity, game or work acceptance.

The initial source-only draft required a fresh successful plain admission before
the observer could be compiled. That circular NEW diagnostic prerequisite conflicted
with the current no-repeat hold and was removed from this candidate after review.
No original sampler or existing game-admission gate was changed. Diagnostic
preparation creates no original admission result or build eligibility; the original
sampler and all its criteria remain authoritative when the observation runs.

`Observe` additionally requires a genuine review of the actual compiled component,
preparation outcome and normal external caller completion. Unknown future bytes
are not source-approved activation artifacts. Observation is exactly one 20-second
Admission operation, plus the original aggregate five-second cleanup. There is no
retry loop or stage after owner release. Each future stage requires current lane
clearance. No runtime is reserved while source review is pending.

The controller validates native identity/output/drain, frame and marker identity,
counter/QPC bounds and all original resource data. An observation may be complete
while `resourceEligible=false`; that is recorded evidence, never admission success.
Returned data/gate files are nonterminal. A real outer caller must observe normal
native exit, reconcile terminal/raw output and remeasure final original-origin
20-second and post-exit five-second timing before any result is accepted. A raw
success file alone is insufficient. Late receipt writes remain failed on return.

## Current validation status

Source authored and statically inspected only. C# compilation, Windows API/layout
checks, cap/error/cancel/survivor controls, component birth/provenance and the actual
single observation are **unrun**. Independent source review is required before any
bounded preparation; compiled-component review is required before observation.
The game correction remains unbuilt, all prior failures/35 frozen PR71 pins remain
intact, and PR72 public-status disclosure remains on hold. No push or merge.
