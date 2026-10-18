#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include <exception>
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#define PROOF_TRAVEL_HAS_MXCSR 1
#else
#define PROOF_TRAVEL_HAS_MXCSR 0
#endif

int main(int Argc, char** Argv)
{
    const std::string Mode = Argc == 1 ? "native" : Argv[1];
    if (Argc > 2 || (Mode != "native" && Mode != "gradual" && Mode != "ftz" && Mode != "daz" && Mode != "both"))
    {
        std::cerr << "Usage: " << Argv[0] << " [native|gradual|ftz|daz|both]\n";
        return 2;
    }
    const bool Native = Mode == "native";
#if PROOF_TRAVEL_HAS_MXCSR
    constexpr unsigned ModeMask = 0x8040u;
    const unsigned Saved = _mm_getcsr();
    const unsigned Requested = Mode == "ftz" ? 0x8000u : Mode == "daz" ? 0x40u : Mode == "both" ? ModeMask : 0u;
    struct FRestoreMxcsr
    {
        unsigned Saved;
        bool Active;
        ~FRestoreMxcsr() { if (Active) { _mm_setcsr(Saved); } }
    } Restore{Saved, !Native};
    // Only explicitly requested, isolated portable processes change FP mode.
    // The baseline and the actual native automation body never set MXCSR.
    if (!Native) { _mm_setcsr((Saved & ~ModeMask) | Requested); }
    std::cout << "FP mode=" << Mode << " initial_mxcsr=0x" << std::hex << Saved
        << " active_mxcsr=0x" << _mm_getcsr() << std::dec << '\n';
#else
    if (!Native)
    {
        std::cerr << "FAIL: requested FP mode " << Mode << " requires x86 MXCSR support; no mode was skipped.\n";
        return 2;
    }
    std::cout << "FP mode=native (MXCSR is unavailable on this architecture)\n";
#endif
    try
    {
        PortableProofTests::AutomationTest Bridge;
#if PROOF_TRAVEL_HAS_MXCSR
        Bridge.TestEqual("requested FP controls are active", _mm_getcsr() & ModeMask, Native ? Saved & ModeMask : Requested);
#else
        Bridge.TestTrue("native FP environment was left untouched", Native);
#endif
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
#if PROOF_TRAVEL_HAS_MXCSR
        if (!Native)
        {
            _mm_setcsr(Saved);
            Restore.Active = false;
            Bridge.TestEqual("explicit FP mode restores complete original MXCSR", _mm_getcsr(), Saved);
            std::cout << "restored_mxcsr=0x" << std::hex << _mm_getcsr() << std::dec << '\n';
        }
        else
        {
            // Arithmetic may set exception-status bits 0..5. No control bit may change.
            Bridge.TestEqual("native default retains all original MXCSR control bits", _mm_getcsr() & ~0x3fu, Saved & ~0x3fu);
        }
#else
        Bridge.TestTrue("native FP environment remains untouched", Native);
#endif
        std::cout << "FP mode=" << Mode << ": " << PortableProofTests::Checks << " checks, " << PortableProofTests::Failures << " failures\n";
        return PortableProofTests::Failures == 0 ? 0 : 1;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
