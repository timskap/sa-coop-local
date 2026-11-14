#pragma once
#include <cstdint>

namespace game {
struct CControllerState {
    int16_t LeftStickX;      // 0x00 move left/right
    int16_t LeftStickY;      // 0x02 move forward/back
    int16_t RightStickX;     // 0x04
    int16_t RightStickY;     // 0x06
    int16_t LeftShoulder1;   // 0x08 (look behind / action)
    int16_t LeftShoulder2;   // 0x0A (prev weapon / look left)
    int16_t RightShoulder1;  // 0x0C aim / handbrake
    int16_t RightShoulder2;  // 0x0E (next weapon / look right)
    int16_t DPadUp;          // 0x10
    int16_t DPadDown;        // 0x12
    int16_t DPadLeft;        // 0x14
    int16_t DPadRight;       // 0x16
    int16_t Start;           // 0x18
    int16_t Select;          // 0x1A
    int16_t ButtonSquare;    // 0x1C jump / brake
    int16_t ButtonTriangle;  // 0x1E enter/exit vehicle
    int16_t ButtonCross;     // 0x20 sprint / accelerate
    int16_t ButtonCircle;    // 0x22 fire
    int16_t ShockButtonL;    // 0x24 crouch / horn
    int16_t ShockButtonR;    // 0x26 look behind
    int16_t m_bChatIndicated;      // 0x28
    int16_t m_bPedWalk;            // 0x2A
    int16_t m_bVehicleMouseLook;   // 0x2C
    int16_t m_bRadioTrackSkip;     // 0x2E
};
static_assert(sizeof(CControllerState) == 0x30);
}
