#include "Misc/AutomationTest.h"
#include "Tests/ProofSkirmishBotTests.cpp"
#include "Tests/ProofBotMinerRecoveryTests.cpp"

static_assert(std::is_enum_v<decltype(INDEX_NONE)>,
    "Portable INDEX_NONE must expose UE's enum-shaped lambda return-type checks.");

int main()
{
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
