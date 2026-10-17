# Offline startup log summary

`unreal/Scripts/summarize-startup-logs.ps1` reads one retained lifecycle run's
`result.json` and role logs. It launches no processes and writes no files. Use
PowerShell 7; JSON is emitted to standard output:

```powershell
.\unreal\Scripts\summarize-startup-logs.ps1 -RunDirectory D:\path\to\retained-run -MaxGaps 3
.\unreal\Scripts\test-startup-log-summary.ps1
```

Each role reports process start, first dated log, engine initialization completion,
and the corresponding ready/join marker in `host.log`. The rejected `third` role
reports a full-session rejection instead of a join. Initial lead/owner2 and
replacement rejoin markers use their existing owner/epoch/seeded distinctions.
Unknown roles have no inferred endpoint. Missing files, timestamps and markers
remain explicit; unavailable or negative durations are `null`.

Unreal's dated log prefix is parsed as UTC. Process timestamp strings retain their
explicit UTC/offset semantics. Unqualified local process timestamps are unavailable
unless `-ProcessTimeZoneId` names their timezone; ambiguous or invalid local times
are rejected. The parser never substitutes the machine's current timezone.

SHA-256 hashes describe the exact result/log bytes parsed. Marker/gap entries
include source paths and one-based line numbers. Input is bounded to 16 process
records, 16 MiB per file, and 1–10 ranked gaps per role. Log paths derive only from
validated role filenames in the supplied directory, never result-file log paths.
The script does not modify retained evidence. The synthetic test writes and cleans
up its own uniquely named temporary fixture only.

Gap analysis ends at the first engine initialization completion line. Missing that
boundary disables gap analysis. Equal-sized gaps retain source-line order. Quiet
periods waiting for later clients or play gates are excluded. Readiness can precede
the engine's final initialization message, so their signed difference is not
reported as elapsed work. UTC wall-clock regression counts are reported separately.

This is diagnostic evidence, not a lifecycle acceptance result, performance
benchmark or causal attribution. A long interval between two messages does not
prove either named subsystem consumed that interval. In issue #33, the retained
failed run's long engine initialization and short map-load messages justify
narrowing the next investigation to pre-map initialization; they do not establish
a cache, scheduler, security, I/O, or gameplay cause. No timeout or launch-policy
change is proposed. See [the retained timing review](LIFECYCLE_TIMING_REVIEW.md).

## Offline validation

The synthetic policy script passed 21 assertions. Both retained integration-checkout
runs parsed all five roles with no missing fields: failed
`session-lifecycle-20261017-004312-990` and passing
`session-lifecycle-20261017-002102-766`. No engine was launched.

| Host interval, seconds | Failed | Passing |
|---|---:|---:|
| Process start to first dated log | 14.696 | 1.042 |
| First dated log to initialization completion | 33.404 | 5.594 |
| Process start to readiness | 48.023 | 6.553 |

The failed host's largest pre-init log gap is 9.871 seconds, lines 729–730.
The result-file SHA-256 hashes are respectively
`18a10d72f604c256601df522407725f4494fe69eeb518444d3b2a02a14585ea1`
and `339e4871ce5ad50bfac7db92371066b394362521fc06e1bab524c28e19117d73`.
These observations narrow the retained evidence; the startup cause remains open.
