#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

/** Presentation/input policy only. Host command validation remains authoritative. */
namespace ProofCommandCard
{
enum class Action { None, BuildRefinery, BuildFactory, ProduceCombat, Stop };
inline constexpr std::array<Action, 4> Actions = {
    Action::BuildRefinery, Action::BuildFactory, Action::ProduceCombat, Action::Stop
};

struct Rect
{
    double X = 0.0, Y = 0.0, Width = 0.0, Height = 0.0;
    bool Contains(double PointX, double PointY) const noexcept
    {
        return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Width) && std::isfinite(Height)
            && Width > 0.0 && Height > 0.0 && std::isfinite(X + Width) && std::isfinite(Y + Height)
            && std::isfinite(PointX) && std::isfinite(PointY)
            && PointX >= X && PointY >= Y && PointX < X + Width && PointY < Y + Height;
    }
};

/** Shared by drawing, hit testing, camera exclusion and world-label reservations. */
struct Layout
{
    bool Valid = false;
    double Scale = 1.0;
    double ViewportWidth = 0.0, ViewportHeight = 0.0;
    Rect Panel;
    std::array<Rect, 4> Buttons{};

    Layout(double Width, double Height) noexcept : ViewportWidth(Width), ViewportHeight(Height)
    {
        // Below this size retain the existing hotkeys instead of drawing unusable controls.
        if (!std::isfinite(Width) || !std::isfinite(Height) || Width < 640.0 || Height < 360.0
            || Width > (std::numeric_limits<int>::max)() || Height > (std::numeric_limits<int>::max)()) return;
        Scale = Width / 1280.0;
        if (Scale < 0.72) Scale = 0.72;
        if (Scale > 1.0) Scale = 1.0;
        Panel = {Width - 316.0 * Scale, Height - 190.0 * Scale, 304.0 * Scale, 178.0 * Scale};
        for (std::size_t Index = 0; Index < Buttons.size(); ++Index)
        {
            Buttons[Index] = {Panel.X + (12.0 + static_cast<double>(Index % 2) * 144.0) * Scale,
                Panel.Y + (34.0 + static_cast<double>(Index / 2) * 58.0) * Scale, 136.0 * Scale, 50.0 * Scale};
        }
        Valid = true;
    }

    bool PointerValid(double X, double Y) const noexcept
    {
        return Valid && std::isfinite(X) && std::isfinite(Y)
            && X >= 0.0 && Y >= 0.0 && X < ViewportWidth && Y < ViewportHeight;
    }
    bool Contains(double X, double Y) const noexcept { return Valid && Panel.Contains(X, Y); }
    Action HitTest(double X, double Y) const noexcept
    {
        if (!PointerValid(X, Y)) return Action::None;
        for (std::size_t Index = 0; Index < Buttons.size(); ++Index)
            if (Buttons[Index].Contains(X, Y)) return Actions[Index];
        return Action::None;
    }
};

struct Context
{
    bool Playing = false;
    bool HasHQ = false, HasFactory = false, HasSelectedMobile = false;
    int Owner = 0, Epoch = 0;
    int Credits = 0, LiveRecords = 0, RecordCap = 0;
    int RefineryCost = 0, FactoryCost = 0, ProductionCost = 0;
};

enum class Availability { Ready, Unavailable, NeedHQ, NeedFactory, NeedMobile, NeedCredits, AtCapacity };

inline Availability Check(const Context& State, Action Command) noexcept
{
    if (!State.Playing || State.Owner <= 0 || State.Epoch <= 0 || State.Credits < 0
        || State.LiveRecords < 0 || State.RecordCap <= 0 || State.LiveRecords > State.RecordCap)
        return Availability::Unavailable;
    if (Command == Action::Stop)
        return State.HasSelectedMobile ? Availability::Ready : Availability::NeedMobile;
    int Cost = 0, Slots = 1;
    switch (Command)
    {
    case Action::BuildRefinery:
        if (!State.HasHQ) return Availability::NeedHQ;
        Cost = State.RefineryCost; Slots = 2; break; // Existing refinery grant includes its miner.
    case Action::BuildFactory:
        if (!State.HasHQ) return Availability::NeedHQ;
        Cost = State.FactoryCost; break;
    case Action::ProduceCombat:
        if (!State.HasFactory) return Availability::NeedFactory;
        Cost = State.ProductionCost; break;
    default: return Availability::Unavailable;
    }
    if (Cost < 0) return Availability::Unavailable;
    if (State.Credits < Cost) return Availability::NeedCredits;
    if (State.RecordCap - State.LiveRecords < Slots) return Availability::AtCapacity;
    return Availability::Ready;
}

enum class Effect { PassToWorld, UIOnly, Armed, PlaceBuilding, IssueOrder, Rejected };
struct Intent
{
    Effect Result = Effect::PassToWorld;
    Action Command = Action::None;
    Availability Status = Availability::Unavailable;
};

class InputState
{
    Action Pending = Action::None;
    int Owner = 0, Epoch = 0;
public:
    Action PendingAction() const noexcept { return Pending; }
    void Cancel() noexcept { Pending = Action::None; Owner = Epoch = 0; }
    void Synchronize(const Context& State) noexcept
    {
        if (Pending != Action::None && (State.Owner != Owner || State.Epoch != Epoch
            || Check(State, Pending) != Availability::Ready)) Cancel();
    }
    void PlacementSubmitted(bool Submitted) noexcept { if (Submitted) Cancel(); }

    Intent LeftClick(const Layout& Card, const Context& State, double X, double Y, bool OtherHUD) noexcept
    {
        Synchronize(State);
        if (!Card.Valid) { Cancel(); return {Effect::UIOnly}; }
        if (!Card.PointerValid(X, Y)) return {Effect::UIOnly};
        if (Card.Contains(X, Y))
        {
            const Action Command = Card.HitTest(X, Y);
            if (Command == Action::None) return {Effect::UIOnly};
            Cancel();
            const Availability Status = Check(State, Command);
            if (Status != Availability::Ready) return {Effect::Rejected, Command, Status};
            if (Command == Action::BuildRefinery || Command == Action::BuildFactory)
            {
                Pending = Command; Owner = State.Owner; Epoch = State.Epoch;
                return {Effect::Armed, Command, Status};
            }
            return {Effect::IssueOrder, Command, Status};
        }
        if (OtherHUD) return {Effect::UIOnly};
        if (Pending != Action::None) return {Effect::PlaceBuilding, Pending, Availability::Ready};
        return {};
    }

    Intent RightClick(const Layout& Card, double X, double Y, bool OtherHUD) noexcept
    {
        if (Pending != Action::None) { Cancel(); return {Effect::UIOnly}; }
        if (!Card.PointerValid(X, Y) || Card.Contains(X, Y) || OtherHUD) return {Effect::UIOnly};
        return {};
    }
};
} // namespace ProofCommandCard
