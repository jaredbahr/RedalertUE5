#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include <exception>

int main()
{
    try
    {
        PortableProofTests::AutomationTest Bridge;
        Bridge.TestEqual("real MD5 bridge known vector", FMD5::HashAnsiString("abc"),
            FString("900150983cd24fb0d6963f7d28e17f72"));
        Bridge.TestEqual("exact LastTravelDirection native test registration", PortableProofTests::Registry().size(), std::size_t(1));
        for (const auto& Test : PortableProofTests::Registry())
        {
            Bridge.TestEqual("exact LastTravelDirection native name", FString(Test.Name), FString("RedalertProof.Skirmish.LastTravelDirection"));
            const int Before = PortableProofTests::Failures;
            const bool Completed = Test.Run();
            if (!Completed && Before == PortableProofTests::Failures) ++PortableProofTests::Failures;
            std::cout << Test.Name << ": " << (Completed && Before == PortableProofTests::Failures ? "PASS" : "FAIL") << '\n';
        }
        std::cout << PortableProofTests::Checks << " checks, " << PortableProofTests::Failures << " failures\n";
        return PortableProofTests::Failures == 0 ? 0 : 1;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
