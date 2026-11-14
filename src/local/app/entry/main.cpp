// Native couch co-op launcher. No browser, game assets, or external controller middleware required.
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "common/lib/local-coop/Setup.h"
#include "common/lib/local-coop/XInput.h"
#include "common/lib/util/Util.h"
#include "features/session/LocalSession.h"

namespace {
using namespace coop;
constexpr int kWidth = 1040, kHeight = 760;
constexpr COLORREF kBg = RGB(10, 17, 28), kPanel = RGB(20, 31, 45), kMuted = RGB(151, 168, 188);
constexpr COLORREF kWhite = RGB(242, 247, 251), kMint = RGB(149, 244, 170), kBlue = RGB(121, 192, 255);
constexpr int kStart = 100, kLayout = 101, kSave = 102, kData = 103, kReady1 = 110, kReady2 = 111, kDevice = 200;
struct Control { HWND hwnd; int id, x, y, w, h; };
std::vector<Control> controls;
local::Setup setup;
std::unique_ptr<local::Session> session;
std::array<WORD, 4> lastButtons{};
HWND window = nullptr;
HFONT buttonFont = nullptr;
int selectedSave = -1;
bool wasActive = false;
std::wstring DeviceName(int device) {
    return device == local::kKeyboard ? L"KEYBOARD + MOUSE" : device >= 0 ? L"GAMEPAD " + std::to_wstring(device + 1) : L"CHOOSE YOUR DEVICE";
}
HWND ControlFor(int id) {
    for (const auto& control : controls) if (control.id == id) return control.hwnd;
    return nullptr;
}
void Label(int id, const std::wstring& text) {
    HWND control = ControlFor(id);
    wchar_t previous[256]{}; GetWindowTextW(control, previous, 256);
    if (text != previous) { SetWindowTextW(control, text.c_str()); InvalidateRect(control, nullptr, FALSE); }
}
void Box(HDC dc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color); FillRect(dc, &rect, brush); DeleteObject(brush);
}
void Text(HDC dc, const std::wstring& value, RECT rect, int size, COLORREF color = kWhite, bool bold = false,
          UINT flags = DT_LEFT | DT_SINGLELINE | DT_VCENTER) {
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    DrawTextW(dc, value.c_str(), -1, &rect, flags);
    SelectObject(dc, old); DeleteObject(font);
}
void Outline(HDC dc, RECT rect, COLORREF color, int width = 2) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 18, 18);
    SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
}
void DeviceIcon(HDC dc, int x, int y, bool keyboard, COLORREF color) {
    if (keyboard) {
        Outline(dc, {x, y + 8, x + 118, y + 66}, color, 3);
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 9; ++col)
            Box(dc, {x + 10 + col * 11, y + 17 + row * 10, x + 16 + col * 11, y + 22 + row * 10}, color);
        Box(dc, {x + 35, y + 51, x + 84, y + 56}, color);
    } else {
        Outline(dc, {x, y + 5, x + 118, y + 70}, color, 3);
        Box(dc, {x + 23, y + 20, x + 30, y + 45}, color);
        Box(dc, {x + 14, y + 29, x + 39, y + 36}, color);
        for (POINT point : {POINT{x + 86, y + 21}, POINT{x + 98, y + 33}, POINT{x + 86, y + 45}, POINT{x + 74, y + 33}})
            Box(dc, {point.x, point.y, point.x + 6, point.y + 6}, color);
    }
}
void Paint(HDC target) {
    RECT client{}; GetClientRect(window, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    SetMapMode(dc, MM_ANISOTROPIC);
    SetWindowExtEx(dc, kWidth, kHeight, nullptr); SetViewportExtEx(dc, client.right, client.bottom, nullptr);
    Box(dc, {0, 0, kWidth, kHeight}, kBg);
    // Stadium-like diagonal bands, kept subtle behind the content.
    HPEN line = CreatePen(PS_SOLID, 1, RGB(24, 36, 50)); HGDIOBJ oldPen = SelectObject(dc, line);
    for (int x = 400; x < 1400; x += 45) { MoveToEx(dc, x, 0, nullptr); LineTo(dc, x - 450, kHeight); }
    SelectObject(dc, oldPen); DeleteObject(line);
    Box(dc, {36, 32, 41, 69}, kMint);
    Text(dc, L"SA:COOP", {54, 26, 360, 70}, 31, kWhite, true);
    Text(dc, L"LOCAL PLAY  /  TWO PLAYERS", {670, 30, 1004, 66}, 15, kMint, true, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    Text(dc, L"CHOOSE SIDES", {36, 82, 740, 137}, 45, kWhite, true);
    Text(dc, L"One screen. Two players. Your own controls.", {39, 140, 1000, 166}, 18, kMuted);
    for (int i = 0; i < 2; ++i) {
        const int x = i ? 550 : 36;
        const COLORREF accent = i ? kBlue : kMint;
        const auto& seat = setup.seats[i];
        Box(dc, {x, 188, x + 454, 376}, kPanel);
        Box(dc, {x, 188, x + 454, 192}, accent);
        Text(dc, i ? L"PLAYER 2" : L"PLAYER 1", {x + 22, 204, x + 230, 237}, 24, accent, true);
        Text(dc, i ? L"PARTNER" : L"HOST", {x + 300, 204, x + 432, 237}, 13, kMuted, true, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        DeviceIcon(dc, x + 24, 258, seat.device == local::kKeyboard, seat.device == local::kUnassigned ? kMuted : accent);
        Text(dc, DeviceName(seat.device), {x + 167, 254, x + 440, 284}, 19, kWhite, true);
        const bool available = setup.Available(seat.device);
        Text(dc, seat.device == local::kUnassigned ? L"Move a device to this side" : !available ? L"DISCONNECTED - reconnect this pad" :
             seat.ready ? L"READY TO PLAY" : L"Press A / Enter to get ready", {x + 167, 288, x + 442, 317}, 14,
             seat.ready && available ? accent : kMuted);
    }
    Text(dc, L"+", {490, 258, 550, 313}, 35, kMuted, false, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    Text(dc, L"AVAILABLE DEVICES", {36, 398, 340, 429}, 17, kWhite, true);
    Text(dc, L"D-pad / arrows: choose side    A / Enter: ready    B / Esc: leave side", {350, 398, 1004, 429}, 13, kMuted,
         false, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    for (int device = -1; device < 4; ++device) {
        int y = 440 + (device + 1) * 34;
        const bool connected = setup.Available(device);
        Box(dc, {36, y, 1004, y + 30}, RGB(17, 27, 40));
        Text(dc, DeviceName(device), {50, y, 330, y + 30}, 14, connected ? kWhite : kMuted, true);
        Text(dc, connected ? (setup.Owner(device) < 0 ? L"UNASSIGNED" : L"ASSIGNED TO PLAYER " + std::to_wstring(setup.Owner(device) + 1)) : L"NOT CONNECTED",
             {325, y, 615, y + 30}, 12, connected ? kMint : kMuted);
    }
    const std::wstring status = session->Status().empty() ? L"Assign two different devices and ready both players. Xbox-compatible controllers supported." : session->Status();
    Text(dc, status, {36, 666, 1004, 704}, 14, kMuted, false, DT_LEFT | DT_WORDBREAK);
    Text(dc, L"SPACE / START  Play     L  Layout     S  Save slot", {36, 718, 560, 742}, 13, kMuted);
    Text(dc, L"EXPERIMENTAL  /  GTA SA 1.0 US", {600, 718, 1004, 742}, 12, kMuted, false, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    SetMapMode(dc, MM_TEXT);
    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
}
void AddControl(int id, const std::wstring& caption, int x, int y, int width, int height) {
    const DWORD disabled = id == kStart || id == kReady1 || id == kReady2 ? WS_DISABLED : 0;
    HWND control = CreateWindowExW(0, L"BUTTON", caption.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | disabled,
                                  x, y, width, height, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    controls.push_back({control, id, x, y, width, height});
}
void Resize() {
    RECT rect{}; GetClientRect(window, &rect);
    if (buttonFont) DeleteObject(buttonFont);
    buttonFont = CreateFontW(-std::max<int>(11, 14 * rect.bottom / kHeight), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    for (const auto& c : controls) {
        MoveWindow(c.hwnd, c.x * rect.right / kWidth, c.y * rect.bottom / kHeight, c.w * rect.right / kWidth, c.h * rect.bottom / kHeight, TRUE);
        SendMessageW(c.hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(buttonFont), TRUE);
    }
}
void Refresh() {
    const bool active = session->Active();
    for (int i = 0; i < 2; ++i) {
        Label(kReady1 + i, setup.seats[i].ready ? L"READY - click to cancel" : L"READY UP");
        EnableWindow(ControlFor(kReady1 + i), !active && setup.Available(setup.seats[i].device));
    }
    Label(kLayout, setup.horizontal ? L"LAYOUT: TOP / BOTTOM" : L"LAYOUT: LEFT / RIGHT");
    Label(kSave, selectedSave < 0 ? L"STORY: NEW GAME" : L"STORY: SAVE SLOT " + std::to_wstring(selectedSave));
    Label(kStart, active ? L"END SESSION" : L"PLAY LOCAL CO-OP");
    EnableWindow(ControlFor(kStart), active || setup.CanStart());
    EnableWindow(ControlFor(kLayout), !active); EnableWindow(ControlFor(kSave), !active);
    for (int d = -1; d < 4; ++d) for (int side = 0; side < 2; ++side) {
        int id = kDevice + (d + 1) * 2 + side;
        EnableWindow(ControlFor(id), !active && setup.Available(d));
        Label(id, setup.Owner(d) == side ? L"ASSIGNED" : side ? L"PLAYER 2 >" : L"< PLAYER 1");
    }
    InvalidateRect(window, nullptr, FALSE);
}
void CycleSave() {
    for (int next = selectedSave < 0 ? 1 : selectedSave + 1; next <= 8; ++next)
        if (session->SaveExists(next)) { selectedSave = next; return; }
    selectedSave = -1;
}
void StartOrStop() {
    if (session->Active()) {
        if (MessageBoxW(window, L"End both games? Save at a safe house first; unsaved progress will be lost.", L"End local co-op", MB_YESNO | MB_ICONQUESTION) == IDYES)
            session->Stop();
    } else {
        // Validate freshly polled connections again at the action boundary.
        for (int i = 0; i < 4; ++i) { XINPUT_STATE input{}; setup.Connect(i, local::ReadPad(i, input) == ERROR_SUCCESS); }
        if (setup.CanStart() && !session->Start(setup, selectedSave))
            MessageBoxW(window, session->Status().c_str(), L"Could not start local co-op", MB_ICONERROR);
    }
    Refresh();
}
void Action(int id) {
    if (id == kStart) { StartOrStop(); return; }
    if (id == kData) {
        EnsureDirectory(session->Directory());
        ShellExecuteW(window, L"open", session->Directory().c_str(), nullptr, nullptr, SW_SHOWNORMAL); return;
    }
    if (session->Active()) return;
    if (id == kLayout) setup.horizontal = !setup.horizontal;
    else if (id == kSave) CycleSave();
    else if (id == kReady1 || id == kReady2) setup.ToggleReady(setup.seats[id - kReady1].device);
    else if (id >= kDevice && id < kDevice + 10) setup.Assign((id - kDevice) / 2 - 1, (id - kDevice) % 2);
    Refresh();
}
void Poll() {
    const bool focused = GetForegroundWindow() == window;
    for (int i = 0; i < 4; ++i) {
        XINPUT_STATE input{};
        const bool connected = local::ReadPad(i, input) == ERROR_SUCCESS;
        if (!session->Active()) setup.Connect(i, connected);
        else setup.connected[i] = connected;
        WORD buttons = input.Gamepad.wButtons;
        if (input.Gamepad.sThumbLX < -18000) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
        if (input.Gamepad.sThumbLX > 18000) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        const WORD pressed = buttons & ~lastButtons[i]; lastButtons[i] = buttons;
        if (!focused || session->Active()) continue;
        if (pressed & XINPUT_GAMEPAD_DPAD_LEFT) setup.Assign(i, 0);
        if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) setup.Assign(i, 1);
        if (pressed & XINPUT_GAMEPAD_A) setup.ToggleReady(i);
        if (pressed & XINPUT_GAMEPAD_B) setup.Release(i);
        if ((pressed & XINPUT_GAMEPAD_START) && setup.CanStart()) StartOrStop();
    }
    session->Tick();
    if (session->TakeFocusRequest()) {
        HWND game = session->KeyboardWindow();
        // Never steal focus if the user has switched to another application during startup.
        if (focused || GetPropW(GetForegroundWindow(), L"SA_COOP_LOCAL_SESSION") == reinterpret_cast<HANDLE>(static_cast<uintptr_t>(GetCurrentProcessId()))) {
            ShowWindow(window, SW_MINIMIZE);
            if (game) SetForegroundWindow(game);
        }
    }
    if (wasActive && !session->Active()) ShowWindow(window, SW_RESTORE);
    wasActive = session->Active();
    Refresh();
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        window = hwnd;
        AddControl(kReady1, L"READY UP", 203, 330, 265, 30);
        AddControl(kReady2, L"READY UP", 717, 330, 265, 30);
        for (int d = -1; d < 4; ++d) {
            AddControl(kDevice + (d + 1) * 2, L"< PLAYER 1", 678, 440 + (d + 1) * 34, 148, 30);
            AddControl(kDevice + (d + 1) * 2 + 1, L"PLAYER 2 >", 834, 440 + (d + 1) * 34, 158, 30);
        }
        AddControl(kLayout, L"LAYOUT", 36, 623, 252, 36);
        AddControl(kSave, L"STORY", 300, 623, 220, 36);
        AddControl(kData, L"SAVES / LOGS", 532, 623, 170, 36);
        AddControl(kStart, L"PLAY LOCAL CO-OP", 714, 623, 290, 36);
        Resize(); Refresh(); SetTimer(hwnd, 1, 100, nullptr); return 0;
    case WM_COMMAND: if (HIWORD(wp) == BN_CLICKED) Action(LOWORD(wp)); return 0;
    case WM_TIMER: Poll(); return 0;
    case WM_SIZE: if (wp != SIZE_MINIMIZED) Resize(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd, &paint); Paint(dc); EndPaint(hwnd, &paint); return 0; }
    case WM_DRAWITEM: {
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        const bool enabled = !(item.itemState & ODS_DISABLED), pressed = item.itemState & ODS_SELECTED;
        bool accent = item.CtlID == kStart && enabled;
        if (item.CtlID >= kDevice && item.CtlID < kDevice + 10)
            accent = setup.Owner((item.CtlID - kDevice) / 2 - 1) == static_cast<int>((item.CtlID - kDevice) % 2);
        Box(item.hDC, item.rcItem, accent ? kMint : pressed ? RGB(60, 80, 99) : RGB(36, 51, 69));
        wchar_t caption[256]{}; GetWindowTextW(item.hwndItem, caption, 256);
        auto oldFont = SelectObject(item.hDC, buttonFont); SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, !enabled ? RGB(98, 115, 134) : accent ? kBg : kWhite);
        RECT text = item.rcItem; DrawTextW(item.hDC, caption, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item.hDC, oldFont);
        if (item.itemState & ODS_FOCUS) { RECT focus = item.rcItem; InflateRect(&focus, -3, -3); DrawFocusRect(item.hDC, &focus); }
        return TRUE;
    }
    case WM_CLOSE:
        if (session->Active() && MessageBoxW(hwnd, L"Close both games? Unsaved progress will be lost.", L"Close local co-op", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
        session->Stop(); DestroyWindow(hwnd); return 0;
    case WM_DESTROY: KillTimer(hwnd, 1); PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd, message, wp, lp);
    }
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    WSADATA network{};
    if (WSAStartup(MAKEWORD(2, 2), &network)) return 1;
    session = std::make_unique<coop::local::Session>(coop::ModuleDir());
    const std::wstring mutexName = L"Local\\SA_COOP_LAUNCHER_" + session->Directory().substr(session->Directory().find_last_of(L'-') + 1);
    // A stable per-install mutex prevents a second launcher from rewriting live player configs.
    std::wstring safeMutex = mutexName; if (!safeMutex.empty() && safeMutex.back() == L'\\') safeMutex.pop_back();
    HANDLE single = CreateMutexW(nullptr, TRUE, safeMutex.c_str());
    if (!single || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"The local co-op launcher is already open for this game folder.", L"SA:COOP", MB_ICONINFORMATION);
        if (single) CloseHandle(single); WSACleanup(); return 1;
    }
    WNDCLASSW wc{}; wc.hInstance = instance; wc.lpszClassName = L"SA_COOP_LOCAL_SETUP"; wc.lpfnWndProc = WindowProc;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); wc.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&wc);
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT rect{0, 0, kWidth, kHeight}; AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    const int width = std::min<int>(rect.right - rect.left, work.right - work.left);
    const int height = std::min<int>(rect.bottom - rect.top, work.bottom - work.top);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"SA:COOP - Local Play", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                               work.left + (work.right - work.left - width) / 2, work.top + (work.bottom - work.top - height) / 2,
                               width, height, nullptr, nullptr, instance, nullptr);
    if (!hwnd) { CloseHandle(single); WSACleanup(); return 1; }
    ShowWindow(hwnd, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_KEYDOWN && !session->Active() && !(message.lParam & (1 << 30))) {
            bool handled = true;
            switch (message.wParam) {
            case VK_LEFT: setup.Assign(local::kKeyboard, 0); break;
            case VK_RIGHT: setup.Assign(local::kKeyboard, 1); break;
            case VK_RETURN: setup.ToggleReady(local::kKeyboard); break;
            case VK_ESCAPE: setup.Release(local::kKeyboard); break;
            case VK_SPACE: if (GetFocus() == window) StartOrStop(); else handled = false; break;
            case 'L': setup.horizontal = !setup.horizontal; break;
            case 'S': CycleSave(); break;
            default: handled = false; break;
            }
            if (handled) { Refresh(); continue; }
        }
        if (!IsDialogMessageW(hwnd, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    session.reset(); if (buttonFont) DeleteObject(buttonFont);
    ReleaseMutex(single); CloseHandle(single); WSACleanup();
    return 0;
}
