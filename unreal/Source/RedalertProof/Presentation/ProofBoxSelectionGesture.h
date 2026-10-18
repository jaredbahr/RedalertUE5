#pragma once

#include "ProofBoxSelection.h"
#include <cmath>
#include <cstdint>

// Viewport-coordinate gesture only: no engine input, selection, or commands.
namespace ProofBoxSelectionGesture
{
struct Context
{
    std::int32_t Owner = -1;
    std::int32_t Epoch = -1;
    std::int32_t Phase = -1;
    double Width = 0.0;
    double Height = 0.0;
    std::uintptr_t Controller = 0;
    std::uintptr_t Viewport = 0;

    bool operator==(const Context& Other) const
    {
        return Owner == Other.Owner && Epoch == Other.Epoch && Phase == Other.Phase
            && Width == Other.Width && Height == Other.Height
            && Controller == Other.Controller && Viewport == Other.Viewport;
    }
};

enum class Kind { None, Click, Box };
struct Release
{
    Kind Action = Kind::None;
    Context PressContext;
    ProofBoxSelection::Point Start = {0.0, 0.0};
    ProofBoxSelection::Point End = {0.0, 0.0};
    bool Additive = false;
    std::int32_t ClickTarget = -1;
};

class State
{
public:
    // Four actual viewport pixels is a prototype interaction default.
    static constexpr double DragThreshold = 4.0;

    bool Begin(const Context& Current, ProofBoxSelection::Point Point,
        bool Additive, std::int32_t Target, bool Valid)
    {
        if (Down) return false; // Includes canceled gestures awaiting button-up.
        Down = true;
        if (!Valid || !Contains(Current, Point)) return false;
        PressContext = Current;
        Start = End = Point;
        Shift = Additive;
        ClickTarget = Target;
        Armed = true;
        Dragged = false;
        return true;
    }

    void Sample(const Context& Current, ProofBoxSelection::Point Point, bool Valid)
    {
        if (!Armed) return;
        if (!Valid || !(Current == PressContext) || !Contains(Current, Point))
        {
            Cancel();
            return;
        }
        End = Point;
        Dragged = Dragged || std::hypot(End.X - Start.X, End.Y - Start.Y) >= DragThreshold;
    }

    Release EndPress(const Context& Current, ProofBoxSelection::Point Point, bool Valid)
    {
        Sample(Current, Point, Valid); // The release position participates in the threshold.
        Release Result;
        if (Armed)
        {
            Result = {Dragged ? Kind::Box : Kind::Click, PressContext, Start, End, Shift, ClickTarget};
        }
        Armed = Down = Dragged = false;
        return Result; // Unmatched and duplicate release are inert.
    }

    void Cancel() { Armed = Dragged = false; }
    void ObserveButton(bool IsDown)
    {
        if (!IsDown)
        {
            // A lost release never synthesizes a click. Fresh input can rearm.
            Cancel();
            Down = false;
        }
    }
    bool IsArmed() const { return Armed; }
    bool IsDragging() const { return Armed && Dragged; }
    bool Matches(const Context& Current) const { return Armed && Current == PressContext; }
    ProofBoxSelection::Point GetStart() const { return Start; }
    ProofBoxSelection::Point GetEnd() const { return End; }

private:
    static bool Contains(const Context& Current, ProofBoxSelection::Point Point)
    {
        return Current.Owner >= 0 && Current.Epoch > 0 && Current.Controller != 0 && Current.Viewport != 0
            && std::isfinite(Current.Width) && std::isfinite(Current.Height)
            && Current.Width > 0.0 && Current.Height > 0.0
            && std::isfinite(Point.X) && std::isfinite(Point.Y)
            && Point.X >= 0.0 && Point.Y >= 0.0 && Point.X < Current.Width && Point.Y < Current.Height;
    }
    Context PressContext;
    ProofBoxSelection::Point Start = {0.0, 0.0};
    ProofBoxSelection::Point End = {0.0, 0.0};
    bool Down = false;
    bool Armed = false;
    bool Dragged = false;
    bool Shift = false;
    std::int32_t ClickTarget = -1;
};
}
