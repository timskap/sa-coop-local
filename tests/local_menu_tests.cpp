#include <cassert>
#include <iostream>
#include "shared/local-coop/Gamepad.h"

using namespace coop::localinput;
int main() {
    PadRouter first, second;
    first.Update({}, true, false); second.Update({}, true, false);
    auto press = first.Update({Start}, true, false);
    assert(press.escape && !second.Update({}, true, false).escape);
    for (int i = 0; i < 20; ++i) assert(!first.Update({Start}, true, false).escape);
    // Native menu activation clears CPad, but a held physical Menu must not close it again.
    assert(!first.Update({Start}, true, true).escape);
    first.Update({}, true, true);
    assert(first.Update({Start}, true, true).escape);
    assert(!first.Update({Start}, true, true).escape);
    first.Update({}, true, true);
    assert(first.Update({Back}, true, true).escape);
    assert(!first.Update({Back}, true, true).escape);
    first.Update({}, true, true);
    assert(first.Update({B}, true, true).escape);
    // Cancel from a menu must not become fire when the game resumes with B still held.
    const auto resumed = first.Update({B, 255, 255}, true, false);
    const auto state = MapGamepad(resumed.sample, false);
    assert(!resumed.escape && !state.ButtonCircle && !state.RightShoulder1);
    first.Update({}, true, false);
    assert(MapGamepad(first.Update({B}, true, false).sample, false).ButtonCircle == 255);
    first.Update({}, true, false);
    const auto view = first.Update({Back}, true, false);
    assert(!view.escape && MapGamepad(view.sample, false).Select == 255);
    assert(!first.Update({Back}, true, true).escape); // held camera button cannot cancel a newly opened menu
    first.Update({}, true, true);
    assert(first.Update({Back}, true, true).escape);

    for (bool vehicle : {false, true}) {
        const auto confirm = MapGamepad({A}, vehicle, true);
        assert(confirm.ButtonCross == 255 && !confirm.ButtonCircle && !confirm.ButtonTriangle);
        const auto cancel = MapGamepad({B | Back}, vehicle, true);
        assert(!cancel.ButtonCircle && !cancel.ButtonTriangle && !cancel.ButtonCross);
        const auto waypoint = MapGamepad({X}, vehicle, true);
        assert(waypoint.ButtonCircle == 255 && !waypoint.ButtonSquare);
        const auto zoom = MapGamepad({LB | RB, 255, 255}, vehicle, true);
        assert(zoom.LeftShoulder2 == 255 && zoom.RightShoulder2 == 255 && !zoom.LeftShoulder1);
        assert(!zoom.RightShoulder1 && !zoom.ButtonCircle && !zoom.ButtonSquare && !zoom.ButtonCross);
    }
    // Loss of focus, disconnect or overlay capture all disable the route. Returning
    // with a held button must wait for release, and cannot deliver a queued menu action.
    for (uint16_t button : {uint16_t(Start), uint16_t(Back), uint16_t(B)}) {
        PadRouter route;
        assert(!route.Update({button}, false, true).escape);
        assert(!route.Update({button}, true, true).escape);
        route.Update({}, true, true);
        assert(route.Update({button}, true, true).escape);
        assert(!route.Update({button}, false, true).escape);
        assert(!route.Update({button}, true, true).escape);
    }

    // Exercise the exact scoped bridge used around native menu processing, with
    // the game's new/old-key check and a simulated ClearKeyBoardHistory inside it.
    int16_t now = 17, old = 23;
    {
        MenuEscapeScope scope(now, old);
        assert(now && !old);
        now = old = 0;
    }
    assert(now == 17 && old == 23);
    std::cout << "Local menus: Menu/View/B edges, scoped Esc bridge, A confirm, camera mapping, seat isolation and held-button recovery passed\n";
}
