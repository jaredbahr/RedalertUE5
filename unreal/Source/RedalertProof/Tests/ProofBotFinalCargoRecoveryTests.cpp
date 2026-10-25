#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skirmish/ProofSkirmishBot.h"

namespace
{
const FProofUnitRecord* PublicFinalCargoUnit(const FProofSnapshot& Snapshot, int32 UnitId)
{
    return Snapshot.Units.FindByPredicate([UnitId](const FProofUnitRecord& Unit)
        { return Unit.UnitId == UnitId; });
}

const FProofPlayerRecord* PublicFinalCargoPlayer(const FProofSnapshot& Snapshot, int32 OwnerId)
{
    return Snapshot.Players.FindByPredicate([OwnerId](const FProofPlayerRecord& Player)
        { return Player.OwnerId == OwnerId; });
}

int32 PublicFinalCargoLiveKindId(const FProofSnapshot& Snapshot, int32 OwnerId, EProofUnitKind Kind)
{
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == OwnerId && Unit.Kind == Kind && Unit.HitPoints > 0.f) return Unit.UnitId;
    return INDEX_NONE;
}

int32 PublicFinalCargoLiveKindCount(const FProofSnapshot& Snapshot, int32 OwnerId, EProofUnitKind Kind)
{
    int32 Count = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        Count += Unit.OwnerId == OwnerId && Unit.Kind == Kind && Unit.HitPoints > 0.f ? 1 : 0;
    return Count;
}

int32 PublicFinalCargoOreRemaining(const FProofSnapshot& Snapshot)
{
    int32 Remaining = 0;
    for (const FProofResourceRecord& Resource : Snapshot.Resources) Remaining += Resource.Remaining;
    return Remaining;
}

int32 PublicFinalCargoLoadedAmount(const FProofSnapshot& Snapshot, int32 OwnerId)
{
    int32 Cargo = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == OwnerId && Unit.Kind == EProofUnitKind::Harvester && Unit.HitPoints > 0.f)
            Cargo += Unit.Cargo;
    return Cargo;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotFinalCargoPublicRecoveryTest, "RedalertProof.Skirmish.BotFinalCargoPublicRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotFinalCargoPublicRecoveryTest::RunTest(const FString&)
{
    // This public-model history feeds a fresh snapshot planner. Setup has its
    // own owner sequence history, so submitted intents receive that owner's
    // next sequence. It is not an autonomous bot-match trace.
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Final cargo trace seeds bot owner"), Model.SeedSkirmishPlayer(100, 1, FVector::ZeroVector))
        || !TestTrue(TEXT("Final cargo trace seeds opposing owner"), Model.SeedSkirmishPlayer(1, 0, FVector(6000, 0, 0)))) return false;
    int32 BotSequence = 0, OpponentSequence = 0;
    const auto PublicFinalCargoSubmit = [&](int32 OwnerId, EProofCommandType Type, int32 UnitId,
        int32 TargetId, const FVector& Destination)
    {
        FProofCommand Command;
        Command.Sequence = OwnerId == 100 ? ++BotSequence : ++OpponentSequence;
        Command.Type = Type;
        Command.UnitId = UnitId;
        Command.TargetUnitId = TargetId;
        Command.Destination = Destination;
        const FProofCommandResult Result = Model.Submit(OwnerId, Command);
        return TestTrue(FString::Printf(TEXT("Final cargo public command %d accepted (%s)"),
            Command.Sequence, *Result.Reason), Result.bAccepted);
    };
    if (!PublicFinalCargoSubmit(100, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(350, 0, 0))) return false;
    const int32 OldRefineryId = PublicFinalCargoLiveKindId(Model.GetSnapshot(), 100, EProofUnitKind::Refinery);
    if (!TestTrue(TEXT("Original own refinery exists"), OldRefineryId != INDEX_NONE)) return false;
    if (!PublicFinalCargoSubmit(100, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(-350, 0, 0))
        || !PublicFinalCargoSubmit(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(6350, 0, 0))
        || !PublicFinalCargoSubmit(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(5650, 0, 0))
        || !PublicFinalCargoSubmit(1, EProofCommandType::Produce,
            PublicFinalCargoLiveKindId(Model.GetSnapshot(), 1, EProofUnitKind::Factory), INDEX_NONE, FVector::ZeroVector)) return false;
    const int32 AttackerId = PublicFinalCargoLiveKindId(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
    if (!TestTrue(TEXT("Public factory production creates the opposing attacker"), AttackerId != INDEX_NONE)) return false;

    const FProofSnapshot BeforeGather = Model.GetSnapshot();
    const FProofResourceRecord* OwnOre = BeforeGather.Resources.FindByPredicate([](const FProofResourceRecord& Resource)
        { return Resource.OwnerId == 100; });
    const FProofResourceRecord* OpposingOre = BeforeGather.Resources.FindByPredicate([](const FProofResourceRecord& Resource)
        { return Resource.OwnerId == 1; });
    if (!TestTrue(TEXT("Both owners have their real finite ore patches"), OwnOre && OpposingOre
        && OwnOre->ResourceId > 0 && OpposingOre->ResourceId > 0 && BeforeGather.Resources.Num() == 2)) return false;
    for (const FProofUnitRecord& Unit : BeforeGather.Units)
        if (Unit.Kind == EProofUnitKind::Harvester && Unit.HitPoints > 0.f
            && !PublicFinalCargoSubmit(Unit.OwnerId, EProofCommandType::Gather, Unit.UnitId,
                Unit.OwnerId == 100 ? OwnOre->ResourceId : OpposingOre->ResourceId, FVector::ZeroVector)) return false;
    int32 OreRemaining = PublicFinalCargoOreRemaining(Model.GetSnapshot());
    for (int32 Tick = 0; Tick < 200 && OreRemaining > 0; ++Tick)
    {
        Model.Advance(1.f);
        OreRemaining = PublicFinalCargoOreRemaining(Model.GetSnapshot());
    }
    if (!TestEqual(TEXT("Public harvesting exhausts every real ore patch"), OreRemaining, 0)) return false;

    const FProofSnapshot Exhausted = Model.GetSnapshot();
    const FProofPlayerRecord* ExhaustedOwner = PublicFinalCargoPlayer(Exhausted, 100);
    const int32 CargoAtExhaustion = PublicFinalCargoLoadedAmount(Exhausted, 100);
    TArray<int32> LoadedMinerIds;
    for (const FProofUnitRecord& Unit : Exhausted.Units)
        if (Unit.OwnerId == 100 && Unit.Kind == EProofUnitKind::Harvester && Unit.HitPoints > 0.f && Unit.Cargo > 0)
            LoadedMinerIds.Add(Unit.UnitId);
    if (!TestTrue(TEXT("Exhaustion leaves affordable recovery and real loaded own miners"), ExhaustedOwner
        && ExhaustedOwner->Credits >= FProofCommandModel::RefineryCost && CargoAtExhaustion > 0 && LoadedMinerIds.Num() > 0
        && Exhausted.MatchPhase == EProofMatchPhase::Playing)) return false;
    const int32 CreditsAtExhaustion = ExhaustedOwner->Credits;
    const int32 GatheredAtExhaustion = ExhaustedOwner->TotalGathered;

    // Replace every loaded own Gather with a specific ReturnCargo. On the next
    // long tick all miners arrive first, then combat kills that exact refinery
    // before deposit. Its death cancels ReturnCargo and preserves the cargo.
    for (int32 MinerId : LoadedMinerIds)
        if (!PublicFinalCargoSubmit(100, EProofCommandType::ReturnCargo, MinerId, OldRefineryId, FVector::ZeroVector)) return false;
    if (!PublicFinalCargoSubmit(1, EProofCommandType::Attack, AttackerId, OldRefineryId, FVector::ZeroVector)) return false;
    Model.Advance(40.f);
    const FProofSnapshot Lost = Model.GetSnapshot();
    const FProofUnitRecord* LostRefinery = PublicFinalCargoUnit(Lost, OldRefineryId);
    const FProofPlayerRecord* LostOwner = PublicFinalCargoPlayer(Lost, 100);
    if (!TestTrue(TEXT("Real combat destroys the last own refinery before cargo conversion"), LostRefinery
        && LostRefinery->HitPoints == 0.f && PublicFinalCargoLiveKindCount(Lost, 100, EProofUnitKind::Refinery) == 0)
        || !TestTrue(TEXT("Canceled explicit returns preserve all finite cargo and credits"), LostOwner
        && PublicFinalCargoLoadedAmount(Lost, 100) == CargoAtExhaustion
        && LostOwner->Credits == CreditsAtExhaustion && LostOwner->TotalGathered == GatheredAtExhaustion)) return false;
    for (int32 MinerId : LoadedMinerIds)
    {
        const FProofUnitRecord* OriginalMiner = PublicFinalCargoUnit(Exhausted, MinerId);
        const FProofUnitRecord* LoadedMiner = PublicFinalCargoUnit(Lost, MinerId);
        if (!TestTrue(TEXT("Every targeted miner survives with its original cargo"), OriginalMiner && LoadedMiner
            && LoadedMiner->HitPoints > 0.f && LoadedMiner->Cargo == OriginalMiner->Cargo)) return false;
    }

    if (!PublicFinalCargoSubmit(1, EProofCommandType::Move, AttackerId, INDEX_NONE, FVector(6000, 0, 0))) return false;
    Model.Advance(40.f);
    Model.Advance(1.f); // No replacement order can unload the canceled returns.
    const FProofSnapshot Stranded = Model.GetSnapshot();
    const FProofPlayerRecord* StrandedOwner = PublicFinalCargoPlayer(Stranded, 100);
    if (!TestTrue(TEXT("Additional public advances leave canceled cargo stranded and uncredited"), StrandedOwner
        && PublicFinalCargoLoadedAmount(Stranded, 100) == CargoAtExhaustion
        && StrandedOwner->Credits == CreditsAtExhaustion && StrandedOwner->TotalGathered == GatheredAtExhaustion
        && PublicFinalCargoLiveKindCount(Stranded, 100, EProofUnitKind::Refinery) == 0
        && PublicFinalCargoOreRemaining(Stranded) == 0 && Stranded.MatchPhase == EProofMatchPhase::Playing)) return false;

    FProofSkirmishBot Bot;
    const TArray<FProofCommand> Recovery = Bot.Decide(Stranded, 100, 0.0);
    int32 RebuildIntents = 0, RecoverySpend = 0;
    if (!TestTrue(TEXT("Public recovery plan retains the four-command budget"), Recovery.Num() <= 4)) return false;
    for (const FProofCommand& Intent : Recovery)
    {
        int32 Cost = 0;
        if (Intent.Type == EProofCommandType::BuildRefinery)
        {
            ++RebuildIntents;
            Cost = FProofCommandModel::RefineryCost;
        }
        else if (Intent.Type == EProofCommandType::Produce) Cost = FProofCommandModel::ProductionCost;
        else { TestTrue(TEXT("No miner return is planned before a live refinery is committed"), false); return false; }
        if (!PublicFinalCargoSubmit(100, Intent.Type, Intent.UnitId, Intent.TargetUnitId, Intent.Destination)) return false;
        RecoverySpend += Cost;
    }
    if (!TestEqual(TEXT("Loaded cargo justifies exactly one replacement refinery"), RebuildIntents, 1)) return false;
    Model.Advance(1.f);
    const FProofSnapshot Rebuilt = Model.GetSnapshot();
    const int32 NewRefineryId = PublicFinalCargoLiveKindId(Rebuilt, 100, EProofUnitKind::Refinery);
    const FProofPlayerRecord* RebuiltOwner = PublicFinalCargoPlayer(Rebuilt, 100);
    if (!TestTrue(TEXT("Shared validation commits exactly one live replacement refinery"), NewRefineryId != INDEX_NONE
        && NewRefineryId != OldRefineryId && PublicFinalCargoLiveKindCount(Rebuilt, 100, EProofUnitKind::Refinery) == 1)
        || !TestTrue(TEXT("Rebuilding charges accepted costs while canceled returns still wait"), RebuiltOwner
        && RebuiltOwner->Credits == CreditsAtExhaustion - RecoverySpend
        && RebuiltOwner->TotalGathered == GatheredAtExhaustion
        && PublicFinalCargoLoadedAmount(Rebuilt, 100) == CargoAtExhaustion)) return false;
    const int32 CreditsAfterRecovery = RebuiltOwner->Credits;

    // Submit only ReturnCargo intents during delivery so production/attack
    // choices cannot obscure the exact cargo-to-credit conservation check.
    TSet<int32> ReturnedMinerIds;
    for (int32 Tick = 1; Tick <= 60 && PublicFinalCargoLoadedAmount(Model.GetSnapshot(), 100) > 0; ++Tick)
    {
        const FProofSnapshot BeforeReturn = Model.GetSnapshot();
        const TArray<FProofCommand> Intents = Bot.Decide(BeforeReturn, 100, static_cast<double>(Tick));
        int32 ReturnIntents = 0;
        if (!TestTrue(TEXT("Delivery decisions retain the four-command budget"), Intents.Num() <= 4)) return false;
        for (const FProofCommand& Intent : Intents)
        {
            if (Intent.Type != EProofCommandType::ReturnCargo) continue;
            ++ReturnIntents;
            const FProofUnitRecord* Miner = PublicFinalCargoUnit(BeforeReturn, Intent.UnitId);
            const FProofUnitRecord* Refinery = PublicFinalCargoUnit(BeforeReturn, Intent.TargetUnitId);
            if (!TestTrue(TEXT("Final cargo intent names a loaded own miner and the committed live own refinery"), Miner && Refinery
                && Miner->OwnerId == 100 && Miner->Kind == EProofUnitKind::Harvester && Miner->HitPoints > 0.f && Miner->Cargo > 0
                && Refinery->OwnerId == 100 && Refinery->Kind == EProofUnitKind::Refinery && Refinery->HitPoints > 0.f
                && Refinery->UnitId == NewRefineryId && Intent.Destination == FVector::ZeroVector)) return false;
            if (!PublicFinalCargoSubmit(100, Intent.Type, Intent.UnitId, Intent.TargetUnitId, Intent.Destination)) return false;
            ReturnedMinerIds.Add(Intent.UnitId);
        }
        if (!TestTrue(TEXT("Delivery uses at most one miner intent per decision"), ReturnIntents <= 1)) return false;
        Model.Advance(1.f);
    }
    const FProofSnapshot Final = Model.GetSnapshot();
    const FProofPlayerRecord* FinalOwner = PublicFinalCargoPlayer(Final, 100);
    if (!TestTrue(TEXT("Final cargo owner remains present"), FinalOwner != nullptr)) return false;
    TestEqual(TEXT("Every originally loaded miner receives a shared-path ReturnCargo"), ReturnedMinerIds.Num(), LoadedMinerIds.Num());
    TestEqual(TEXT("All stranded cargo is delivered within the bounded public advance loop"), PublicFinalCargoLoadedAmount(Final, 100), 0);
    TestEqual(TEXT("Deposit adds exactly the original cargo to total gathered"), FinalOwner->TotalGathered,
        GatheredAtExhaustion + CargoAtExhaustion);
    TestEqual(TEXT("Deposit adds exactly the original cargo after accepted recovery spending"), FinalOwner->Credits,
        CreditsAfterRecovery + CargoAtExhaustion);
    TestEqual(TEXT("The full history conserves credits across rebuilding and deposit"), FinalOwner->Credits,
        CreditsAtExhaustion - RecoverySpend + CargoAtExhaustion);
    TestEqual(TEXT("Recovery and delivery create no new ore"), PublicFinalCargoOreRemaining(Final), 0);
    TestEqual(TEXT("Exactly one live replacement refinery remains"), PublicFinalCargoLiveKindCount(Final, 100, EProofUnitKind::Refinery), 1);
    TestTrue(TEXT("Public final cargo recovery leaves a playing match"), Final.MatchPhase == EProofMatchPhase::Playing);
    return true;
}
#endif
