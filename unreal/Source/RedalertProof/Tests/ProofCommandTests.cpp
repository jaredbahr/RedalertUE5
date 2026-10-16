#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include <limits>

namespace
{
FProofCommand Order(int32 Sequence, EProofCommandType Type, int32 Unit = 0, int32 Target = 0, FVector Destination = FVector::ZeroVector)
{
    FProofCommand C;
    C.Sequence = Sequence; C.Type = Type; C.UnitId = Unit; C.TargetUnitId = Target; C.Destination = Destination;
    return C;
}
const FProofUnitRecord* Find(const FProofSnapshot& S, int32 Id)
{
    return S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofValidationTest, "RedalertProof.Commands.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofValidationTest::RunTest(const FString&)
{
    FProofCommandModel M;
    M.AddPlayer(1); M.AddPlayer(2); M.AddPlayer(100);
    const int32 Human = M.SeedUnit(1, FVector::ZeroVector);
    const int32 Enemy = M.SeedUnit(2, FVector(100, 0, 0));
    const int32 Bot = M.SeedUnit(100, FVector::ZeroVector);
    TestTrue(TEXT("Human valid move"), M.Submit(1, Order(1, EProofCommandType::Move, Human, 0, FVector(20,0,0))).bAccepted);
    TestTrue(TEXT("Bot uses same move validator"), M.Submit(100, Order(1, EProofCommandType::Move, Bot, 0, FVector(20,0,0))).bAccepted);
    TestFalse(TEXT("Cannot command enemy"), M.Submit(1, Order(2, EProofCommandType::Move, Enemy)).bAccepted);
    TestFalse(TEXT("Rejected gameplay sequence cannot replay"), M.Submit(1, Order(2, EProofCommandType::Move, Human)).bAccepted);
    TestFalse(TEXT("Unknown trusted owner"), M.Submit(99, Order(1, EProofCommandType::Move, Human)).bAccepted);
    TestFalse(TEXT("Skipped sequence rejected"), M.Submit(1, Order(5, EProofCommandType::Move, Human)).bAccepted);
    TestFalse(TEXT("Nonpositive sequence rejected"), M.Submit(1, Order(0, EProofCommandType::Move, Human)).bAccepted);
    FProofCommand Nonfinite = Order(3, EProofCommandType::Move, Human);
    Nonfinite.Destination.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("NaN position rejected"), M.Submit(1, Nonfinite).bAccepted);
    Nonfinite.Destination.X = std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Infinite position rejected"), M.Submit(1, Nonfinite).bAccepted);
    TestFalse(TEXT("Invalid enum rejected"), M.Submit(1, Order(3, static_cast<EProofCommandType>(255), Human)).bAccepted);
    TestTrue(TEXT("Malformed envelopes did not consume sequence"), M.Submit(1, Order(3, EProofCommandType::Move, Human)).bAccepted);
    TestFalse(TEXT("Out of bounds rejected"), M.Submit(1, Order(4, EProofCommandType::Move, Human, 0, FVector(1000000,0,0))).bAccepted);
    TestFalse(TEXT("Friendly attack rejected"), M.Submit(100, Order(2, EProofCommandType::Attack, Bot, Bot)).bAccepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofResourceRateTest, "RedalertProof.Commands.ResourcesAndRate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofResourceRateTest::RunTest(const FString&)
{
    FProofCommandModel M(2); M.AddPlayer(1, 50); M.AddPlayer(100, 50);
    TestTrue(TEXT("Human production spends resources"), M.Submit(1, Order(1, EProofCommandType::Produce)).bAccepted);
    TestFalse(TEXT("Human cannot overspend"), M.Submit(1, Order(2, EProofCommandType::Produce)).bAccepted);
    TestTrue(TEXT("Bot production same rules"), M.Submit(100, Order(1, EProofCommandType::Produce)).bAccepted);
    TestFalse(TEXT("Bot cannot overspend"), M.Submit(100, Order(2, EProofCommandType::Produce)).bAccepted);
    const auto Spent=M.GetSnapshot();
    TestEqual(TEXT("Exactly two successful productions"),Spent.Units.Num(),2);
    for (const auto& Player:Spent.Players) { TestEqual(TEXT("Credits deducted once per success"),Player.Credits,0); }
    FProofCommandModel Rate(1); Rate.AddPlayer(1);
    const int32 U = Rate.SeedUnit(1, FVector::ZeroVector);
    TestTrue(TEXT("First allowed"), Rate.Submit(1, Order(1, EProofCommandType::Move, U)).bAccepted);
    TestEqual(TEXT("Rate limit reason"), Rate.Submit(1, Order(2, EProofCommandType::Move, U)).Reason, FString(TEXT("rate")));
    Rate.Advance(0.f);
    TestTrue(TEXT("Advance resets rate allowance"), Rate.Submit(1, Order(3, EProofCommandType::Move, U)).bAccepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSimulationTest, "RedalertProof.Commands.SimulationAndSnapshots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSimulationTest::RunTest(const FString&)
{
    FProofCommandModel M; M.AddPlayer(1); M.AddPlayer(2);
    const int32 A = M.SeedUnit(1, FVector::ZeroVector), B = M.SeedUnit(2, FVector(100,0,0));
    const auto Before = M.GetSnapshot(); const FString Digest = Before.ComputeDigest();
    M.Submit(1, Order(1, EProofCommandType::Attack, A, B));
    M.Submit(2, Order(1, EProofCommandType::Attack, B, A)); M.Advance(4.f);
    const auto After = M.GetSnapshot();
    TestEqual(TEXT("Mutual lethal damage A"), Find(After,A)->HitPoints, 0.f);
    TestEqual(TEXT("Mutual lethal damage B"), Find(After,B)->HitPoints, 0.f);
    TestEqual(TEXT("Detached snapshot HP"), Find(Before,A)->HitPoints, 100.f);
    TestEqual(TEXT("Detached digest stable"), Before.ComputeDigest(), Digest);
    TestNotEqual(TEXT("Changed snapshot changes digest"), After.ComputeDigest(), Digest);
    M.Reset(); M.AddPlayer(1); const int32 Moving = M.SeedUnit(1, FVector::ZeroVector);
    M.Submit(1, Order(1, EProofCommandType::Move, Moving, 0, FVector(1000,0,0))); M.Advance(1.f);
    TestTrue(TEXT("Movement progresses without overshoot"), Find(M.GetSnapshot(),Moving)->Position.Equals(FVector(150,0,0), .01f));
    FProofCommandModel Twin; Twin.AddPlayer(1); const int32 TwinId = Twin.SeedUnit(1,FVector::ZeroVector);
    Twin.Submit(1, Order(1,EProofCommandType::Move,TwinId,0,FVector(1000,0,0))); Twin.Advance(1.f);
    TestEqual(TEXT("Identical simulation has identical digest"), M.GetSnapshot().ComputeDigest(), Twin.GetSnapshot().ComputeDigest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofPhaseOrderTest, "RedalertProof.Commands.PhaseOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofPhaseOrderTest::RunTest(const FString&)
{
    for (int32 Reverse = 0; Reverse < 2; ++Reverse)
    {
        FProofCommandModel M; M.AddPlayer(1); M.AddPlayer(2);
        int32 A, B;
        if (!Reverse) { A=M.SeedUnit(1,FVector::ZeroVector); B=M.SeedUnit(2,FVector(450,0,0)); }
        else { B=M.SeedUnit(2,FVector(450,0,0)); A=M.SeedUnit(1,FVector::ZeroVector); }
        M.Submit(1,Order(1,EProofCommandType::Attack,A,B)); M.Submit(2,Order(1,EProofCommandType::Attack,B,A)); M.Advance(1.f);
        const auto S=M.GetSnapshot();
        TestTrue(TEXT("Both movement decisions use initial positions"), Find(S,A)->Position.Equals(FVector(150,0,0),.01f) && Find(S,B)->Position.Equals(FVector(300,0,0),.01f));
        TestEqual(TEXT("Both damage decisions use post-move positions A"), Find(S,A)->HitPoints,75.f);
        TestEqual(TEXT("Insertion order does not bias B damage"),Find(S,B)->HitPoints,75.f);
    }
    return true;
}
#endif
