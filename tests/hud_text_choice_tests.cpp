#include "Presentation/ProofHUDTextChoice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
static_assert(ProofHUDTextChoice::NoChoice ==
    (std::numeric_limits<std::size_t>::max)(), "NoChoice must be the size_t max sentinel");
static_assert(noexcept(ProofHUDTextChoice::ChooseFirstFitting(nullptr, 0, 0.0)),
    "ChooseFirstFitting must be noexcept");

std::size_t CheckCount = 0;
int FailureCount = 0;

void CheckEqual(const char* Name, std::size_t Actual, std::size_t Expected)
{
    ++CheckCount;
    if (Actual != Expected)
    {
        std::printf("FAIL %s: got %zu, expected %zu\n", Name, Actual, Expected);
        ++FailureCount;
    }
}

void CheckTrue(const char* Name, bool Value)
{
    ++CheckCount;
    if (!Value)
    {
        std::printf("FAIL %s\n", Name);
        ++FailureCount;
    }
}
} // namespace

int main()
{
    using ProofHUDTextChoice::ChooseFirstFitting;
    using ProofHUDTextChoice::NoChoice;

    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    CheckEqual("NoChoice uses the size_t max sentinel", NoChoice,
        (std::numeric_limits<std::size_t>::max)());

    const double PreferenceOrder[] = {2.0, 4.0, 1.0};
    CheckEqual("preference order", ChooseFirstFitting(PreferenceOrder, 3, 3.0), 0);

    const double NarrowerLater[] = {6.0, 5.0, 2.0};
    CheckEqual("first fit before narrower later fit", ChooseFirstFitting(NarrowerLater, 3, 5.0), 1);

    const double InvalidAndOverwide[] = {NaN, -1.0, Infinity, -Infinity, 6.0, 5.25, 2.0};
    CheckEqual("skip invalid and overwide candidates", ChooseFirstFitting(InvalidAndOverwide, 7, 5.0), 6);

    const double AllFail[] = {-2.0, NaN, Infinity, 5.01};
    CheckEqual("all candidates fail", ChooseFirstFitting(AllFail, 4, 5.0), NoChoice);

    const double ExactFit[] = {5.0, 4.0};
    CheckEqual("exact fit is accepted", ChooseFirstFitting(ExactFit, 2, 5.0), 0);

    const double AboveOne[] = {std::nextafter(1.0, Infinity)};
    CheckEqual("nextafter above is rejected", ChooseFirstFitting(AboveOne, 1, 1.0), NoChoice);

    const double BelowOne[] = {std::nextafter(1.0, 0.0)};
    CheckEqual("nextafter below is accepted", ChooseFirstFitting(BelowOne, 1, 1.0), 0);

    const double PositiveZero[] = {0.0, 1.0};
    CheckEqual("positive zero fits zero width", ChooseFirstFitting(PositiveZero, 2, 0.0), 0);

    const double NegativeZeroCandidate[] = {-0.0};
    CheckEqual("negative zero candidate is valid", ChooseFirstFitting(NegativeZeroCandidate, 1, 0.0), 0);

    const double ZeroCandidate[] = {0.0};
    CheckEqual("negative zero available width is valid", ChooseFirstFitting(ZeroCandidate, 1, -0.0), 0);

    const double OneCandidate[] = {1.0};
    CheckEqual("NaN available width is invalid", ChooseFirstFitting(OneCandidate, 1, NaN), NoChoice);
    CheckEqual("positive infinity available width is invalid", ChooseFirstFitting(OneCandidate, 1, Infinity), NoChoice);
    CheckEqual("negative infinity available width is invalid", ChooseFirstFitting(OneCandidate, 1, -Infinity), NoChoice);
    CheckEqual("negative available width is invalid", ChooseFirstFitting(OneCandidate, 1, -1.0), NoChoice);

    const double InvalidCandidates[] = {NaN, Infinity, -Infinity, -0.001, 4.0};
    CheckEqual("invalid candidate values are skipped", ChooseFirstFitting(InvalidCandidates, 5, 4.0), 4);

    CheckEqual("null pointer with positive count", ChooseFirstFitting(nullptr, 1, 1.0), NoChoice);
    CheckEqual("nonnull pointer with empty range", ChooseFirstFitting(OneCandidate, 0, 1.0), NoChoice);
    CheckEqual("null pointer with empty range", ChooseFirstFitting(nullptr, 0, 1.0), NoChoice);

    const double Max = (std::numeric_limits<double>::max)();
    const double MaxFinite[] = {Max, std::nextafter(Max, Infinity)};
    CheckEqual("maximum finite exact fit", ChooseFirstFitting(MaxFinite, 2, Max), 0);

    const double Deterministic[] = {8.0, NaN, 3.0, 2.0};
    bool Stable = true;
    for (int Repeat = 0; Repeat < 100; ++Repeat)
    {
        if (ChooseFirstFitting(Deterministic, 4, 4.0) != 2)
        {
            Stable = false;
            break;
        }
    }
    CheckTrue("repeated calls are deterministic", Stable);

    const double Unchanged[] = {5.0, 2.0, 4.0, 8.0};
    unsigned char Before[sizeof(Unchanged)];
    std::memcpy(Before, Unchanged, sizeof(Unchanged));
    const std::size_t UnchangedResult = ChooseFirstFitting(Unchanged, 4, 3.0);
    CheckEqual("unchanged-input selection result", UnchangedResult, 1);
    CheckTrue("candidate storage remains unchanged",
        std::memcmp(Before, Unchanged, sizeof(Unchanged)) == 0);

    if (FailureCount != 0)
    {
        std::printf("FAIL %d of %zu checks\n", FailureCount, CheckCount);
        return 1;
    }

    std::printf("PASS %zu checks (active with or without NDEBUG)\n", CheckCount);
    return 0;
}
