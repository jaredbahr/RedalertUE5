#include "../unreal/Source/RedalertProof/Presentation/ProofBoxSelection.h"

#include <array>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

namespace
{
using namespace ProofBoxSelection;
int Checks = 0, Failures = 0;

void Expect(const std::string& Name, const Result& Actual, bool Valid,
    const std::vector<std::int32_t>& Ids)
{
    ++Checks;
    if (Actual.Valid != Valid || Actual.UnitIds != Ids)
    {
        ++Failures;
        std::cerr << "FAIL " << Name << ": expected valid=" << Valid << " ids=";
        for (auto Id : Ids) std::cerr << Id << ',';
        std::cerr << "; actual valid=" << Actual.Valid << " ids=";
        for (auto Id : Actual.UnitIds) std::cerr << Id << ',';
        std::cerr << '\n';
    }
}

void Check(const std::string& Name, bool Condition)
{
    ++Checks;
    if (!Condition) { ++Failures; std::cerr << "FAIL " << Name << '\n'; }
}

void ClosedEdgesAndDirections()
{
    const double BelowLeft = std::nextafter(10.0, 0.0);
    const double AboveRight = std::nextafter(30.0, 100.0);
    const double AboveTop = std::nextafter(20.0, 0.0);
    const double BelowBottom = std::nextafter(40.0, 100.0);
    const Candidate Candidates[] = {
        {1, {10, 20}, true}, {2, {30, 20}, true},
        {3, {10, 40}, true}, {4, {30, 40}, true},
        {5, {10, 30}, true}, {6, {30, 30}, true},
        {7, {20, 20}, true}, {8, {20, 40}, true}, {9, {20, 30}, true},
        {10, {BelowLeft, 30}, true}, {11, {AboveRight, 30}, true},
        {12, {20, AboveTop}, true}, {13, {20, BelowBottom}, true}
    };
    const Point Starts[] = {{10, 20}, {30, 20}, {10, 40}, {30, 40}};
    const Point Ends[] = {{30, 40}, {10, 40}, {30, 20}, {10, 20}};
    for (std::size_t I = 0; I < 4; ++I)
        Expect("closed_edges_direction_" + std::to_string(I),
            Build(Candidates, std::size(Candidates), Starts[I], Ends[I], 100, 80, 20),
            true, {1, 2, 3, 4, 5, 6, 7, 8, 9});

    const Candidate Points[] = {
        {1, {20, 30}, true}, {2, {std::nextafter(20.0, 0.0), 30}, true},
        {3, {std::nextafter(20.0, 100.0), 30}, true},
        {4, {20, std::nextafter(30.0, 0.0)}, true},
        {5, {20, std::nextafter(30.0, 100.0)}, true}
    };
    Expect("zero_area_exact_center", Build(Points, std::size(Points), {20, 30}, {20, 30}, 100, 80, 10), true, {1});
    Expect("zero_width_closed_line", Build(Points, std::size(Points), {20, 0}, {20, 60}, 100, 80, 10), true, {1, 4, 5});
    Expect("zero_height_closed_line", Build(Points, std::size(Points), {0, 30}, {60, 30}, 100, 80, 10), true, {1, 2, 3});
    Expect("valid_no_hits", Build(Points, std::size(Points), {0, 0}, {1, 1}, 100, 80, 10), true, {});
}

void ViewportAndEligibility()
{
    const double LastX = std::nextafter(100.0, 0.0);
    const double LastY = std::nextafter(80.0, 0.0);
    const double Nan = std::numeric_limits<double>::quiet_NaN();
    const double Inf = std::numeric_limits<double>::infinity();
    const Candidate Candidates[] = {
        {1, {0, 0}, true}, {2, {LastX, LastY}, true}, {3, {-0.0, -0.0}, true},
        {4, {100, 40}, true}, {5, {50, 80}, true},
        {6, {-1, 40}, true}, {7, {50, -1}, true},
        {8, {Nan, 40}, true}, {9, {50, Nan}, true},
        {10, {Inf, 40}, true}, {11, {50, Inf}, true},
        {12, {-Inf, 40}, true}, {13, {50, -Inf}, true},
        {14, {101, 40}, true}, {15, {50, 81}, true}
    };
    Expect("viewport_half_open_and_finite_centers",
        Build(Candidates, std::size(Candidates), {0, 0}, {LastX, LastY}, 100, 80, 30), true, {1, 2, 3});

    const Candidate Eligibility[] = {
        {1, {5, 5}, false}, {2, {5, 5}, true}, {0, {5, 5}, true},
        {-1, {5, 5}, true}, {std::numeric_limits<std::int32_t>::min(), {5, 5}, true},
        {std::numeric_limits<std::int32_t>::max(), {5, 5}, true},
        {3, {5, 5}, false}, {3, {5, 5}, true},
        {4, {Nan, 5}, true}, {4, {5, 5}, true}
    };
    Expect("eligibility_and_positive_ids",
        Build(Eligibility, std::size(Eligibility), {0, 0}, {9, 9}, 10, 10, 30),
        true, {2, 3, 4, std::numeric_limits<std::int32_t>::max()});
    Expect("ineligible_does_not_consume_cap",
        Build(Eligibility, std::size(Eligibility), {0, 0}, {9, 9}, 10, 10, 1), true, {2});
}

void RequestValidation()
{
    const Candidate One{1, {0, 0}, true};
    const double Nan = std::numeric_limits<double>::quiet_NaN();
    const double Inf = std::numeric_limits<double>::infinity();
    const double InvalidDimensions[] = {0, -0.0, -1, Nan, Inf, -Inf};
    for (std::size_t I = 0; I < std::size(InvalidDimensions); ++I)
    {
        Expect("invalid_width_" + std::to_string(I), Build(&One, 1, {0, 0}, {1, 1}, InvalidDimensions[I], 80, 1), false, {});
        Expect("invalid_height_" + std::to_string(I), Build(&One, 1, {0, 0}, {1, 1}, 100, InvalidDimensions[I], 1), false, {});
    }
    const Point InvalidPoints[] = {
        {-1, 0}, {0, -1}, {-std::numeric_limits<double>::denorm_min(), 0},
        {0, -std::numeric_limits<double>::denorm_min()}, {100, 0}, {0, 80},
        {std::nextafter(100.0, Inf), 0}, {0, std::nextafter(80.0, Inf)},
        {Nan, 0}, {0, Nan}, {Inf, 0}, {0, Inf}, {-Inf, 0}, {0, -Inf}
    };
    for (std::size_t I = 0; I < std::size(InvalidPoints); ++I)
    {
        Expect("invalid_start_" + std::to_string(I), Build(&One, 1, InvalidPoints[I], {1, 1}, 100, 80, 1), false, {});
        Expect("invalid_end_" + std::to_string(I), Build(&One, 1, {0, 0}, InvalidPoints[I], 100, 80, 1), false, {});
    }
    Expect("null_nonempty", Build(nullptr, 1, {0, 0}, {1, 1}, 100, 80, 1), false, {});
    Expect("null_empty", Build(nullptr, 0, {0, 0}, {1, 1}, 100, 80, 1), true, {});
    Expect("nonnull_empty", Build(&One, 0, {0, 0}, {1, 1}, 100, 80, 1), true, {});
    Expect("zero_limit", Build(&One, 1, {0, 0}, {1, 1}, 100, 80, 0), false, {});
    Expect("empty_still_validates_limit", Build(nullptr, 0, {0, 0}, {1, 1}, 100, 80, 0), false, {});
    Expect("empty_still_validates_viewport", Build(nullptr, 0, {0, 0}, {1, 1}, 0, 80, 1), false, {});
    Expect("empty_still_validates_end", Build(nullptr, 0, {0, 0}, {100, 1}, 100, 80, 1), false, {});
    Expect("maximum_limit", Build(&One, 1, {0, 0}, {1, 1}, 100, 80,
        std::numeric_limits<std::size_t>::max()), true, {1});

    // No rectangle subtraction or area multiplication is needed even at limits.
    const double Max = std::numeric_limits<double>::max();
    const double Last = std::nextafter(Max, 0.0);
    const Candidate Extreme[] = {{1, {Last, Last}, true}, {2, {Max, Last}, true}};
    Expect("maximum_finite_viewport", Build(Extreme, 2, {0, 0}, {Last, Last}, Max, Max, 2), true, {1});
    const double MinNormal = std::numeric_limits<double>::min();
    Expect("small_positive_viewport", Build(&One, 1, {0, 0}, {0, 0}, MinNormal, MinNormal, 1), true, {1});
    Expect("signed_zero_endpoints", Build(&One, 1, {-0.0, 0}, {0, -0.0}, 1, 1, 1), true, {1});
}

void DeterminismAndStorage()
{
    const Candidate Unsorted[] = {
        {9, {5, 5}, true}, {4, {5, 5}, true}, {1, {5, 5}, true},
        {4, {5, 5}, true}, {2, {5, 5}, true}
    };
    Expect("sort_before_cap", Build(Unsorted, std::size(Unsorted), {0, 0}, {9, 9}, 10, 10, 2), true, {1, 2});
    const Candidate Duplicates[] = {
        {1, {5, 5}, true}, {1, {5, 5}, true}, {2, {5, 5}, true}, {3, {5, 5}, true}
    };
    Expect("deduplicate_before_cap", Build(Duplicates, std::size(Duplicates), {0, 0}, {9, 9}, 10, 10, 3), true, {1, 2, 3});

    const Candidate Input[] = {{9, {5, 5}, true}, {2, {5, 5}, true}, {2, {6, 6}, true}, {1, {5, 5}, true}};
    std::array<std::size_t, 4> Order{{0, 1, 2, 3}};
    std::size_t Permutation = 0;
    do
    {
        Candidate Shuffled[4];
        for (std::size_t I = 0; I < 4; ++I) Shuffled[I] = Input[Order[I]];
        // Preserve the actual representation, including NaN/sign bits or padding.
        unsigned char Before[sizeof(Shuffled)];
        std::memcpy(Before, Shuffled, sizeof(Shuffled));
        for (std::size_t Limit = 1; Limit <= 3; ++Limit)
        {
            std::vector<std::int32_t> Expected{1, 2, 9};
            Expected.resize(Limit);
            Expect("permutation_" + std::to_string(Permutation) + "_limit_" + std::to_string(Limit),
                Build(Shuffled, 4, {0, 0}, {9, 9}, 10, 10, Limit), true, Expected);
        }
        Check("input_unchanged_permutation_" + std::to_string(Permutation),
            std::memcmp(Before, Shuffled, sizeof(Shuffled)) == 0);
        ++Permutation;
    } while (std::next_permutation(Order.begin(), Order.end()));
    Check("all_24_input_permutations", Permutation == 24);

    Candidate Repeated[] = {
        {4, {-0.0, 0}, true}, {2, {5, 5}, true},
        {1, {std::numeric_limits<double>::quiet_NaN(), 0}, true}, {3, {5, 5}, false}
    };
    unsigned char Before[sizeof(Repeated)];
    std::memcpy(Before, Repeated, sizeof(Repeated));
    for (int I = 0; I < 8; ++I)
        Expect("repeated_call_" + std::to_string(I), Build(Repeated, std::size(Repeated), {0, 0}, {9, 9}, 10, 10, 3), true, {2, 4});
    Check("input_representation_unchanged", std::memcmp(Before, Repeated, sizeof(Repeated)) == 0);
}
}

int main()
{
    ClosedEdgesAndDirections();
    ViewportAndEligibility();
    RequestValidation();
    DeterminismAndStorage();
    std::cout << Checks << " checks, " << Failures << " failures (actual ProofBoxSelection header)\n";
    return Failures == 0 ? 0 : 1;
}
