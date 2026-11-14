#pragma once
#include <algorithm>
#include "common/lib/local-coop/Setup.h"
#include "shared/game/ControllerState.h"

namespace coop::localinput {
// XInput's wire values, kept independent of Windows so the actual mapping is tested on every platform.
enum PadButton : uint16_t {
    Up = 0x0001, Down = 0x0002, Left = 0x0004, Right = 0x0008,
    Start = 0x0010, Back = 0x0020, LS = 0x0040, RS = 0x0080,
    LB = 0x0100, RB = 0x0200, A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000
};
struct PadSample {
    uint16_t buttons = 0;
    uint8_t lt = 0, rt = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};
inline game::CControllerState MapGamepad(const PadSample& p, bool vehicle) {
    game::CControllerState s{};
    auto button = [&](uint16_t b) -> int16_t { return (p.buttons & b) ? 255 : 0; };
    s.LeftStickX = local::Axis(p.lx, 7849);
    s.LeftStickY = -local::Axis(p.ly, 7849);
    s.RightStickX = local::Axis(p.rx, 8689);
    s.RightStickY = -local::Axis(p.ry, 8689);
    s.DPadUp = button(Up); s.DPadDown = button(Down);
    s.DPadLeft = button(Left); s.DPadRight = button(Right);
    s.Start = button(Start); s.Select = button(Back);
    s.ButtonTriangle = button(Y);
    s.ButtonSquare = button(X);
    s.ButtonCross = button(A);
    s.ButtonCircle = std::max(button(B), local::Trigger(p.rt));
    s.RightShoulder1 = local::Trigger(p.lt);
    s.LeftShoulder2 = button(LB); s.RightShoulder2 = button(RB);
    s.ShockButtonL = button(LS); s.ShockButtonR = button(RS);
    if (vehicle) {
        s.ButtonCross = local::Trigger(p.rt);
        s.ButtonSquare = local::Trigger(p.lt);
        s.ButtonCircle = button(B);
        s.RightShoulder1 = button(A);
        s.LeftShoulder1 = button(X);  // secondary vehicle action / nitro
    } else if (button(LB) && button(RB)) {
        // GTA's mode-0 pickup/action requires L1, which was previously unreachable.
        s.LeftShoulder1 = 255;
        s.LeftShoulder2 = s.RightShoulder2 = 0;
    }
    return s;
}
}
