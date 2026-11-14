#pragma once
#include <windows.h>
#include <dinput.h>

namespace coop::local {
// DirectInput requires a top-level HWND in its own process. A game embedded in the
// launcher's window cannot pass either its child HWND or the foreign parent. Use a
// hidden local owner and nonexclusive background access, gated by session focus in
// LocalInput before any state reaches GTA. This window has no taskbar/Alt+Tab entry.
inline HWND BackgroundInputWindow() {
    static HWND window = nullptr;
    if (!IsWindow(window))
        window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"SA:COOP input owner",
                                 WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    return window;
}
template<class Device> HRESULT RecoverMouse(Device* mouse) {
    if (!mouse) return DIERR_NOTINITIALIZED;
    HWND owner = BackgroundInputWindow();
    if (!owner) return E_FAIL;
    mouse->Unacquire();
    HRESULT result = mouse->SetCooperativeLevel(owner, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(result)) return result;
    result = mouse->Acquire();
    if (FAILED(result)) return result;
    // Discard motion accumulated while another app/desktop owned the pointer.
    DIMOUSESTATE2 stale{};
    return mouse->GetDeviceState(sizeof(stale), &stale);
}
}
