#include "Misc/AutomationTest.h"
#include "Tests/ProofSkirmishBotTests.cpp"
#include "Tests/ProofBotMinerRecoveryTests.cpp"
#include "Tests/ProofBotFinalCargoTests.cpp"
#include "Tests/ProofBotFinalCargoRecoveryTests.cpp"

#include <array>
#include <cstring>

static_assert(std::is_enum_v<decltype(INDEX_NONE)>,
    "Portable INDEX_NONE must expose UE's enum-shaped lambda return-type checks.");

int main()
{
    constexpr std::array<const char*, 8> Expected = {
        "RedalertProof.Skirmish.BotPlanner", "RedalertProof.Skirmish.BotModelPath",
        "RedalertProof.Skirmish.BotRefineryPlanning", "RedalertProof.Skirmish.BotMinerRotation",
        "RedalertProof.Skirmish.BotMinerRecoveryPlanner", "RedalertProof.Skirmish.BotMinerRecoveryModel",
        "RedalertProof.Skirmish.BotFinalCargoPlanner", "RedalertProof.Skirmish.BotFinalCargoPublicRecovery"
    };
    const auto& Registered = PortableProofTests::Registry();
    if (Registered.size() != Expected.size())
    {
        std::cerr << "Expected exactly eight bot regression groups\n";
        return 2;
    }
    for (const char* Name : Expected)
    {
        if (std::count_if(Registered.begin(), Registered.end(), [Name](const auto& Test)
            { return std::strcmp(Test.Name, Name) == 0; }) != 1)
        {
            std::cerr << "Missing or duplicate bot group: " << Name << '\n';
            return 2;
        }
    }
    for (const auto& Test : PortableProofTests::Registry())
    {
        const int Before = PortableProofTests::Failures;
        const bool Completed = Test.Run();
        if (!Completed && Before == PortableProofTests::Failures) ++PortableProofTests::Failures;
        std::cout << Test.Name << ": " << (Completed && Before == PortableProofTests::Failures ? "PASS" : "FAIL") << '\n';
    }
    std::cout << PortableProofTests::Checks << " checks, " << PortableProofTests::Failures << " failures\n";
    return PortableProofTests::Failures == 0 ? 0 : 1;
}
