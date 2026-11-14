#pragma once
#include <windows.h>
#include <cstdint>

namespace coop::local {
inline constexpr wchar_t kSessionProperty[] = L"SA_COOP_LOCAL_SESSION";
inline constexpr wchar_t kContainerClass[] = L"SA_COOP_FULLSCREEN";
inline constexpr UINT kFocusPlayer = WM_APP + 0x340;
inline constexpr UINT kFocusKeyboard = WM_APP + 0x341;
inline constexpr UINT kSessionControls = WM_APP + 0x342;
inline constexpr UINT kSessionActivation = WM_APP + 0x343;

inline bool IsContainer(HWND window, DWORD session) {
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    wchar_t name[64]{};
    GetClassNameW(window, name, 64);
    return session && owner == session && !wcscmp(name, kContainerClass) &&
        reinterpret_cast<uintptr_t>(GetPropW(window, kSessionProperty)) == session;
}
inline bool ContainerFocused(HWND container) {
    return container && GetForegroundWindow() == container && !IsIconic(container);
}
inline bool PlayerFocused(HWND window, HWND container) {
    if (!ContainerFocused(container)) return false;
    GUITHREADINFO info{sizeof(info)};
    return GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &info) &&
        (info.hwndFocus == window || IsChild(window, info.hwndFocus));
}
// Call from the game's window thread. Both processes use system DPI awareness.
inline bool EmbedPlayer(HWND window, HWND container, DWORD session, int x, int y, int width, int height) {
    if (!IsContainer(container, session)) return false;
    const LONG style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    SetWindowLongW(window, GWL_STYLE, style);
    SetWindowLongW(window, GWL_EXSTYLE, 0);
    if (GetParent(window) != container) {
        SetLastError(0);
        if (!SetParent(window, container) && GetLastError()) return false;
        SendMessageW(window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_INITIALIZE, 0), 0);
    }
    return SetWindowPos(window, nullptr, x, y, width, height,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW) != FALSE;
}
// Reserved session controls are handled before GTA or the mod's overlay sees keys.
inline bool PlayerWindowMessage(HWND window, HWND container, UINT message, WPARAM wp, LPARAM lp, LRESULT& result) {
    if (!container || GetParent(window) != container) return false;
    result = 0;
    if (message == kSessionActivation) {
        // Child windows do not receive the top-level parent's activation transition.
        // Deliver it on the game thread so native audio/input activation stays coherent.
        SendMessageW(window, WM_ACTIVATE, wp ? WA_ACTIVE : WA_INACTIVE, 0);
        return true;
    }
    if (message == kFocusPlayer) {
        if (ContainerFocused(container)) SetFocus(window);
        return true;
    }
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wp == VK_F10) {
        if (!(lp & (1L << 30))) PostMessageW(container, kSessionControls, 0, 0);
        return true;
    }
    if ((message == WM_KEYUP || message == WM_SYSKEYUP) && wp == VK_F10) return true;
    if ((message == WM_SYSKEYDOWN && wp == VK_F4) || (message == WM_CLOSE && wp != 1)) {
        PostMessageW(container, WM_CLOSE, 0, 0);
        return true;
    }
    return false;
}
}  // namespace coop::local
