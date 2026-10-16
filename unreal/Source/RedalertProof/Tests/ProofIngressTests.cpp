#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofIngressBudget.h"
#include "Commands/ProofCommandModel.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofIngressBudgetTest, "RedalertProof.Commands.IngressBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofIngressBudgetTest::RunTest(const FString&)
{
    FProofIngressBudget First;
    FProofIngressBudget Second;
    for (int32 Index = 0; Index < 64; ++Index)
    {
        TestTrue(TEXT("Initial burst admitted"), First.TryAdmit(10.0));
    }
    TestFalse(TEXT("Burst plus one trips the connection"), First.TryAdmit(10.0));
    TestTrue(TEXT("Trip is latched"), First.IsTripped());
    TestFalse(TEXT("Time cannot silently recover a tripped connection"), First.TryAdmit(1000.0));
    TestTrue(TEXT("Another connection has its own allowance"), Second.TryAdmit(10.0));

    FProofIngressBudget Refill;
    for (int32 Index = 0; Index < 64; ++Index) { Refill.TryAdmit(0.0); }
    TestTrue(TEXT("One token refills at 1/32 second"), Refill.TryAdmit(1.0 / 32.0));
    TestFalse(TEXT("No second token at same instant"), Refill.TryAdmit(1.0 / 32.0));

    FProofIngressBudget Clock;
    for (int32 Index = 0; Index < 63; ++Index) { Clock.TryAdmit(5.0); }
    TestTrue(TEXT("Nonfinite time cannot add tokens"), Clock.TryAdmit(std::numeric_limits<double>::infinity()));
    TestFalse(TEXT("Regressed time cannot add tokens"), Clock.TryAdmit(4.0));

    FProofIngressBudget Saturation;
    TestTrue(TEXT("First token"), Saturation.TryAdmit(0.0));
    TestTrue(TEXT("Large idle period refills but caps burst"), Saturation.TryAdmit(100000.0));
    for (int32 Index = 0; Index < 63; ++Index) { Saturation.TryAdmit(100000.0); }
    TestFalse(TEXT("Idle refill cannot exceed burst"), Saturation.TryAdmit(100000.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofIngressFixtureTest, "RedalertProof.Commands.IngressFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofIngressFixtureTest::RunTest(const FString&)
{
    FProofIngressBudget Budget;
    FProofCommandModel Model;
    Model.AddPlayer(1); Model.AddPlayer(2);
    const int32 OwnUnit = Model.SeedUnit(1, FVector::ZeroVector);
    const int32 EnemyUnit = Model.SeedUnit(2, FVector(100.0, 0.0, 0.0));
    const double Times[] = { 0.2, 4.0, 4.5, 5.0 };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FProofCommand Command;
        Command.Sequence = Index + 1;
        Command.UnitId = Index == 3 ? EnemyUnit : OwnUnit;
        Command.Type = Index == 1 ? EProofCommandType::Attack :
            Index == 2 ? EProofCommandType::Produce : EProofCommandType::Move;
        Command.TargetUnitId = EnemyUnit;
        Command.Destination = FVector(20.0, 0.0, 0.0);
        TestTrue(TEXT("Normal ordered fixture request admitted"), Budget.TryAdmit(Times[Index]));
        const FProofCommandResult Result = Model.Submit(1, Command);
        TestEqual(TEXT("Normal fixture receives its ordered receipt"), Result.Sequence, Index + 1);
        TestEqual(TEXT("Normal fixture acceptance remains unchanged"), Result.bAccepted, Index < 3);
        if (Index == 3) { TestEqual(TEXT("Wrong-owner receipt remains useful"), Result.Reason, FString(TEXT("ownership"))); }
    }
    TestFalse(TEXT("Normal fixture never trips ingress"), Budget.IsTripped());

    // This mirrors the adapter's ordering: every admitted malformed or replayed
    // RPC still gets one model receipt, while the overflow never enters Submit.
    FProofIngressBudget ReplayBudget;
    FProofCommandModel ReplayModel;
    ReplayModel.AddPlayer(1);
    ReplayModel.SeedUnit(1, FVector::ZeroVector);
    int32 Receipts = 0;
    for (int32 Index = 0; Index < 65; ++Index)
    {
        FProofCommand Bad;
        Bad.Sequence = Index == 0 || Index % 2 == 1 ? 1 : 0;
        Bad.UnitId = 1;
        if (ReplayBudget.TryAdmit(1.0))
        {
            const FProofCommandResult Result = ReplayModel.Submit(1, Bad);
            if (Index == 0) TestTrue(TEXT("First valid sequence accepted"), Result.bAccepted);
            else
            {
                TestFalse(TEXT("Malformed or duplicate envelope rejected"), Result.bAccepted);
                TestEqual(TEXT("Duplicate gets sequence rejection"), Result.Reason,
                    FString(Index % 2 == 1 ? TEXT("sequence") : TEXT("malformed")));
            }
            ++Receipts;
        }
    }
    TestEqual(TEXT("Only bounded admitted requests produce receipts"), Receipts, 64);
    TestTrue(TEXT("Malformed and replayed traffic trips ingress"), ReplayBudget.IsTripped());
    return true;
}
#endif
