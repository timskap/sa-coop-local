#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>

namespace coop::local {
// One physical keyboard/mouse, four XInput slots. A device can belong to only one player.
constexpr int kUnassigned = -2, kKeyboard = -1;
struct Seat { int device = kUnassigned; bool ready = false; };
struct Setup {
    std::array<Seat, 2> seats{};
    std::array<bool, 4> connected{};
    bool horizontal = true;

    bool Available(int device) const {
        return device == kKeyboard || (device >= 0 && device < 4 && connected[device]);
    }
    int Owner(int device) const {
        if (device == kUnassigned) return -1;
        for (int i = 0; i < 2; ++i) if (seats[i].device == device) return i;
        return -1;
    }
    bool Assign(int device, int player) {
        if (!Available(device) || player < 0 || player > 1) return false;
        if (seats[player].device == device) return true;
        const int old = Owner(device);
        if (old >= 0) seats[old] = {};
        seats[player] = {device, false};
        return true;
    }
    void Release(int device) { const int i = Owner(device); if (i >= 0) seats[i] = {}; }
    void ToggleReady(int device) {
        const int i = Owner(device);
        if (i >= 0 && Available(device)) seats[i].ready = !seats[i].ready;
    }
    void Connect(int device, bool present) {
        if (device < 0 || device >= 4) return;
        if (!present) Release(device);
        connected[device] = present;
    }
    bool CanStart() const {
        return seats[0].ready && seats[1].ready && Available(seats[0].device) &&
               Available(seats[1].device) && seats[0].device != seats[1].device;
    }
};
struct Viewport { int x, y, width, height; };
inline std::array<Viewport, 2> Split(int width, int height, bool horizontal) {
    if (horizontal) return {{{0, 0, width, height / 2}, {0, height / 2, width, height - height / 2}}};
    return {{{0, 0, width / 2, height}, {width / 2, 0, width - width / 2, height}}};
}
inline int16_t Axis(int value, int deadzone) {
    const int magnitude = std::abs(value);
    if (magnitude <= deadzone) return 0;
    // Clamp -32768 before conversion; GTA's full scale is 128, not XInput's 32767.
    int scaled = (magnitude - deadzone) * 128 / (32767 - deadzone);
    if (scaled > 128) scaled = 128;
    return static_cast<int16_t>(value < 0 ? -scaled : scaled);
}
inline int16_t Trigger(unsigned value) {
    return value <= 30 ? 0 : static_cast<int16_t>((value - 30) * 255 / 225);
}
}  // namespace coop::local
