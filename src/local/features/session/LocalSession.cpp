#include <winsock2.h>
#include "features/session/LocalSession.h"
#include <shlobj.h>
#include <algorithm>
#include <vector>
#include "common/lib/ini/Ini.h"
#include "common/lib/util/Util.h"
#include "common/lib/local-coop/WindowHost.h"

namespace coop::local {
namespace {
std::wstring SaveFile(const std::wstring& dir, int slot) {
    return dir + L"GTASAsf" + std::to_wstring(slot) + L".b";
}
struct WindowSearch { DWORD pid; HWND hwnd = nullptr; };
BOOL CALLBACK FindWindowForProcess(HWND hwnd, LPARAM param) {
    auto& found = *reinterpret_cast<WindowSearch*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == found.pid && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER) &&
        GetPropW(hwnd, L"SA_COOP_LOCAL_SESSION")) { found.hwnd = hwnd; return FALSE; }
    return TRUE;
}
HWND Window(DWORD pid, HWND container) {
    WindowSearch found{pid};
    if (container) EnumChildWindows(container, FindWindowForProcess, reinterpret_cast<LPARAM>(&found));
    return found.hwnd;
}
bool Alive(HANDLE process) { return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT; }
unsigned FreePort() {
    // Ask Windows for a loopback UDP port. The host still reports a bind race as a startup failure.
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) return 0;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int length = sizeof(address);
    unsigned port = 0;
    if (bind(socket, reinterpret_cast<sockaddr*>(&address), length) == 0 &&
        getsockname(socket, reinterpret_cast<sockaddr*>(&address), &length) == 0) port = ntohs(address.sin_port);
    closesocket(socket);
    return port;
}
}
Session::Session(std::wstring dir) : gameDir(std::move(dir)) {
    wchar_t appData[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
        // Separate installs should not silently share a campaign. The suffix is stable across launches.
        const std::string identity = ToLower(WideToUtf8(gameDir));
        dataDir = std::wstring(appData) + L"\\SA-COOP\\local-" +
                  std::to_wstring(Crc32(reinterpret_cast<const uint8_t*>(identity.data()), identity.size())) + L"\\";
    }
}
Session::~Session() { Stop(); }
bool Session::CreateContainer() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = ContainerProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kContainerClass;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    container = CreateWindowExW(WS_EX_APPWINDOW, kContainerClass, L"SA:COOP - Local split-screen",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, this);
    if (!container) return false;
    SetPropW(container, kSessionProperty, reinterpret_cast<HANDLE>(static_cast<uintptr_t>(GetCurrentProcessId())));
    return true;
}
void Session::FocusKeyboard() {
    if (ContainerFocused(container))
        if (HWND player = KeyboardWindow()) PostMessageW(player, kFocusPlayer, 0, 0);
}
void Session::NotifyActivation(bool active) {
    for (DWORD pid : pids)
        if (HWND player = Window(pid, container)) PostMessageW(player, kSessionActivation, active, 0);
}
void Session::Resume() {
    if (!Active() || !container) return;
    if (controls) ShowWindow(controls, SW_HIDE);
    ShowWindow(container, SW_RESTORE);
    SetForegroundWindow(container);
    NotifyActivation(ContainerFocused(container));
    FocusKeyboard();
}
void Session::ShowControls() {
    if (!controls) return;
    ShowWindow(container, SW_MINIMIZE);
    ShowWindow(controls, SW_RESTORE);
    SetForegroundWindow(controls);
}
LRESULT CALLBACK Session::ContainerProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<Session*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<Session*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(window, message, wp, lp);
    switch (message) {
    case WM_ACTIVATE:
        self->NotifyActivation(LOWORD(wp) != WA_INACTIVE && !HIWORD(wp));
        if (LOWORD(wp) != WA_INACTIVE) PostMessageW(window, kFocusKeyboard, 0, 0);
        return 0;
    case WM_SETFOCUS: case kFocusKeyboard: self->FocusKeyboard(); return 0;
    case kSessionControls: self->ShowControls(); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (wp == VK_F10) { if (!(lp & (1L << 30))) self->ShowControls(); return 0; }
        break;
    case WM_CLOSE:
        if (!self->closing && !self->stopping) {
            self->closing = true;
            if (MessageBoxW(window, L"End both games? Save first; unsaved progress will be lost.",
                            L"End local co-op", MB_YESNO | MB_ICONQUESTION) == IDYES) self->Stop();
            self->closing = false;
            self->FocusKeyboard();
        }
        return 0;
    case WM_DISPLAYCHANGE:
        // Keep the render targets at their startup resolution. Reopening a session reconfigures both games.
        self->ShowControls();
        return 0;
    case WM_NCDESTROY:
        RemovePropW(window, kSessionProperty);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProcW(window, message, wp, lp);
}
bool Session::SaveExists(int slot) const {
    if (FileExists(SaveFile(dataDir + L"player1\\documents\\GTA San Andreas User Files\\", slot))) return true;
    wchar_t docs[MAX_PATH]{};
    return SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs)) &&
           FileExists(SaveFile(std::wstring(docs) + L"\\GTA San Andreas User Files\\", slot));
}
bool Session::WriteConfigs(const Setup& setup, int slot, unsigned port) {
    const auto views = Split(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), setup.horizontal);
    for (int i = 0; i < 2; ++i) {
        const auto& view = views[i];
        if (view.width < 640 || view.height < 360) { status = L"This layout needs at least 640 x 360 per player."; return false; }
        const std::wstring dir = dataDir + L"player" + std::to_wstring(i + 1) + L"\\";
        EnsureDirectory(dir);
        const std::wstring docs = dir + L"documents\\GTA San Andreas User Files\\";
        EnsureDirectory(docs);
        if (i == 0 && slot > 0 && !FileExists(SaveFile(docs, slot))) {
            wchar_t original[MAX_PATH]{};
            if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, original)) ||
                !CopyFileW(SaveFile(std::wstring(original) + L"\\GTA San Andreas User Files\\", slot).c_str(),
                           SaveFile(docs, slot).c_str(), TRUE)) {
                status = L"Could not copy the selected save to the local co-op profile."; return false;
            }
        }
        // Start afresh to avoid inheriting online destinations or input choices from a previous session.
        Ini ini;
        ini.Set("player", "name", "Player" + std::to_string(i + 1));
        ini.Set("player", "language", "en");
        ini.SetInt("connect", "auto_connect", 1);
        ini.Set("connect", "mode", i == 0 ? "host" : "join");
        ini.Set("connect", "host", "127.0.0.1");
        ini.SetInt("connect", "port", port);
        ini.SetInt("connect", "load_slot", i == 0 ? slot : 0);
        ini.SetInt("connect", "max_players", 2);
        ini.SetInt("game", "windowed", 1);
        ini.SetInt("game", "windowed_strict", 1);
        ini.SetInt("game", "borderless", 1);
        ini.SetInt("game", "multi_instance", 1);
        ini.SetInt("game", "run_in_background", 1);
        ini.SetInt("game", "window_x", view.x);
        ini.SetInt("game", "window_y", view.y);
        ini.SetInt("game", "width", view.width);
        ini.SetInt("game", "height", view.height);
        ini.Set("game", "window_title", "SA:COOP - Player " + std::to_string(i + 1));
        ini.Set("game", "user_dir", WideToUtf8(dir + L"documents"));
        ini.SetInt("voice", "enabled", 0);
        ini.SetInt("debug", "firewall_prompt", 0);
        ini.SetInt("local", "enabled", 1);
        ini.Set("local", "input", setup.seats[i].device == kKeyboard ? "keyboard" : "gamepad");
        ini.SetInt("local", "controller", setup.seats[i].device);
        ini.SetInt("local", "session", GetCurrentProcessId());
        ini.SetInt("local", "container", static_cast<int>(reinterpret_cast<uintptr_t>(container)));
        ini.Set("local", "data_dir", WideToUtf8(dir));
        eventNames[i] = L"Local\\SA_COOP_READY_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(i);
        ini.Set("local", "ready_event", WideToUtf8(eventNames[i]));
        ready[i] = CreateEventW(nullptr, TRUE, FALSE, eventNames[i].c_str());
        configs[i] = dir + L"local.ini";
        if (!ready[i] || !ini.Save(configs[i])) { status = L"Could not write the local player config."; return false; }
    }
    return true;
}
bool Session::Launch(int player) {
    // Pass a private child environment; do not mutate the launcher's or user's SACOOP_CONFIG.
    LPWCH environment = GetEnvironmentStringsW();
    if (!environment) return false;
    std::vector<std::wstring> entries;
    for (const wchar_t* p = environment; *p; p += wcslen(p) + 1)
        if (_wcsnicmp(p, L"SACOOP_CONFIG=", 14) != 0) entries.emplace_back(p);
    FreeEnvironmentStringsW(environment);
    entries.push_back(L"SACOOP_CONFIG=" + configs[player]);
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    std::vector<wchar_t> block;
    for (const auto& entry : entries) { block.insert(block.end(), entry.begin(), entry.end()); block.push_back(0); }
    block.push_back(0);
    std::wstring exe = gameDir + L"gta_sa.exe", command = L"\"" + exe + L"\"";
    // Assign at creation, before any game code can spawn its server. The non-inherited job
    // handle belongs only to the launcher: even a launcher crash closes the entire process tree.
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<uint8_t> storage(bytes);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes)) return false;
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job, sizeof(job), nullptr, nullptr)) {
        const DWORD error = GetLastError();
        DeleteProcThreadAttributeList(attributes);
        SetLastError(error);
        return false;
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                                       CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                                       block.data(), gameDir.c_str(), &startup.StartupInfo, &process);
    const DWORD error = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    if (!created) { SetLastError(error); return false; }
    CloseHandle(process.hThread);
    processes[player] = process.hProcess;
    pids[player] = process.dwProcessId;
    return true;
}
bool Session::Start(const Setup& setup, int slot) {
    if (Active() || !setup.CanStart()) return false;
    if (dataDir.empty()) { status = L"Windows did not provide a writable local app-data folder."; return false; }
    if (!FileExists(gameDir + L"gta_sa.exe") || !FileExists(gameDir + L"coop.asi") ||
        !FileExists(gameDir + L"coop\\coop-server.exe")) {
        status = L"Put coop-local.exe beside gta_sa.exe and install this build of SA:COOP first."; return false;
    }
    if (slot > 0 && !SaveExists(slot)) { status = L"That save slot is empty. Choose another slot or New game."; return false; }
    const unsigned port = FreePort();
    if (!port) { status = L"Could not allocate a local UDP port."; return false; }
    if (!CreateContainer()) { status = L"Could not create the fullscreen session window."; return false; }
    if (!WriteConfigs(setup, slot, port)) { Fail(status); return false; }
    job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        Fail(L"Could not create the local session process group. Windows error " + std::to_wstring(GetLastError()));
        return false;
    }
    keyboardSeat = setup.seats[1].device == kKeyboard ? 1 : 0;
    if (!Launch(0)) { Fail(L"Could not start Player 1. Windows error " + std::to_wstring(GetLastError())); return false; }
    phase = Phase::Host;
    ShowWindow(container, SW_SHOWNOACTIVATE);
    deadline = GetTickCount64() + 120000;
    status = L"Starting Player 1 and the local server...";
    return true;
}
void Session::Tick() {
    if (!Active()) return;
    if (!Alive(processes[0]) || ((phase == Phase::Partner || phase == Phase::Running) && !Alive(processes[1]))) {
        Fail(L"A game closed. The local session has ended; see the player logs if it was unexpected."); return;
    }
    if (phase == Phase::Running) return;
    if (GetTickCount64() > deadline) { Fail(L"Startup timed out. Check player1/local.log and player2/local.log in the local data folder."); return; }
    if (phase == Phase::Host && WaitForSingleObject(ready[0], 0) == WAIT_OBJECT_0) {
        if (!Launch(1)) { Fail(L"Could not start Player 2. Windows error " + std::to_wstring(GetLastError())); return; }
        phase = Phase::Partner;
        deadline = GetTickCount64() + 120000;
        status = L"Player 1 is online. Connecting Player 2...";
    } else if (phase == Phase::Partner && WaitForSingleObject(ready[1], 0) == WAIT_OBJECT_0 &&
               Window(pids[0], container) && Window(pids[1], container)) {
        phase = Phase::Running;
        status = L"Local co-op is running. F10 opens session controls; Alt+F4 ends both games.";
        focusRequested = true;
    }
}
bool Session::TakeFocusRequest() { const bool value = focusRequested; focusRequested = false; return value; }
HWND Session::KeyboardWindow() const { return Window(pids[keyboardSeat], container); }
void Session::Fail(std::wstring message) { Stop(); status = message; }
void Session::Stop() {
    if (stopping) return;
    stopping = true;
    if (Active()) status = L"Session ended. Choose devices to play again.";
    phase = Phase::Idle;
    for (int i = 0; i < 2; ++i) if (Alive(processes[i]))
        if (HWND window = Window(pids[i], container)) PostMessageW(window, WM_CLOSE, 1, 0);
    for (auto& process : processes) if (process) {
        const auto until = GetTickCount64() + 3000;
        // Reparented windows share input queues. Pump messages while they shut down instead of
        // blocking a child that is sending a synchronous window-destruction notification to us.
        while (Alive(process) && GetTickCount64() < until) {
            MsgWaitForMultipleObjects(1, &process, FALSE, 25, QS_ALLINPUT);
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { PostQuitMessage(static_cast<int>(message.wParam)); break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
        if (Alive(process)) TerminateProcess(process, 0);
        CloseHandle(process); process = nullptr;
    }
    for (auto& event : ready) if (event) { CloseHandle(event); event = nullptr; }
    if (job) { CloseHandle(job); job = nullptr; }
    pids = {};
    phase = Phase::Idle;
    focusRequested = false;
    if (container) { DestroyWindow(container); container = nullptr; }
    if (controls) ShowWindow(controls, SW_RESTORE);
    stopping = false;
}
}  // namespace coop::local
