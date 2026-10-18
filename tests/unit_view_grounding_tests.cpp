#include "Presentation/ProofUnitViewGrounding.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
using Vec3 = std::array<double, 3>;
using Matrix3 = std::array<Vec3, 3>;

struct Box
{
    Vec3 Min;
    Vec3 Max;
};

struct Transform
{
    Matrix3 Linear;
    Vec3 Translation;
};

constexpr Matrix3 Identity = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
constexpr Box Cube = {{-50.0, -50.0, -50.0}, {50.0, 50.0, 50.0}};
constexpr Box GroundOrigin = {{-130.0, -70.0, 0.0}, {170.0, 80.0, 100.0}};
constexpr Box AboveOrigin = {{10.0, -45.0, 20.0}, {80.0, 15.0, 110.0}};
constexpr Box ArbitraryPivot = {{-10.0, 20.0, -30.0}, {40.0, 60.0, 70.0}};

void Require(bool Condition, const std::string& Label)
{
    if (!Condition) throw std::runtime_error(Label);
}

void Near(double Actual, double Expected, const std::string& Label)
{
    const double Tolerance = 1.e-9 * (1.0 + std::abs(Expected));
    Require(std::isfinite(Actual) && std::abs(Actual - Expected) <= Tolerance,
        Label + ": expected " + std::to_string(Expected) + ", got " + std::to_string(Actual));
}

Matrix3 Multiply(const Matrix3& Left, const Matrix3& Right)
{
    Matrix3 Result{};
    for (std::size_t Row = 0; Row < 3; ++Row)
        for (std::size_t Col = 0; Col < 3; ++Col)
            for (std::size_t Inner = 0; Inner < 3; ++Inner)
                Result[Row][Col] += Left[Row][Inner] * Right[Inner][Col];
    return Result;
}

Matrix3 Scaled(const Matrix3& Rotation, const Vec3& Scale)
{
    Matrix3 Result = Rotation;
    for (std::size_t Row = 0; Row < 3; ++Row)
        for (std::size_t Col = 0; Col < 3; ++Col)
            Result[Row][Col] *= Scale[Col];
    return Result;
}

// Independent reference matrices, not an emulation of UE's quaternion API.
// This covers rotations about each axis and mixed orientations in portable C++.
Matrix3 Rotation(double PitchDegrees, double YawDegrees, double RollDegrees)
{
    constexpr double Radians = 3.14159265358979323846 / 180.0;
    const double P = PitchDegrees * Radians;
    const double Y = YawDegrees * Radians;
    const double R = RollDegrees * Radians;
    const Matrix3 X = {{{1.0, 0.0, 0.0}, {0.0, std::cos(R), -std::sin(R)}, {0.0, std::sin(R), std::cos(R)}}};
    const Matrix3 YAxis = {{{std::cos(P), 0.0, std::sin(P)}, {0.0, 1.0, 0.0}, {-std::sin(P), 0.0, std::cos(P)}}};
    const Matrix3 Z = {{{std::cos(Y), -std::sin(Y), 0.0}, {std::sin(Y), std::cos(Y), 0.0}, {0.0, 0.0, 1.0}}};
    return Multiply(Multiply(Z, YAxis), X);
}

Vec3 TransformPosition(const Transform& World, const Vec3& Local)
{
    Vec3 Result = World.Translation;
    for (std::size_t Row = 0; Row < 3; ++Row)
        for (std::size_t Col = 0; Col < 3; ++Col)
            Result[Row] += World.Linear[Row][Col] * Local[Col];
    return Result;
}

// The oracle transforms all eight corners and rebuilds the full world AABB.
// It does not use the helper's sum-of-axis-minima formula.
Box WorldBounds(const Box& Local, const Transform& World)
{
    const double Infinity = std::numeric_limits<double>::infinity();
    Box Result = {{Infinity, Infinity, Infinity}, {-Infinity, -Infinity, -Infinity}};
    for (unsigned Corner = 0; Corner < 8; ++Corner)
    {
        Vec3 LocalCorner{};
        for (std::size_t Axis = 0; Axis < 3; ++Axis)
            LocalCorner[Axis] = (Corner & (1u << Axis)) != 0 ? Local.Max[Axis] : Local.Min[Axis];
        const Vec3 WorldCorner = TransformPosition(World, LocalCorner);
        for (std::size_t Axis = 0; Axis < 3; ++Axis)
        {
            if (WorldCorner[Axis] < Result.Min[Axis]) Result.Min[Axis] = WorldCorner[Axis];
            if (WorldCorner[Axis] > Result.Max[Axis]) Result.Max[Axis] = WorldCorner[Axis];
        }
    }
    return Result;
}

double OffsetFor(const Box& Local, const Transform& Source)
{
    // In a column-vector matrix these entries are the world Z components of
    // the scaled local X/Y/Z axes. Source translation cannot enter this call.
    return FProofUnitViewGrounding::VerticalOffset(Local.Min, Local.Max,
        {Source.Linear[2][0], Source.Linear[2][1], Source.Linear[2][2]});
}

Transform Place(const Box& Local, const Transform& Source, const Vec3& ModelPosition)
{
    Transform Result = Source;
    Result.Translation = ModelPosition;
    Result.Translation[2] += OffsetFor(Local, Source);
    return Result;
}

double CheckPlacement(const std::string& Label, const Box& Local,
    const Matrix3& Linear, const Vec3& ModelPosition)
{
    const Box SavedLocal = Local;
    const Vec3 SavedModel = ModelPosition;
    Transform Source = {Linear, {917.0, -431.0, 2850.0}};
    const Transform SavedSource = Source;
    const double Offset = OffsetFor(Local, Source);
    const Box RelativeBounds = WorldBounds(Local, {Linear, {0.0, 0.0, 0.0}});
    Near(Offset, -RelativeBounds.Min[2], Label + " / eight-corner relative offset");

    Transform Placed = Place(Local, Source, ModelPosition);
    const Box PlacedBounds = WorldBounds(Local, Placed);
    Near(PlacedBounds.Min[2], ModelPosition[2], Label + " / world bottom touches ground");
    Require(Placed.Translation[0] == ModelPosition[0] && Placed.Translation[1] == ModelPosition[1],
        Label + " / XY must be authoritative");
    Require(Placed.Linear == Linear, Label + " / rotation and signed scale must be preserved");
    Require(Local.Min == SavedLocal.Min && Local.Max == SavedLocal.Max && ModelPosition == SavedModel
        && Source.Linear == SavedSource.Linear && Source.Translation == SavedSource.Translation,
        Label + " / inputs must remain unchanged");

    Source.Translation = {-6000.0, 4100.0, -1200.0};
    Near(OffsetFor(Local, Source), Offset, Label + " / changed source translation");
    Require(Place(Local, Source, ModelPosition).Translation == Placed.Translation,
        Label + " / prior translation cannot affect placement");
    const Vec3 ExpectedPlacement = Placed.Translation;
    for (int Tick = 0; Tick < 4; ++Tick)
    {
        Placed = Place(Local, Placed, ModelPosition);
        Require(Placed.Translation == ExpectedPlacement, Label + " / repeated placement cannot accumulate lift");
    }
    return Offset;
}

void KnownOffsets()
{
    struct KnownCase
    {
        const char* Name;
        Box Local;
        Vec3 Scale;
        double Expected;
    };
    const KnownCase Cases[] = {
        {"Measured Harvester centered cube", Cube, {1.0, 0.7, 0.7}, 35.0},
        {"Non-skirmish fallback cube", Cube, {0.8, 0.8, 0.6}, 30.0},
        {"Unscaled fallback cube", Cube, {1.0, 1.0, 1.0}, 50.0},
        {"Ground-origin proxy", GroundOrigin, {1.2, 0.8, 1.7}, 0.0},
        {"Above-origin mesh needs lowering", AboveOrigin, {1.0, 2.0, 0.5}, -10.0},
        {"Negative Z scale and asymmetric pivot", ArbitraryPivot, {2.0, 3.0, -0.5}, 35.0},
        {"Reflected ground-origin proxy", GroundOrigin, {1.0, 1.0, -0.5}, 50.0},
        {"Collapsed vertical scale", ArbitraryPivot, {2.0, 0.7, 0.0}, 0.0}
    };
    for (const KnownCase& Case : Cases)
    {
        const double Actual = CheckPlacement(Case.Name, Case.Local,
            Scaled(Identity, Case.Scale), {211.0, -107.0, 123.0});
        Near(Actual, Case.Expected, std::string(Case.Name) + " / explicit offset");
    }

    // Exact quarter turns isolate the contribution of local X and Y. A helper
    // using only local Z/scale.Z cannot pass these independent known values.
    const Matrix3 YQuarterTurn = {{{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}}};
    Near(CheckPlacement("Rotated X supplies world Z", ArbitraryPivot,
        Scaled(YQuarterTurn, {2.0, 3.0, 0.5}), {0.0, 0.0, 0.0}), 80.0, "Known rotated X offset");
    const Matrix3 XQuarterTurn = {{{1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}}};
    Near(CheckPlacement("Rotated Y supplies world Z", ArbitraryPivot,
        Scaled(XQuarterTurn, {2.0, 3.0, 0.5}), {0.0, 0.0, -23.5}), -60.0, "Known rotated Y offset");
}

void DeterministicMatrix()
{
    const std::array<Box, 4> Boxes = {Cube, GroundOrigin, AboveOrigin, ArbitraryPivot};
    const std::array<Matrix3, 6> Rotations = {
        Identity, Rotation(0.0, 37.0, 0.0), Rotation(90.0, 0.0, 0.0),
        Rotation(0.0, 0.0, -90.0), Rotation(27.0, -68.0, 39.0), Rotation(-119.0, 151.0, -73.0)};
    const std::array<Vec3, 5> Scales = {{{1.0, 1.0, 1.0}, {2.0, 0.3, 1.7},
        {-2.0, 0.3, 1.7}, {2.0, -0.3, -1.7}, {-2.0, -0.3, -1.7}}};
    for (std::size_t B = 0; B < Boxes.size(); ++B)
        for (std::size_t R = 0; R < Rotations.size(); ++R)
            for (std::size_t S = 0; S < Scales.size(); ++S)
                CheckPlacement("Matrix box=" + std::to_string(B) + " rotation=" + std::to_string(R)
                    + " scale=" + std::to_string(S), Boxes[B], Scaled(Rotations[R], Scales[S]),
                    {211.0, -107.0, -23.5});
}
}

int main()
{
    try
    {
        KnownOffsets();
        DeterministicMatrix();
        std::cout << "PASS: 10 explicit-offset cases and 120 bounds/rotation/signed-scale combinations.\n"
                  << "Production helper checked against independent eight-corner world bounds; "
                  << "translation independence and repeated placement checked per case.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return EXIT_FAILURE;
    }
}
