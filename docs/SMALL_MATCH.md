# Small-match implementation contract

User confirmation: Jared said the existing opened slice "it works. lot of work to do though." This confirms general usability only; individual controls/Alt-Tab are not all confirmed. Jared approved one complete small-match milestone on 2026-10-16: build a base, gather resources, produce units, fight a bot, win or lose. Existing placeholder assets only. No PR merges, paid tools, deployment or hosted builds. The live practice session remains untouched.

Integration baseline: PR19 fd7eb1f plus local cherry-picks of reserved owner IDs and PR20 RPC ingress. Dependency PRs remain open; local integration does not merge them on GitHub.

## Shared code contract (implementation defaults, not final balance)

Opt-in `-ProofSkirmish` keeps legacy proof fixtures unchanged. Host-authoritative FProofCommandModel remains the only mutation/command path. One human or two cooperative human owners (team0) versus bot owner100 (team1). Each starts with HQ and one Harvester, 350 credits; finite local ore field. Generic models retain clear role labels. Proposed costs: refinery100, factory150, combat unit50. Require refinery to gather/deposit and factory to produce. Build within800cm of own live HQ, avoid overlapping structures. Provisional small-fixture cap12 live records per owner, not a performance budget. Attack range300/speed150/damage25 remain existing proof values; HQ300HP, other structures200HP, mobiles100HP. Bot attack grace20 simulation seconds. No collision/pathfinding or roster expansion claims.

Model/header owned by economy worker:
- Extend enum EProofCommandType with BuildRefinery, BuildFactory, Gather (existing Move/Attack/Produce values preserved).
- enum EProofUnitKind {Combat,Harvester,HQ,Refinery,Factory}; Unit.Kind defaultsCombat, Unit.Cargo int32 defaults0.
- enum EProofMatchPhase {Inactive,Playing,Finished}; Snapshot.bSkirmish false, MatchPhase Inactive, WinnerTeam INDEX_NONE (finished + INDEX_NONE meansdraw), Resources TArray<FProofResourceRecord>.
- FProofPlayerRecord.TeamId int32 defaults0. FProofResourceRecord {ResourceId int32, Position FVector, Remaining int32}.
- Model.StartSkirmish() resets/enables; Model.SeedSkirmishPlayer(int32Owner,int32Team,const FVector& HQPosition) adds350credits/HQ/Harvester/finiteore; bool result. Existing API stays valid. Include new fields in canonical digest.
- Gather uses Command.UnitId harvester and Command.TargetUnitId resourceID. Harvest travel, finite extraction/cargo50, return-to-refinery/deposit, repeat; no free passive income. Structure Build commands use Destination; Produce uses live factory and costs50. Skirmish attack only Combat against opposing team, structures immobile. Freeze simulation/deny commands at terminal outcome; keep outcome stable.
- Pure rules API in Skirmish/ProofSkirmishRules.h: struct FProofMatchOutcome {bool bFinished=false;int32 WinnerTeam=INDEX_NONE;}; class FProofSkirmishRules {public: static FProofMatchOutcome Evaluate(const FProofSnapshot&);}; Model calls after simultaneous damage. Rules waits until both teams represented, checks live HQ for each team; both absent draw.
- Bot API Skirmish/ProofSkirmishBot.h: class FProofSkirmishBot { public: TArray<FProofCommand> Decide(const FProofSnapshot&,int32OwnerId,doubleNowSeconds);void Reset(); }; bounded decision cadence, own monotonic command sequence, maxsmallbatch. Build/gather/produce/attack through commands only; choose legal deterministic targets/placements. No cheating/direct mutation.

Root owns runtime GameMode/controller/Pawn integration, launch/test scripts and evidence docs. Economy owns Commands/ProofCommandModel.h/.cpp and Tests/ProofSkirmishEconomyTests.cpp. Bot owns Skirmish/ProofSkirmishBot.h/.cpp and Tests/ProofSkirmishBotTests.cpp. Rules/UI owns Skirmish/ProofSkirmishRules.h/.cpp, Tests/ProofSkirmishRulesTests.cpp and Presentation/ProofHUD.cpp. Do not edit another owner's files; communicate contract corrections.

## Acceptance gates

1. Pure deterministic/state tests: build ownership, costs/prerequisites/placement, finite harvest travel/cargo/deposit and no negative resources, production cap, repeated same command traces yield same digest, winner/loser/draw and frozen terminal state, bot uses valid shared commands and progresses from base/economy to attacks without grants.
2. Actual UE standalone/offscreen match: real replicated snapshot-backed view; build refinery/factory through same bound handlers, gather with Harvester, earn/debit resources, produce/select/attack, bot visibly progresses and fights, terminal victory and defeat fixtures, clear result UI/captures. Automation does not establish physical input verification.
3. Existing seven native groups, bounded ingress close/isolation regression, existing controls proof and basic actual two-client shared snapshot/outcome regression. New small-match two-client cooperative fixture as appropriate. Preserve exact SHAs and failure logs. One serialized heavy job, two CPUs/Normal tests, D: cache/temp soft guards; observe C: writes without zero-write promises.
4. Document exact dependencies, controls, provisional rules, tests and remaining limits. No merge/deploy/extra GitHub fee. Open next visible game only with Jared's explicit request.
