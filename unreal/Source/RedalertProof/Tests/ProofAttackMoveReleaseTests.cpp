#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Runtime/ProofAttackMoveReleasePolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveReleaseBarrierTest, "RedalertProof.AttackMove.ReleaseBarrier",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProofAttackMoveReleaseBarrierTest::RunTest(const FString&)
{
    const FString Token = TEXT("0123456789abcdef0123456789abcdef");
    const FString Clients = TEXT("clients:") + Token, Host = TEXT("host:") + Token;
    const auto Decide = [&](bool Startup, bool IsHost, bool Ready, bool HasClients, const FString& C, bool HasHost, const FString& H)
    { return FProofAttackMoveReleasePolicy::Evaluate(Startup, IsHost, Ready, HasClients, C, HasHost, H, Token); };
    TestFalse(TEXT("Fresh startup accepted"), Decide(true, true, false, false, TEXT(""), false, TEXT("")).Error != nullptr);
    TestTrue(TEXT("Stale client marker rejected"), Decide(true, true, false, true, Clients, false, TEXT("")).Error != nullptr);
    TestTrue(TEXT("Stale host marker rejected"), Decide(true, false, false, false, TEXT(""), true, Host).Error != nullptr);
    const auto Waiting = Decide(false, true, true, true, Clients, false, TEXT(""));
    TestTrue(TEXT("Host holds while clients exit"), Waiting.bStopTick && !Waiting.bComplete && !Waiting.Error);
    TestTrue(TEXT("Ready client completes on client phase"), Decide(false, false, true, true, Clients, false, TEXT("")).bComplete);
    TestTrue(TEXT("Host completes only on host phase"), Decide(false, true, true, true, Clients, true, Host).bComplete);
    TestFalse(TEXT("No marker continues normal fixture"), Decide(false, true, true, false, TEXT(""), false, TEXT("")).bStopTick);
    TestTrue(TEXT("Not-ready client rejects release"), Decide(false, false, false, true, Clients, false, TEXT("")).Error != nullptr);
    TestTrue(TEXT("Host phase without client phase rejected"), Decide(false, true, true, false, TEXT(""), true, Host).Error != nullptr);
    TestTrue(TEXT("Live client rejects early host phase"), Decide(false, false, true, true, Clients, true, Host).Error != nullptr);
    TestTrue(TEXT("Wrong client phase rejected"), Decide(false, false, true, true, Host, false, TEXT("")).Error != nullptr);
    TestTrue(TEXT("Wrong client token rejected"), Decide(false, false, true, true, TEXT("clients:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), false, TEXT("")).Error != nullptr);
    TestTrue(TEXT("Wrong host phase rejected"), Decide(false, true, true, true, Clients, true, Clients).Error != nullptr);
    TestTrue(TEXT("Wrong host token rejected"), Decide(false, true, true, true, Clients, true, TEXT("host:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")).Error != nullptr);
    TestFalse(TEXT("Malformed token rejected"), FProofAttackMoveReleasePolicy::IsToken(TEXT("not-a-run-token")));
    TestFalse(TEXT("Nonhex token rejected"), FProofAttackMoveReleasePolicy::IsToken(TEXT("gggggggggggggggggggggggggggggggg")));
    return true;
}
#endif
