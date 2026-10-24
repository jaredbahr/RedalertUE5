#include "Presentation/ProofCommandCard.h"
#include <cstdio>
#include <limits>

using namespace ProofCommandCard;
namespace
{
int Checks = 0, Failures = 0;
void Expect(bool Condition, const char* Label)
{
    ++Checks;
    if (!Condition) { ++Failures; std::printf("FAIL: %s\n", Label); }
}
Context Ready()
{
    Context State;
    State.Playing = State.HasHQ = State.HasFactory = State.HasSelectedMobile = true;
    State.Owner = 1; State.Epoch = 4; State.Credits = 200;
    State.LiveRecords = 5; State.RecordCap = 12;
    // Deliberately distinct from current prototype prices: no hardcoded prices in the policy.
    State.RefineryCost = 97; State.FactoryCost = 149; State.ProductionCost = 51;
    return State;
}
Intent Click(InputState& Input, const Layout& Card, const Context& State, std::size_t Index)
{
    const Rect& Button = Card.Buttons[Index];
    return Input.LeftClick(Card, State, Button.X + Button.Width / 2.0, Button.Y + Button.Height / 2.0, false);
}

void Geometry()
{
    for (const auto& Size : {std::array<double, 2>{640, 360}, {960, 540}, {1280, 720}, {2560, 1440}})
    {
        const Layout Card(Size[0], Size[1]);
        Expect(Card.Valid, "supported viewport has a card");
        Expect(Card.Panel.X >= 0 && Card.Panel.Y >= 0
            && Card.Panel.X + Card.Panel.Width < Size[0] && Card.Panel.Y + Card.Panel.Height < Size[1],
            "card is fully inside viewport");
        Expect(Card.Scale >= 0.72 && Card.Scale <= 1.0, "scale is bounded");
        for (std::size_t Index = 0; Index < Card.Buttons.size(); ++Index)
        {
            const Rect& Button = Card.Buttons[Index];
            Expect(Card.HitTest(Button.X, Button.Y) == Actions[Index], "button includes its top-left pixel");
            Expect(Card.HitTest(Button.X + Button.Width / 2, Button.Y + Button.Height / 2) == Actions[Index],
                "drawn button center resolves to its action");
            Expect(Card.HitTest(Button.X + Button.Width, Button.Y + Button.Height / 2) == Action::None,
                "button excludes right edge and gap");
            Expect(Card.HitTest(Button.X + Button.Width / 2, Button.Y + Button.Height) == Action::None,
                "button excludes bottom edge and gap");
            Expect(Card.Contains(Button.X, Button.Y)
                && Button.X + Button.Width < Card.Panel.X + Card.Panel.Width
                && Button.Y + Button.Height < Card.Panel.Y + Card.Panel.Height, "buttons stay inside panel");
        }
        Expect(Card.HitTest(Card.Panel.X + 1, Card.Panel.Y + 1) == Action::None, "panel background is not a button");
        Expect(!Card.PointerValid(Size[0], Size[1] / 2) && !Card.PointerValid(Size[0] / 2, Size[1]),
            "viewport right and bottom edges are excluded");
    }
    const double Nan = std::numeric_limits<double>::quiet_NaN(), Inf = std::numeric_limits<double>::infinity();
    for (const double Width : {0.0, -1.0, 639.99, Nan, Inf, (std::numeric_limits<double>::max)()})
    {
        const Layout Card(Width, 720);
        Expect(!Card.Valid && Card.HitTest(0, 0) == Action::None && !Card.Contains(0, 0), "invalid width has no hit target");
    }
    for (const double Height : {0.0, -1.0, 359.99, Nan, Inf, (std::numeric_limits<double>::max)()})
        Expect(!Layout(1280, Height).Valid, "invalid height has no card");
    const Layout Card(1280, 720);
    Expect(Card.HitTest(Nan, 600) == Action::None && Card.HitTest(1000, Inf) == Action::None,
        "nonfinite pointer has no action");
    Expect(!Rect{0, 0, Inf, 20}.Contains(1, 1) && !Rect{0, 0, 20, -1}.Contains(1, 1), "invalid rectangle cannot capture input");
}

void Eligibility()
{
    const Context Baseline = Ready();
    for (const Action Command : Actions) Expect(Check(Baseline, Command) == Availability::Ready, "ready context supports each action");
    Expect(Check(Baseline, Action::None) == Availability::Unavailable, "no action is never enabled");
    const int Costs[] = {Baseline.RefineryCost, Baseline.FactoryCost, Baseline.ProductionCost};
    for (std::size_t Index = 0; Index < 3; ++Index)
    {
        Context State = Baseline;
        State.Credits = Costs[Index];
        Expect(Check(State, Actions[Index]) == Availability::Ready, "exact credits are sufficient");
        --State.Credits;
        Expect(Check(State, Actions[Index]) == Availability::NeedCredits, "one credit short disables action");
        State = Baseline; State.LiveRecords = State.RecordCap;
        Expect(Check(State, Actions[Index]) == Availability::AtCapacity, "full unit cap disables paid spawns");
    }
    Context State = Baseline; State.LiveRecords = State.RecordCap - 1;
    Expect(Check(State, Action::BuildRefinery) == Availability::AtCapacity, "refinery reserves two records for structure and miner");
    Expect(Check(State, Action::BuildFactory) == Availability::Ready && Check(State, Action::ProduceCombat) == Availability::Ready,
        "factory and combat need only one record");
    State.HasHQ = false;
    Expect(Check(State, Action::BuildRefinery) == Availability::NeedHQ && Check(State, Action::BuildFactory) == Availability::NeedHQ,
        "building requires live HQ");
    State = Baseline; State.HasFactory = false;
    Expect(Check(State, Action::ProduceCombat) == Availability::NeedFactory, "production requires live factory");
    State = Baseline; State.HasSelectedMobile = false;
    Expect(Check(State, Action::Stop) == Availability::NeedMobile, "Stop requires selected live owned mobile units");
    State = Baseline; State.Credits = 0; State.LiveRecords = State.RecordCap;
    Expect(Check(State, Action::Stop) == Availability::Ready, "Stop is available with no credits at capacity");
    for (int Invalid = 0; Invalid < 8; ++Invalid)
    {
        State = Baseline;
        switch (Invalid)
        {
        case 0: State.Playing = false; break;
        case 1: State.Owner = 0; break;
        case 2: State.Epoch = 0; break;
        case 3: State.Credits = -1; break;
        case 4: State.LiveRecords = -1; break;
        case 5: State.RecordCap = 0; break;
        case 6: State.LiveRecords = State.RecordCap + 1; break;
        case 7: State.Owner = -1; break;
        }
        for (const Action Command : Actions)
            Expect(Check(State, Command) == Availability::Unavailable, "invalid context cannot enable an action");
    }
    State = Baseline; State.RefineryCost = -1; State.FactoryCost = -1; State.ProductionCost = -1;
    for (std::size_t Index = 0; Index < 3; ++Index)
        Expect(Check(State, Actions[Index]) == Availability::Unavailable, "negative cost cannot enable paid action");
}

void Interaction()
{
    const Context State = Ready();
    for (const auto& Size : {std::array<double, 2>{960, 540}, {1280, 720}})
    {
        const Layout Card(Size[0], Size[1]);
        InputState Input;
        Expect(Input.LeftClick(Card, State, 600, 250, false).Result == Effect::PassToWorld, "ordinary world selection remains available");
        Expect(Click(Input, Card, State, 0).Result == Effect::Armed && Input.PendingAction() == Action::BuildRefinery,
            "refinery button arms placement without building at button coordinates");
        Expect(Input.LeftClick(Card, State, Card.Panel.X + 1, Card.Panel.Y + 1, false).Result == Effect::UIOnly,
            "panel background cannot click through while placing");
        Expect(Input.LeftClick(Card, State, 20, 20, true).Result == Effect::UIOnly, "diagnostic panel cannot receive a ground placement");
        const Intent Placement = Input.LeftClick(Card, State, 600, 250, false);
        Expect(Placement.Result == Effect::PlaceBuilding && Placement.Command == Action::BuildRefinery,
            "ground click requests the armed building action");
        Input.PlacementSubmitted(false);
        Expect(Input.PendingAction() == Action::BuildRefinery, "bad ground projection retains placement");
        Input.PlacementSubmitted(true);
        Expect(Input.PendingAction() == Action::None, "submitted command clears placement without claiming host acceptance");
        Click(Input, Card, State, 1);
        Expect(Input.PendingAction() == Action::BuildFactory, "factory button arms factory action");
        Expect(Input.RightClick(Card, 600, 250, false).Result == Effect::UIOnly && Input.PendingAction() == Action::None,
            "right-click cancels placement and cannot issue a world order in the same click");
        Expect(Input.RightClick(Card, 600, 250, false).Result == Effect::PassToWorld, "next ordinary right-click reaches world orders");
        Expect(Input.RightClick(Card, Card.Panel.X + 1, Card.Panel.Y + 1, false).Result == Effect::UIOnly,
            "right-click over command panel cannot issue a ground order");
        Expect(Input.RightClick(Card, 20, 20, true).Result == Effect::UIOnly, "right-click over other HUD cannot issue a ground order");
        Click(Input, Card, State, 0);
        const Intent Produce = Click(Input, Card, State, 2);
        Expect(Produce.Result == Effect::IssueOrder && Produce.Command == Action::ProduceCombat && Input.PendingAction() == Action::None,
            "production button replaces placement with one shared production action");
        const Intent Stop = Click(Input, Card, State, 3);
        Expect(Stop.Result == Effect::IssueOrder && Stop.Command == Action::Stop, "Stop button uses shared Stop action");
        Click(Input, Card, State, 0);
        Context Disabled = State; Disabled.HasFactory = false;
        const Intent Rejected = Click(Input, Card, Disabled, 2);
        Expect(Rejected.Result == Effect::Rejected && Rejected.Status == Availability::NeedFactory
            && Input.PendingAction() == Action::None, "disabled button cannot dispatch and cancels stale placement intent");
        for (int Change = 0; Change < 6; ++Change)
        {
            Click(Input, Card, State, 0);
            Context Changed = State;
            switch (Change)
            {
            case 0: ++Changed.Owner; break;
            case 1: ++Changed.Epoch; break;
            case 2: Changed.Playing = false; break;
            case 3: Changed.HasHQ = false; break;
            case 4: Changed.Credits = 0; break;
            case 5: Changed.LiveRecords = Changed.RecordCap; break;
            }
            Input.Synchronize(Changed);
            Expect(Input.PendingAction() == Action::None, "owner, rematch, finish, lost HQ/resources/capacity cancel pending placement");
            Input.Synchronize(State);
            Expect(Input.PendingAction() == Action::None, "restored context never rearms a cancelled build");
        }
        Click(Input, Card, State, 0);
        Expect(Input.LeftClick(Card, State, std::numeric_limits<double>::quiet_NaN(), 200, false).Result == Effect::UIOnly,
            "invalid pointer cannot dispatch placement");
        Input.LeftClick(Layout(0, 0), State, 0, 0, false);
        Expect(Input.PendingAction() == Action::None, "unusable viewport cancels placement");
        Click(Input, Card, State, 0); Input.Cancel();
        Expect(Input.PendingAction() == Action::None, "explicit cancel is safe for focus loss and Escape");
    }
}
}

int main()
{
    Geometry(); Eligibility(); Interaction();
    std::printf("%s: %d command-card checks, %d failures (production header)\n", Failures ? "FAIL" : "PASS", Checks, Failures);
    return Failures ? 1 : 0;
}
