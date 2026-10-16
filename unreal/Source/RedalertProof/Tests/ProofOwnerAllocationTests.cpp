#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofOwnerIds.h"
#include "Commands/ProofCommandModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOwnerAllocationTest, "RedalertProof.Commands.OwnerAllocation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOwnerAllocationTest::RunTest(const FString&)
{
    int32 OwnerId = INDEX_NONE;
    FProofHumanOwnerAllocator Boundary(99);
    TestTrue(TEXT("Allocate last human before reservation"), Boundary.TryAllocate(OwnerId));
    TestEqual(TEXT("Boundary starts at 99"), OwnerId, 99);
    TestTrue(TEXT("Allocation crosses reservation"), Boundary.TryAllocate(OwnerId));
    TestEqual(TEXT("Human skips bot 100"), OwnerId, 101);
    TestTrue(TEXT("Continue after reservation"), Boundary.TryAllocate(OwnerId));
    TestEqual(TEXT("Following human is distinct"), OwnerId, 102);
    FProofHumanOwnerAllocator ReservedStart(ProofOwnerIds::Bot);
    TestTrue(TEXT("Reserved starting candidate is skipped"), ReservedStart.TryAllocate(OwnerId));
    TestEqual(TEXT("Reserved start yields 101"), OwnerId, 101);

    FProofCommandModel Model;
    Model.AddPlayer(ProofOwnerIds::Bot);
    const int32 BotUnit = Model.SeedUnit(ProofOwnerIds::Bot, FVector::ZeroVector);
    FProofCommand BotMove;
    BotMove.Sequence = 1; BotMove.Type = EProofCommandType::Move;
    BotMove.UnitId = BotUnit; BotMove.Destination = FVector(150, 0, 0);
    TestTrue(TEXT("Bot has a pending order before human allocation"), Model.Submit(ProofOwnerIds::Bot, BotMove).bAccepted);

    FProofHumanOwnerAllocator Humans;
    TSet<int32> Seen;
    for (int32 Index = 0; Index < 256; ++Index)
    {
        if (!TestTrue(TEXT("Repeated human allocation succeeds"), Humans.TryAllocate(OwnerId))) { return false; }
        TestTrue(TEXT("Human identity is positive"), OwnerId > 0);
        TestNotEqual(TEXT("No human receives bot identity"), OwnerId, ProofOwnerIds::Bot);
        TestFalse(TEXT("No human identity is reused"), Seen.Contains(OwnerId));
        if (Index < 2) { TestEqual(TEXT("Initial fixture human identities preserved"), OwnerId, Index + 1); }
        Seen.Add(OwnerId);
        Model.AddPlayer(OwnerId);
        FProofCommand Steal = BotMove;
        const FProofCommandResult Rejected = Model.Submit(OwnerId, Steal);
        TestFalse(TEXT("Allocated human cannot command bot unit"), Rejected.bAccepted);
        TestEqual(TEXT("Bot command ownership remains isolated"), Rejected.Reason, FString(TEXT("ownership")));
    }
    TestEqual(TEXT("256 unique human identities"), Seen.Num(), 256);
    const FProofSnapshot Allocated = Model.GetSnapshot();
    TestEqual(TEXT("Human allocation preserves one bot unit"), Allocated.Units.Num(), 1);
    TestEqual(TEXT("Bot unit retains its owner"), Allocated.Units[0].OwnerId, ProofOwnerIds::Bot);
    const FProofPlayerRecord* Bot = Allocated.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == ProofOwnerIds::Bot; });
    if (!TestNotNull(TEXT("Original bot record retained"), Bot)) { return false; }
    TestEqual(TEXT("Bot resources were not reset or spent"), Bot->Credits, 200);
    Model.Advance(1.f);
    TestTrue(TEXT("Original bot order survives human allocation"), Model.GetSnapshot().Units[0].Position.Equals(FVector(150, 0, 0), .001));
    FProofCommand Produce;
    Produce.Sequence = 2; Produce.Type = EProofCommandType::Produce;
    TestTrue(TEXT("Bot command sequence remains independent"), Model.Submit(ProofOwnerIds::Bot, Produce).bAccepted);
    const FProofSnapshot Produced = Model.GetSnapshot();
    Bot = Produced.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == ProofOwnerIds::Bot; });
    TestEqual(TEXT("Bot production spends original resource account"), Bot->Credits, 150);

    FProofHumanOwnerAllocator End(MAX_int32 - 1);
    TestTrue(TEXT("Penultimate identity allocates"), End.TryAllocate(OwnerId));
    TestEqual(TEXT("Penultimate identity exact"), OwnerId, MAX_int32 - 1);
    TestTrue(TEXT("Final identity allocates once"), End.TryAllocate(OwnerId));
    TestEqual(TEXT("Final identity exact"), OwnerId, MAX_int32);
    for (int32 Attempt = 0; Attempt < 3; ++Attempt)
    {
        OwnerId = 123;
        TestFalse(TEXT("Exhaustion fails repeatedly without wrap or reuse"), End.TryAllocate(OwnerId));
        TestEqual(TEXT("Exhaustion clears stale output"), OwnerId, INDEX_NONE);
    }
    for (const int32 InvalidStart : { 0, -1 })
    {
        FProofHumanOwnerAllocator Invalid(InvalidStart);
        OwnerId = 123;
        TestFalse(TEXT("Invalid nonpositive cursor fails closed"), Invalid.TryAllocate(OwnerId));
        TestEqual(TEXT("Invalid cursor clears output"), OwnerId, INDEX_NONE);
    }
    return true;
}
#endif
