#include <cassert>
#include <iostream>
#include <cstring>
#include "common/lib/local-coop/Setup.h"
#include "shared/local-coop/Gamepad.h"

using namespace coop::local;
int main() {
    Setup setup;
    assert(!setup.CanStart());
    assert(!setup.Assign(0, 0));  // an unplugged pad cannot be assigned
    assert(!setup.Assign(kUnassigned, 0));
    assert(!setup.Assign(kKeyboard, 2));
    setup.Assign(kKeyboard, 0);
    setup.ToggleReady(kKeyboard);
    assert(setup.seats[0].ready && !setup.CanStart());
    setup.Connect(2, true);
    setup.Assign(2, 1);
    setup.ToggleReady(2);
    assert(setup.CanStart());
    // Moving a device empties its old seat; it does not clone ownership or readiness.
    setup.Assign(kKeyboard, 1);
    assert(setup.seats[0].device == kUnassigned);
    assert(!setup.seats[1].ready && setup.Owner(2) == -1 && !setup.CanStart());
    setup.Assign(2, 0);
    setup.ToggleReady(2); setup.ToggleReady(kKeyboard);
    assert(setup.CanStart());  // keyboard works as P2 too
    setup.Connect(2, false);
    assert(setup.Owner(2) == -1 && !setup.CanStart());
    setup.Connect(2, true);
    assert(!setup.CanStart());  // reconnect never silently readies a player
    setup.Assign(2, 0); setup.ToggleReady(2);
    assert(setup.CanStart());
    setup.Release(kKeyboard);
    assert(!setup.CanStart());
    // Exercise every supported device pairing, including rejected duplicates.
    for (int a = -1; a < 4; ++a) for (int b = -1; b < 4; ++b) {
        Setup pair;
        for (int i = 0; i < 4; ++i) pair.Connect(i, true);
        pair.Assign(a, 0); pair.ToggleReady(a);
        pair.Assign(b, 1); pair.ToggleReady(b);
        assert(pair.CanStart() == (a != b));
        if (a != b) {
            pair.ToggleReady(a); assert(!pair.CanStart());
            pair.ToggleReady(a); assert(pair.CanStart());
        }
    }
    // Odd desktop sizes must not produce a gap, overlap, or a clipped last row/column.
    for (auto size : {std::array<int, 2>{1920, 1080}, {2561, 1441}, {1280, 720}}) {
        const auto h = Split(size[0], size[1], true);
        assert(h[0].width == size[0] && h[1].width == size[0]);
        assert(h[0].height + h[1].height == size[1] && h[1].y == h[0].height);
        const auto v = Split(size[0], size[1], false);
        assert(v[0].height == size[1] && v[1].height == size[1]);
        assert(v[0].width + v[1].width == size[0] && v[1].x == v[0].width);
    }
    // Exhaustive axis checks cover -32768 (asymmetric signed range), dead zones and monotonicity.
    for (int deadzone : {7849, 8689}) {
        int previous = -128;
        for (int value = -32768; value <= 32767; ++value) {
            const int axis = Axis(value, deadzone);
            assert(axis >= -128 && axis <= 128 && axis >= previous);
            if (std::abs(value) <= deadzone) assert(axis == 0);
            previous = axis;
        }
        assert(Axis(-32768, deadzone) == -128 && Axis(32767, deadzone) == 128);
    }
    int previous = 0;
    for (unsigned value = 0; value < 256; ++value) {
        const int trigger = Trigger(value);
        assert(trigger >= previous && trigger <= 255);
        if (value <= 30) assert(trigger == 0);
        previous = trigger;
    }
    assert(Trigger(255) == 255);
    using namespace coop::localinput;
    const auto pickup = MapGamepad({LB | RB}, false);
    assert(pickup.LeftShoulder1 == 255 && !pickup.LeftShoulder2 && !pickup.RightShoulder2);
    assert(!pickup.ButtonCircle && !pickup.ButtonCross && !pickup.ButtonSquare);
    assert(MapGamepad({LB}, false).LeftShoulder2 == 255);
    assert(MapGamepad({RB}, false).RightShoulder2 == 255);
    const auto nitro = MapGamepad({X}, true);
    assert(nitro.LeftShoulder1 == 255 && !nitro.ButtonSquare && !nitro.ButtonCross);
    const auto drive = MapGamepad({A, 255, 255}, true);
    assert(drive.ButtonCross == 255 && drive.ButtonSquare == 255 && drive.RightShoulder1 == 255);
    assert(!drive.ButtonCircle);  // accelerating must not shoot
    const auto onFoot = MapGamepad({X, 255, 255}, false);
    assert(onFoot.ButtonSquare == 255 && onFoot.RightShoulder1 == 255 && onFoot.ButtonCircle == 255);
    const auto sticks = MapGamepad({0, 0, 0, -32768, 32767, 32767, -32768}, false);
    assert(sticks.LeftStickX == -128 && sticks.LeftStickY == -128);
    assert(sticks.RightStickX == 128 && sticks.RightStickY == 128);
    const game::CControllerState zero{};
    for (bool vehicle : {false, true}) {
        const auto neutral = MapGamepad({}, vehicle);
        assert(std::memcmp(&zero, &neutral, sizeof(zero)) == 0);
        // Exhaust every button combination for invalid ranges or uninitialised output.
        for (unsigned buttons = 0; buttons <= 0xFFFF; ++buttons) {
            const auto state = MapGamepad({static_cast<uint16_t>(buttons)}, vehicle);
            int16_t values[sizeof(state) / sizeof(int16_t)];
            std::memcpy(values, &state, sizeof(state));
            for (int16_t value : values) assert(value == 0 || value == 255);
        }
    }
    std::cout << "Local co-op: assignments, readiness, disconnects, layouts and input ranges passed\n";
}
