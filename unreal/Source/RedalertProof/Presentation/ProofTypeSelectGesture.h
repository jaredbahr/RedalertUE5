#pragma once
#include <cmath>
#include <cstdint>

// Prototype input timing, not an assertion about original-game timing.
namespace ProofTypeSelectGesture
{
inline constexpr double PairSeconds = 0.3;
enum class Scope { Ignored, Visible, Battlefield };
struct Context
{
    std::int32_t Owner = -1, Epoch = -1, Anchor = -1, Kind = -1, Mode = -1;
    std::uint64_t SelectionRevision = 0;
    bool operator==(const Context& Other) const noexcept
    {
        return Owner == Other.Owner && Epoch == Other.Epoch && Anchor == Other.Anchor
            && Kind == Other.Kind && Mode == Other.Mode && SelectionRevision == Other.SelectionRevision;
    }
};
class State
{
public:
    bool IsArmed() const noexcept { return Armed; }
    const Context& Expected() const noexcept { return ExpectedContext; }
    void Cancel() noexcept { Armed = AwaitVisible = Down = false; }
    void Release() noexcept { Down = false; }
    Scope Press(double Now, const Context& Current, bool Valid, bool Focused) noexcept
    {
        if (!std::isfinite(Now) || Now < 0.0 || (HasTime && Now < LastTime))
        { Cancel(); HasTime = false; return Scope::Ignored; }
        if (!Valid || !Focused) { Cancel(); return Scope::Ignored; }
        if (Down) return Scope::Ignored; // Held/repeated key callbacks do not form a pair.
        Down = true;
        HasTime = true;
        LastTime = Now;
        AwaitVisible = false;
        if (Armed && Current == ExpectedContext && Now - FirstTime <= PairSeconds)
        { Armed = false; return Scope::Battlefield; }
        Armed = false;
        AwaitVisible = true;
        return Scope::Visible;
    }
    bool CommitVisible(double Now, const Context& PostSelection) noexcept
    {
        if (!Down || !AwaitVisible || !std::isfinite(Now) || Now != LastTime) return false;
        ExpectedContext = PostSelection;
        FirstTime = Now;
        Armed = true;
        AwaitVisible = false;
        return true;
    }
private:
    bool Armed = false, Down = false, AwaitVisible = false, HasTime = false;
    double FirstTime = 0.0, LastTime = 0.0;
    Context ExpectedContext;
};
}
