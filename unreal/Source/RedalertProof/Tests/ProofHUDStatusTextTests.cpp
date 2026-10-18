#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofHUDStatusText.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofHUDEconomyTextTest, "RedalertProof.HUD.MarkerLayout.StatusEconomyValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofHUDEconomyTextTest::RunTest(const FString&)
{
    for (const int32 Value : {0, 2147483647})
    {
        const TArray<FString> Variants = ProofHUDStatusText::Economy(Value, 987654321);
        if (!TestEqual(TEXT("Full and compact economy variants"), Variants.Num(), 2)) return false;
        for (const FString& Variant : Variants)
        {
            TestTrue(TEXT("Credit field complete"), Variant.StartsWith(FString::Printf(TEXT("Credits: %d   "), Value))
                || Variant.StartsWith(FString::Printf(TEXT("Credits %d  "), Value)));
            TestTrue(TEXT("Delivered ore digits complete"), Variant.Contains(TEXT("987654321")));
            TestFalse(TEXT("Selection is not appended to economy"), Variant.Contains(TEXT("Selected")));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofHUDSelectionTextTest, "RedalertProof.HUD.MarkerLayout.StatusSelectionValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofHUDSelectionTextTest::RunTest(const FString&)
{
    const EProofUnitKind Kinds[] = {EProofUnitKind::Combat, EProofUnitKind::Harvester,
        EProofUnitKind::HQ, EProofUnitKind::Refinery, EProofUnitKind::Factory};
    const TCHAR* Names[] = {TEXT("Combat"), TEXT("Harvester"), TEXT("HQ"), TEXT("Refinery"), TEXT("Factory")};
    for (int32 KindIndex = 0; KindIndex < 5; ++KindIndex)
    {
        FProofUnitRecord Unit;
        Unit.UnitId = 2147483647; Unit.Kind = Kinds[KindIndex];
        Unit.HitPoints = 123.75f; Unit.Cargo = 987654321;
        Unit.bHasRallyPoint = true; Unit.RallyPoint = FVector(-1400, 600, 0);
        for (const int32 Count : {1, 12, 2147483646})
        {
            const TArray<FString> Variants = ProofHUDStatusText::Selection(&Unit, Count);
            if (!TestEqual(TEXT("Full and compact selection variants"), Variants.Num(), 2)) return false;
            for (const FString& Variant : Variants)
            {
                TestTrue(TEXT("Complete primary ID"), Variant.Contains(TEXT("#2147483647")));
                TestTrue(TEXT("Complete selection count field"), Variant.StartsWith(FString::Printf(TEXT("Selected: %d   "), Count))
                    || Variant.StartsWith(FString::Printf(TEXT("%d selected  "), Count)));
                TestTrue(TEXT("Full generic kind retained"), Variant.Contains(Names[KindIndex]));
                TestTrue(TEXT("Existing rounded HP convention"), Variant.Contains(TEXT("HP 124")));
                TestTrue(TEXT("Complete cargo digits"), Variant.Contains(TEXT("Cargo 987654321")));
                TestEqual(TEXT("Rally coordinates only apply to factories"),
                    Variant.Contains(TEXT("Rally -1400,600")), Unit.Kind == EProofUnitKind::Factory);
            }
        }
        if (Unit.Kind == EProofUnitKind::Factory)
        {
            Unit.bHasRallyPoint = false;
            for (const FString& Variant : ProofHUDStatusText::Selection(&Unit, 1))
                TestTrue(TEXT("Unset factory rally remains explicit"), Variant.Contains(TEXT("Rally none")));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofHUDStatusUnavailableTest, "RedalertProof.HUD.MarkerLayout.StatusNoneAndInvalid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofHUDStatusUnavailableTest::RunTest(const FString&)
{
    const TArray<FString> None = ProofHUDStatusText::Selection(nullptr, 0);
    if (!TestEqual(TEXT("Explicit no-selection variants"), None.Num(), 2)) return false;
    TestEqual(TEXT("Full no-selection wording"), None[0], FString(TEXT("Selected: none   Primary: none")));
    TestEqual(TEXT("Compact no-selection wording"), None[1], FString(TEXT("No selection")));
    for (const FString& Variant : None)
    {
        TestFalse(TEXT("No fabricated HP"), Variant.Contains(TEXT("HP")));
        TestFalse(TEXT("No fabricated zero values"), Variant.Contains(TEXT("0")));
    }
    FProofUnitRecord Unit;
    Unit.UnitId = 1; Unit.HitPoints = 100.f;
    TestTrue(TEXT("Invalid selected count has no success variant"), ProofHUDStatusText::Selection(&Unit, 0).IsEmpty());
    TestTrue(TEXT("Negative selected count has no success variant"), ProofHUDStatusText::Selection(&Unit, -1).IsEmpty());
    Unit.HitPoints = std::numeric_limits<float>::quiet_NaN();
    TestTrue(TEXT("NaN health has no success variant"), ProofHUDStatusText::Selection(&Unit, 1).IsEmpty());
    Unit.HitPoints = std::numeric_limits<float>::infinity();
    TestTrue(TEXT("Infinite health has no success variant"), ProofHUDStatusText::Selection(&Unit, 1).IsEmpty());
    Unit.HitPoints = 0.f;
    TestTrue(TEXT("Dead primary has no success variant"), ProofHUDStatusText::Selection(&Unit, 1).IsEmpty());
    Unit.HitPoints = 100.f; Unit.Kind = EProofUnitKind::Factory;
    Unit.bHasRallyPoint = true; Unit.RallyPoint.X = std::numeric_limits<double>::infinity();
    TestTrue(TEXT("Invalid rally has no success variant"), ProofHUDStatusText::Selection(&Unit, 1).IsEmpty());
    Unit.Kind = static_cast<EProofUnitKind>(255);
    TestTrue(TEXT("Unknown kind has no success variant"), ProofHUDStatusText::Selection(&Unit, 1).IsEmpty());
    return true;
}
#endif