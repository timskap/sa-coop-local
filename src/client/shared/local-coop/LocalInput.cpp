#include "shared/local-coop/LocalInput.h"
#include "shared/local-coop/Gamepad.h"

#include <windows.h>
#include <dinput.h>
#include <algorithm>
#include <map>
#include "common/lib/local-coop/Setup.h"
#include "common/lib/local-coop/XInput.h"
#include "common/lib/local-coop/WindowHost.h"
#include "common/lib/local-coop/InputWindow.h"
#include "common/lib/log/Log.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/input/Input.h"

namespace coop::localinput {
namespace {
using UpdateFn = void(__fastcall*)(game::CPad*, void*, int);
UpdateFn original = nullptr;
using InputCreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, void**, IUnknown*);
using DeviceCreateFn = HRESULT(WINAPI*)(void*, REFGUID, void**, IUnknown*);
using CooperativeFn = HRESULT(WINAPI*)(void*, HWND, DWORD);
InputCreateFn originalInputCreate = nullptr;
std::map<void**, DeviceCreateFn> originalDeviceCreates;
std::map<void**, CooperativeFn> originalCooperatives;
bool recoverFocus = false, wasFocused = false, recoverMouse = false;
ULONGLONG nextMouseRecovery = 0;
HRESULT mouseRecoveryError = S_OK;
HRESULT WINAPI Cooperative(void* device, HWND window, DWORD flags) {
    if (Cfg().localContainer) {
        window = local::BackgroundInputWindow();
        if (!window) return E_FAIL;
        flags = DISCL_BACKGROUND | DISCL_NONEXCLUSIVE;
    }
    return originalCooperatives.at(*reinterpret_cast<void***>(device))(device, window, flags);
}
HRESULT WINAPI DeviceCreate(void* input, REFGUID guid, void** result, IUnknown* outer) {
    HRESULT hr = originalDeviceCreates.at(*reinterpret_cast<void***>(input))(input, guid, result, outer);
    if (SUCCEEDED(hr) && result && *result) {
        void** vt = *reinterpret_cast<void***>(*result);
        if (vt[13] != reinterpret_cast<void*>(&Cooperative)) {
            originalCooperatives[vt] = reinterpret_cast<CooperativeFn>(mem::SwapVTable(vt, 13, reinterpret_cast<void*>(&Cooperative)));
        }
    }
    return hr;
}
HRESULT WINAPI InputCreate(HINSTANCE module, DWORD version, REFIID iid, void** result, IUnknown* outer) {
    HRESULT hr = originalInputCreate(module, version, iid, result, outer);
    if (SUCCEEDED(hr) && result && *result) {
        void** vt = *reinterpret_cast<void***>(*result);
        if (vt[3] != reinterpret_cast<void*>(&DeviceCreate))
            originalDeviceCreates[vt] = reinterpret_cast<DeviceCreateFn>(mem::SwapVTable(vt, 3, reinterpret_cast<void*>(&DeviceCreate)));
    }
    return hr;
}
constexpr wchar_t kSessionProperty[] = L"SA_COOP_LOCAL_SESSION";
bool SessionFocused() {
    if (HWND parent = static_cast<HWND>(ContainerWindow())) return local::ContainerFocused(parent);
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
        if (!HasInputFocus()) keyboard = mouse = {};
    } else {
        keyboard = mouse = {};
        joystick = Controller();
        game::Field<int16_t>(pad, 0x10A) = 0;  // classic GTA controls, not a saved alternate layout
        game::cam::UseMouse3rdPerson() = 0;
    }
    original(pad, edx, number);
}
}  // namespace

void* ContainerWindow() {
    HWND window = reinterpret_cast<HWND>(static_cast<uintptr_t>(Cfg().localContainer));
    return Cfg().localCoop && local::IsContainer(window, Cfg().localSession) ? window : nullptr;
}
bool HasInputFocus() {
    if (UsesGamepad()) return SessionFocused();
    if (HWND parent = static_cast<HWND>(ContainerWindow()))
        return local::PlayerFocused(static_cast<HWND>(game::GameWindow()), parent);
    return game::IsForeground();
}

void RequestFocusRecovery() { if (Cfg().localCoop && Cfg().localContainer) recoverFocus = true; }
void ServiceFocus() {
    HWND container = static_cast<HWND>(ContainerWindow());
    if (!container) return;
    HWND window = static_cast<HWND>(game::GameWindow());
    const bool focused = local::PlayerFocused(window, container);
    // WM_ACTIVATE/WM_KILLFOCUS can be queued out of order across the embedded processes.
    // GTA's cached flag must agree with the real focused child, not a stale message.
    game::Ref<uint8_t>(game::addr::ForegroundApp) = focused;
    if (!focused) { wasFocused = false; recoverMouse = false; return; }
    if (!wasFocused || recoverFocus) {
        recoverFocus = false;
        wasFocused = true;
        ui::In().ClearGameKeys();
        memset((void*)0xB73418, 0, 0x28);
        ClipCursor(nullptr);
        recoverMouse = true;
        nextMouseRecovery = 0;
        mouseRecoveryError = S_OK;
    }
    if (!recoverMouse || GetTickCount64() < nextMouseRecovery) return;
    // psGlobalType::diMouse (same-process native COM interface; no cached dangling pointer).
    auto* ps = game::Ref<void*>(0xC17054);
    auto* mouse = ps ? game::Field<IDirectInputDevice8A*>(ps, 0x1C) : nullptr;
    const HRESULT result = local::RecoverMouse(mouse);
    if (FAILED(result)) {
        if (result != mouseRecoveryError) LOGW("Local focus: mouse reacquire pending (HRESULT %08lX)", (unsigned long)result);
        mouseRecoveryError = result;
        nextMouseRecovery = GetTickCount64() + 250;
        return;
    }
    recoverMouse = false;
    if (!UsesGamepad() && !ui::In().MouseCaptured()) {
        RECT rect{}; GetClientRect(window, &rect);
        POINT center{rect.right / 2, rect.bottom / 2};
        if (ClientToScreen(window, &center)) SetCursorPos(center.x, center.y);
    }
    LOGI("Local focus: keyboard state reset and mouse reacquired for hwnd %p", window);
}

bool UsesGamepad() { return Cfg().localCoop && Cfg().localDevice != -1; }
void RegisterWindow(void* window) {
    if (Cfg().localCoop && Cfg().localSession)
        SetPropW(static_cast<HWND>(window), kSessionProperty, reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Cfg().localSession)));
}
void Install() {
    if (!Cfg().localCoop) return;
    if (Cfg().localContainer) {
        HMODULE input = LoadLibraryW(L"dinput8.dll");
        auto* create = input ? reinterpret_cast<void*>(GetProcAddress(input, "DirectInput8Create")) : nullptr;
        if (!create || !mem::Hook(reinterpret_cast<uintptr_t>(create), reinterpret_cast<void*>(&InputCreate),
                                 reinterpret_cast<void**>(&originalInputCreate), "DirectInput8Create (embedded input)")) {
            LOGE("Cannot install embedded window input support");
            TerminateProcess(GetCurrentProcess(), 5);
        }
    }
    if (!mem::HookFn(0x541C40, &Update, &original, "CPad::Update (local co-op)")) {
        LOGE("Local co-op input hook failed; stopping rather than sharing controls between players");
        MessageBoxW(nullptr, L"Local co-op could not install input isolation. Remove other input mods and retry.", L"SA:COOP", MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(), 4);
    }
}
}  // namespace coop::localinput
