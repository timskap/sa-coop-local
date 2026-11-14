#include "shared/local-coop/LocalInput.h"

#include <windows.h>
#include <algorithm>
#include "common/lib/local-coop/Setup.h"
#include "common/lib/local-coop/XInput.h"
#include "common/lib/log/Log.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/input/Input.h"

namespace coop::localinput {
namespace {
using UpdateFn = void(__fastcall*)(game::CPad*, void*, int);
UpdateFn original = nullptr;
constexpr wchar_t kSessionProperty[] = L"SA_COOP_LOCAL_SESSION";
bool SessionFocused() {
    HWND foreground = GetForegroundWindow();
    if (foreground == static_cast<HWND>(game::GameWindow())) return true;
    return Cfg().localSession && reinterpret_cast<uintptr_t>(GetPropW(foreground, kSessionProperty)) == Cfg().localSession;
}
game::CControllerState Controller() {
    game::CControllerState s{};
    XINPUT_STATE input{};
    if (!SessionFocused() || local::ReadPad(Cfg().localDevice, input) != ERROR_SUCCESS) return s;
    const auto& p = input.Gamepad;
    auto button = [&](WORD b) -> int16_t { return (p.wButtons & b) ? 255 : 0; };
    s.LeftStickX = local::Axis(p.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    s.LeftStickY = -local::Axis(p.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    s.RightStickX = local::Axis(p.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    s.RightStickY = -local::Axis(p.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    s.DPadUp = button(XINPUT_GAMEPAD_DPAD_UP);
    s.DPadDown = button(XINPUT_GAMEPAD_DPAD_DOWN);
    s.DPadLeft = button(XINPUT_GAMEPAD_DPAD_LEFT);
    s.DPadRight = button(XINPUT_GAMEPAD_DPAD_RIGHT);
    s.Start = button(XINPUT_GAMEPAD_START);
    s.Select = button(XINPUT_GAMEPAD_BACK);
    s.ButtonTriangle = button(XINPUT_GAMEPAD_Y);
    s.ButtonSquare = button(XINPUT_GAMEPAD_X);
    s.ButtonCross = button(XINPUT_GAMEPAD_A);
    s.ButtonCircle = std::max(button(XINPUT_GAMEPAD_B), local::Trigger(p.bRightTrigger));
    s.RightShoulder1 = local::Trigger(p.bLeftTrigger);
    s.LeftShoulder2 = button(XINPUT_GAMEPAD_LEFT_SHOULDER);
    s.RightShoulder2 = button(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    s.ShockButtonL = button(XINPUT_GAMEPAD_LEFT_THUMB);
    s.ShockButtonR = button(XINPUT_GAMEPAD_RIGHT_THUMB);
    auto* ped = game::FindPlayerPed();
    if (ped && ped->InVehicle() && !game::MenuActive()) {
        s.ButtonCross = local::Trigger(p.bRightTrigger);
        s.ButtonSquare = local::Trigger(p.bLeftTrigger);
        s.ButtonCircle = button(XINPUT_GAMEPAD_B);
        s.RightShoulder1 = button(XINPUT_GAMEPAD_A);
        s.LeftShoulder2 = button(XINPUT_GAMEPAD_LEFT_SHOULDER);
    }
    return s;
}
void __fastcall Update(game::CPad* pad, void* edx, int number) {
    // Hook before GTA reconciles input, so OldState, horn/steering histories and JustDown stay coherent.
    auto& keyboard = game::Field<game::CControllerState>(pad, 0x78);
    auto& joystick = game::Field<game::CControllerState>(pad, 0xA8);
    auto& mouse = game::Field<game::CControllerState>(pad, 0xD8);
    const bool captured = ui::In().KeyboardCaptured();
    if (pad != game::GetPad(0) || captured || Cfg().localDevice == -2) {
        keyboard = joystick = mouse = {};
    } else if (Cfg().localDevice == -1) {
        joystick = {};  // keep vanilla keyboard bindings and mouse, discard every controller
        game::cam::UseMouse3rdPerson() = 1;
        if (!game::IsForeground()) keyboard = mouse = {};
    } else {
        keyboard = mouse = {};
        joystick = Controller();
        game::Field<int16_t>(pad, 0x10A) = 0;  // classic GTA controls, not a saved alternate layout
        game::cam::UseMouse3rdPerson() = 0;
    }
    original(pad, edx, number);
}
}  // namespace

bool UsesGamepad() { return Cfg().localCoop && Cfg().localDevice != -1; }
void RegisterWindow(void* window) {
    if (Cfg().localCoop && Cfg().localSession)
        SetPropW(static_cast<HWND>(window), kSessionProperty, reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Cfg().localSession)));
}
void Install() {
    if (!Cfg().localCoop) return;
    if (!mem::HookFn(0x541C40, &Update, &original, "CPad::Update (local co-op)")) {
        LOGE("Local co-op input hook failed; stopping rather than sharing controls between players");
        MessageBoxW(nullptr, L"Local co-op could not install input isolation. Remove other input mods and retry.", L"SA:COOP", MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(), 4);
    }
}
}  // namespace coop::localinput
