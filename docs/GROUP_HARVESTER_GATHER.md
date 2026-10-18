# Group harvester Gather

An all-Harvester multiselection can right-click ore to queue the existing Gather command for each eligible miner, in ascending unit-ID order. Enemy targeting keeps precedence; mixed selections retain Attack/Move. Loaded miners may resume unloading from depleted ore; omitted empty miners keep unrelated pending orders. No idle auto-gather is added.

Selection and queue recheck the playing skirmish, participant, owned live Harvester, resource and own live refinery. Stop/Move supersede pending Gather. Dispatch remains two commands per 0.1-second interval, with sequence/epoch stamped only at dispatch. Authoritative Submit, economy, rate and balance rules are unchanged. Single and group miners share the existing strict-inside 32-pixel resource picker.

## Validation

Accepted engine checkpoint: `97f7cd56e7e6bb2281dd9388c930e52cf3b5a7d2`.

- Portable production eligibility: GCC 14.2, strict C++17 warnings; normal and `-O2 -DNDEBUG` cloud runs each passed 292 checks. Header/test blobs are unchanged from the cloud-tested checkpoint.
- Unreal build passed. All 54 native tests passed: Gather 5, controls 12, skirmish 26, Stop 11, using the existing 90-second limits.
- Group Victory runtime passed in 84.219 seconds; default single-miner Victory passed in 91.812 seconds. Both retain scenario <220 seconds and total <240 seconds. Five fresh 1280x720 screenshots were visually reviewed.
- The pure evidence parser passed 51 synthetic checks. These verify parsing and rejection policy separately from engine execution.

`test-small-match.ps1 -Scenario Victory -GroupGatherEvidence` selects the real starter through the pointer handler, adds the refinery-granted miner, and calls `OrderUnderCursor` once over projected ore. The strict parser verifies two unstamped queued intents, normal dispatch, consecutive accepted commands, shared request records, provenance, and both miners' movement/loading/deposits. Exact deposit observations come from the existing real Advance boundary and include cargo, credit, contributor and conservation checks. A third screenshot shows the restored two-miner selection. This is scripted handler/runtime evidence; physical user input remains unverified.

The earlier `5ead44e` runtime attempt remains failed. Its fixture blocked normal defense and incorrectly assumed receipts and exact deposit positions would already appear in published snapshots. The corrected opt-in fixture permits normal play after accepted-state publication and verifies exact authoritative deposit events plus later persistent states. No balance, bot behavior, deadline or deposit-distance rule was changed. Existing human trial and rollback workspaces were preserved.
