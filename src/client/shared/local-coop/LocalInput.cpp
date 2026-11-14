#include "shared/local-coop/LocalInput.h"
#include "shared/local-coop/Gamepad.h"

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
    if (!HasInputFocus() || local::ReadPad(Cfg().localDevice, input) != ERROR_SUCCESS) return s;
    const auto& p = input.Gamepad;
    auto* ped = game::FindPlayerPed();
    return MapGamepad({p.wButtons, p.bLeftTrigger, p.bRightTrigger,
                       p.sThumbLX, p.sThumbLY, p.sThumbRX, p.sThumbRY},
                      ped && ped->InVehicle() && !game::MenuActive());
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

bool HasInputFocus() { return UsesGamepad() ? SessionFocused() : game::IsForeground(); }

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
