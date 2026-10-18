// Portable mathematical coverage of the exact production helper. This does not
// emulate native Unreal transforms or prove the imported mesh's semantic axis.
#include "Presentation/ProofUnitViewFacing.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#define PROOF_FACING_HAS_MXCSR 1
#else
#define PROOF_FACING_HAS_MXCSR 0
#endif

#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#endif

namespace
{
using V = FProofUnitViewFacing::Vector;
int Checks = 0;
constexpr double Pi = 3.14159265358979323846;
void Require(bool Condition, const std::string& Why)
{
    ++Checks;
    if (!Condition) throw std::runtime_error(Why);
}
void CheckAlignment(V Forward, V Direction, const std::string& Label = "ordinary_alignment")
{
    const V SavedForward = Forward, SavedDirection = Direction;
    double Delta = 123.0;
    Require(FProofUnitViewFacing::TryYawDelta(Forward, Direction, Delta), Label + ": valid finite vectors rejected");
    Require(std::isfinite(Delta) && std::abs(Delta) <= Pi, "delta finite and bounded");
    // Independent long-double trig/hypot oracle; no helper normalization reused.
    const long double FL = std::hypot(static_cast<long double>(Forward.X), static_cast<long double>(Forward.Y));
    const long double DL = std::hypot(static_cast<long double>(Direction.X), static_cast<long double>(Direction.Y));
    const long double X = Forward.X / FL, Y = Forward.Y / FL;
    const long double C = std::cos(static_cast<long double>(Delta)), S = std::sin(static_cast<long double>(Delta));
    Require(std::abs(C * X - S * Y - Direction.X / DL) < 1.e-14L
        && std::abs(S * X + C * Y - Direction.Y / DL) < 1.e-14L, "independent rotated forward alignment");
    Require(Forward.X == SavedForward.X && Forward.Y == SavedForward.Y && Forward.Z == SavedForward.Z
        && Direction.X == SavedDirection.X && Direction.Y == SavedDirection.Y && Direction.Z == SavedDirection.Z,
        "helper mutated inputs");
}
void CheckRejected(V Forward, V Direction)
{
    double Delta = 123.0;
    Require(!FProofUnitViewFacing::TryYawDelta(Forward, Direction, Delta), "invalid vector accepted");
    Require(Delta == 0.0 && !std::signbit(Delta), "failure must clear output to positive zero");
}

// Only this isolated portable executable may change its own MXCSR. The native
// automation body observes the engine's existing mode without modifying it.
struct ScopedFpMode
{
#if PROOF_FACING_HAS_MXCSR
    const unsigned Saved = _mm_getcsr();
    ~ScopedFpMode() { _mm_setcsr(Saved); }
    void Restore() { _mm_setcsr(Saved); }
    bool Restored() const { return _mm_getcsr() == Saved; }
#else
    void Restore() {}
    bool Restored() const { return true; }
#endif
    void Set(const std::string& Mode)
    {
        if (Mode == "native") return;
#if PROOF_FACING_HAS_MXCSR
        unsigned Flags;
        if (Mode == "gradual") Flags = 0;
        else if (Mode == "ftz") Flags = 0x8000u;
        else if (Mode == "daz") Flags = 0x40u;
        else if (Mode == "ftz-daz") Flags = 0x8040u;
        else throw std::runtime_error("unknown FP mode");
        _mm_setcsr((Saved & ~0x8040u) | Flags);
#else
        throw std::runtime_error("explicit FP controls require an x86 MXCSR target");
#endif
    }
};
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--fp-modes-supported")
            return PROOF_FACING_HAS_MXCSR ? 0 : 1;
        const std::string Mode = argc == 2 ? argv[1] : "native";
        if (argc > 2) throw std::runtime_error("usage: test [native|gradual|ftz|daz|ftz-daz]");
        ScopedFpMode FpMode;
        FpMode.Set(Mode);
        std::cout << "FP mode: " << Mode << '\n';
        double Delta = 123.0;
        Require(FProofUnitViewFacing::TryYawDelta({0, 1, 0}, {1, 0, 0}, Delta)
            && std::abs(Delta + Pi / 2) < 1.e-15, "+Y front to +X cardinal sign");
        Require(FProofUnitViewFacing::TryYawDelta({0, 1, 0}, {-1, 0, 0}, Delta)
            && std::abs(Delta - Pi / 2) < 1.e-15, "+Y front to -X cardinal sign");
        Require(FProofUnitViewFacing::TryYawDelta({0, 1, 0}, {0, -1, 0}, Delta)
            && std::abs(std::abs(Delta) - Pi) < 1.e-15, "half turn");
        Require(FProofUnitViewFacing::TryYawDelta({1, 0, 0}, {1, -0.0, 0}, Delta)
            && Delta == 0 && !std::signbit(Delta), "aligned signed zero canonicalization");
        const std::array<V, 8> Directions{{{1,0,0},{0,1,0},{-1,0,0},{0,-1,0},
            {.6,.8,0},{-.6,.8,0},{-.6,-.8,0},{.6,-.8,0}}};
        for (int Angle = -180; Angle <= 180; Angle += 3)
        for (double Scale : {-2.0, -.3, .3, 2.0})
        for (V Direction : Directions)
        {
            const double Radians = Angle * Pi / 180.0;
            CheckAlignment({Scale * std::cos(Radians), Scale * std::sin(Radians), 123.0}, Direction);
        }
        const double Tiny = std::numeric_limits<double>::denorm_min();
        const double Huge = std::numeric_limits<double>::max();
        volatile double RuntimeTiny = Tiny;
        if (RuntimeTiny != 0.0)
        {
            CheckAlignment({0,Tiny,0},{Tiny,0,0}, "subnormal_cardinal");
            CheckAlignment({Tiny,Tiny,0},{-Tiny,Tiny,0}, "subnormal_diagonal");
        }
        else
        {
            CheckRejected({0,Tiny,0},{Tiny,0,0});
            CheckRejected({Tiny,Tiny,0},{-Tiny,Tiny,0});
        }
        const double MinimumNormal = std::numeric_limits<double>::min();
        CheckAlignment({MinimumNormal,MinimumNormal,0},{-MinimumNormal,MinimumNormal,0}, "minimum_normal_diagonal");
        CheckAlignment({MinimumNormal,2*MinimumNormal,0},{-2*MinimumNormal,MinimumNormal,0}, "minimum_normal_ratio");
        CheckAlignment({1.e-200,2.e-200,0},{-2.e-200,1.e-200,0}, "small_normal_square_underflow");
        CheckAlignment({Huge,Huge,Huge},{-Huge,Huge,0});
        CheckAlignment({Huge,Tiny,0},{Tiny,-Huge,0});
        CheckAlignment({-Tiny,Huge,0},{Huge,Tiny,0});
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        const double Inf = std::numeric_limits<double>::infinity();
        for (V Direction : {V{0,0,0}, V{0,-0.0,0}, V{1,0,1}, V{1,0,NaN},
            V{1,0,Inf}, V{NaN,1,0}, V{1,Inf,0}, V{-Inf,1,0}}) CheckRejected({0,1,0},Direction);
        for (V Forward : {V{0,0,0}, V{0,0,1}, V{NaN,1,0}, V{0,Inf,0},
            V{0,1,NaN}, V{0,1,Inf}}) CheckRejected(Forward,{1,0,0});
        // Persistent stationary metadata can be reapplied without position or
        // camera history. This uses ordinary planar rotation, not Unreal.
        V Forward{0,1,3};
        for (int Tick = 0; Tick < 1000; ++Tick)
        {
            Require(FProofUnitViewFacing::TryYawDelta(Forward, {.6,-.8,0}, Delta), "stationary repeat valid");
            const double X = Forward.X;
            Forward.X = std::cos(Delta) * X - std::sin(Delta) * Forward.Y;
            Forward.Y = std::sin(Delta) * X + std::cos(Delta) * Forward.Y;
            Require(std::abs(Forward.X - .6) < 1.e-12 && std::abs(Forward.Y + .8) < 1.e-12
                && Forward.Z == 3, "stationary repetition preserves alignment and Z");
        }
        FpMode.Restore();
        Require(FpMode.Restored(), "portable process restores its original MXCSR");
        std::cout << "PASS: unit view facing helper: " << Checks << " checks\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: unit view facing helper: " << Error.what() << '\n';
        return 1;
    }
}
#if defined(_MSC_VER)
#pragma float_control(pop)
#endif
