#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "../unreal/Source/RedalertProof/Tests/ProofGroupOrderQueueTests.cpp"

#include <array>
#include <cstring>
#include <iostream>

int main()
{
    constexpr std::array<const char*, 5> ExpectedTests = {
        "RedalertProof.Controls.QueueCoalescesAndPreservesOrder",
        "RedalertProof.Controls.QueueWallPacing",
        "RedalertProof.Controls.QueueSnapshotRevalidation",
        "RedalertProof.Controls.QueueInvalidBatchIsAtomic",
        "RedalertProof.Controls.QueueCapacityIsAtomic"
    };
    auto& Tests = PortableProofTests::Registry();
    if (Tests.size() != ExpectedTests.size())
    {
        std::cerr << "Expected exactly " << ExpectedTests.size() << " queue test groups, found " << Tests.size() << '\n';
        return 2;
    }
    bool NamesMatch = true;
    for (const char* Expected : ExpectedTests)
    {
        const bool Found = std::any_of(Tests.begin(), Tests.end(), [Expected](const auto& Test)
            { return std::strcmp(Test.Name, Expected) == 0; });
        if (!Found) { std::cerr << "Missing queue test group: " << Expected << '\n'; NamesMatch = false; }
    }
    if (!NamesMatch) { return 2; }

    PortableProofTests::AutomationTest DigestChecks;
    DigestChecks.TestEqual(TEXT("Portable digest bridge: empty input"),
        FMD5::HashAnsiString(""), FString("d41d8cd98f00b204e9800998ecf8427e"));
    DigestChecks.TestEqual(TEXT("Portable digest bridge: abc"),
        FMD5::HashAnsiString("abc"), FString("900150983cd24fb0d6963f7d28e17f72"));
    DigestChecks.TestEqual(TEXT("Portable digest bridge: message digest"),
        FMD5::HashAnsiString("message digest"), FString("f96b697d7cb7938d525a2f31aaf161d0"));

    for (const auto& Test : Tests)
    {
        const int Before = PortableProofTests::Failures;
        const bool Completed = Test.Run();
        if (!Completed && Before == PortableProofTests::Failures) { ++PortableProofTests::Failures; }
        std::cout << Test.Name << ": " << (Completed && Before == PortableProofTests::Failures ? "PASS" : "FAIL") << '\n';
    }
    std::cout << PortableProofTests::Checks << " checks, " << PortableProofTests::Failures << " failures\n";
    return PortableProofTests::Failures == 0 ? 0 : 1;
}
