#pragma once
#include "CoreMinimal.h"
#include <iostream>

namespace PortableProofTests
{
inline int Checks = 0;
inline int Failures = 0;
using TestFunction = bool (*)();
struct Test { const char* Name; TestFunction Run; };
inline std::vector<Test>& Registry() { static std::vector<Test> Tests; return Tests; }
struct Register
{
    Register(const char* Name, TestFunction Run) { Registry().push_back({Name, Run}); }
};
class AutomationTest
{
public:
    bool TestTrue(const FString& Message, bool Value)
    {
        ++Checks;
        if (!Value) { ++Failures; std::cerr << "FAIL: " << *Message << '\n'; }
        return Value;
    }
    bool TestFalse(const FString& Message, bool Value) { return TestTrue(Message, !Value); }
    template<typename A, typename B> bool TestEqual(const FString& Message, const A& Actual, const B& Expected)
    { return TestTrue(Message, Actual == Expected); }
};
}
#define IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, Name, Flags) \
class Class : public PortableProofTests::AutomationTest { public: bool RunTest(const FString&); }; \
static PortableProofTests::Register Register_##Class(Name, []() { Class Test; return Test.RunTest(FString()); });
