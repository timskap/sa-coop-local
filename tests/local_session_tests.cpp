// Real Win32 process/environment/event tests. This executable doubles as a fake GTA/server;
// no game code or assets are loaded. The Python runner owns the temporary installation.
#include <winsock2.h>
#include <windows.h>
#include <dinput.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "features/session/LocalSession.h"
#include "common/lib/ini/Ini.h"
#include "common/lib/util/Util.h"
#include "common/lib/local-coop/WindowHost.h"
#include "common/lib/local-coop/InputWindow.h"

namespace fs = std::filesystem;
using namespace coop;
void Check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
void Write(const fs::path& path, const std::string& text) { std::ofstream(path) << text; }
DWORD Pid(const fs::path& path) { DWORD pid = 0; std::ifstream(path) >> pid; return pid; }
constexpr UINT kLoseMouse = WM_APP + 0x500;
constexpr UINT kCinematic = WM_APP + 0x501;
IDirectInputDevice8W* fakeMouse = nullptr;
local::Viewport fakeSplit{};
uintptr_t Property(HWND window, const wchar_t* name) { return reinterpret_cast<uintptr_t>(GetPropW(window, name)); }
void Property(HWND window, const wchar_t* name, uintptr_t value) { SetPropW(window, name, reinterpret_cast<HANDLE>(value)); }
void Pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
}
template<class F> void Until(F done, const char* message) {
    const auto deadline = GetTickCount64() + 15000;
    while (!done()) { Check(GetTickCount64() < deadline, message); Pump(); Sleep(20); }
}
LRESULT CALLBACK FakeWindow(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == kCinematic) {
        const HWND parent = GetParent(h);
        DWORD session = 0; GetWindowThreadProcessId(parent, &session);
        Check(local::PresentPlayer(h, parent, session, fakeSplit, w != 0), "Cinematic layout failed");
        return 0;
    }
    if (m == WM_SIZE) Property(h, L"TEST_RESIZES", Property(h, L"TEST_RESIZES") + 1);
    if (m == kLoseMouse) {
        Check(fakeMouse && SUCCEEDED(fakeMouse->Unacquire()), "Could not simulate lost mouse acquisition");
        DIMOUSESTATE2 state{};
        Check(FAILED(fakeMouse->GetDeviceState(sizeof(state), &state)), "Unacquired mouse still returns input");
        return 0;
    }
    if (m == local::kFocusPlayer) {
        Property(h, L"TEST_FOCUS_REQUESTS", Property(h, L"TEST_FOCUS_REQUESTS") + 1);
        const auto drop = Property(h, L"TEST_DROP_FOCUS");
        if (drop) { Property(h, L"TEST_DROP_FOCUS", drop - 1); return 0; }
    }
    LRESULT result = 0;
    if (local::PlayerWindowMessage(h, GetParent(h), m, w, l, result)) {
        if (m == local::kFocusPlayer && local::PlayerFocused(h, GetParent(h))) {
            Check(SUCCEEDED(local::RecoverMouse(fakeMouse)), "Mouse did not recover after window restoration");
            Property(h, L"TEST_MOUSE_RECOVERED", Property(h, L"TEST_MOUSE_RECOVERED") + 1);
        }
        return result;
    }
    if (m == WM_ACTIVATE) { SetPropW(h, L"TEST_ACTIVATED", reinterpret_cast<HANDLE>(static_cast<uintptr_t>(w))); return 0; }
    if (m == WM_DESTROY) {
        if (fakeMouse) { fakeMouse->Release(); fakeMouse = nullptr; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
int FakeGame(const std::wstring& root) {
    SetProcessDPIAware();
    wchar_t config[MAX_PATH]{};
    Check(GetEnvironmentVariableW(L"SACOOP_CONFIG", config, MAX_PATH) > 0, "Missing child config");
    unsigned configs = 0;
    LPWCH environment = GetEnvironmentStringsW();
    for (const wchar_t* p = environment; *p; p += wcslen(p) + 1)
        if (_wcsnicmp(p, L"SACOOP_CONFIG=", 14) == 0) ++configs;
    FreeEnvironmentStringsW(environment);
    Check(configs == 1, "Duplicate SACOOP_CONFIG in child environment");
    Ini ini;
    Check(ini.Load(config), "Could not read private child config");
    const bool host = ini.Get("connect", "mode", "") == "host";
    const auto dir = Utf8ToWide(ini.Get("local", "data_dir", ""));
    Check(fs::path(config).parent_path() == fs::path(dir).parent_path(), "Config isolation failed");
    if (host) {
        // Exercise the same nested server job arrangement as the real client.
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        Check(job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info)), "Server job failed");
        std::wstring command = L"\"" + root + L"coop\\coop-server.exe\"";
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION server{};
        Check(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                             root.c_str(), &startup, &server), "Fake server start failed");
        Check(AssignProcessToJobObject(job, server.hProcess), "Nested server job assignment failed");
        Write(fs::path(root) / "server.pid", std::to_string(server.dwProcessId));
        CloseHandle(server.hThread); CloseHandle(server.hProcess);
        // Intentionally retain the job for the process lifetime, as the real host does.
    }
    WNDCLASSW wc{}; wc.lpfnWndProc = FakeWindow; wc.lpszClassName = L"SA_COOP_TEST_GAME";
    wc.hInstance = GetModuleHandleW(nullptr);
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Fake GTA", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                0, 0, 640, 360, nullptr, nullptr, wc.hInstance, nullptr);
    Check(window != nullptr, "Fake game window failed");
    SetPropW(window, L"SA_COOP_LOCAL_SESSION", reinterpret_cast<HANDLE>(static_cast<uintptr_t>(ini.GetInt("local", "session", 0))));
    HWND parent = reinterpret_cast<HWND>(static_cast<uintptr_t>(static_cast<unsigned>(ini.GetInt("local", "container", 0))));
    fakeSplit = {ini.GetInt("game", "window_x", 0), ini.GetInt("game", "window_y", 0),
                 ini.GetInt("game", "width", 0), ini.GetInt("game", "height", 0)};
    Check(local::EmbedPlayer(window, parent, ini.GetInt("local", "session", 0),
                            ini.GetInt("game", "window_x", 0), ini.GetInt("game", "window_y", 0),
                            ini.GetInt("game", "width", 0), ini.GetInt("game", "height", 0)), "Game embedding failed");
    Check(GetAncestor(window, GA_ROOT) == parent && !(GetWindowLongW(window, GWL_STYLE) & WS_POPUP), "Game is still top-level");
    // Actual DirectInput contract: child HWNDs cannot be used as cooperative owners.
    IDirectInput8W* input = nullptr;
    Check(SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                     reinterpret_cast<void**>(&input), nullptr)), "DirectInput factory failed");
    IDirectInputDevice8W* mouse = nullptr;
    Check(SUCCEEDED(input->CreateDevice(GUID_SysMouse, &mouse, nullptr)), "DirectInput mouse creation failed");
    Check(SUCCEEDED(mouse->SetDataFormat(&c_dfDIMouse2)), "Mouse data format failed");
    HWND inputOwner = local::BackgroundInputWindow();
    Check(inputOwner && !GetParent(inputOwner) && !IsWindowVisible(inputOwner), "Input owner must be hidden and top-level");
    Check(SUCCEEDED(mouse->SetCooperativeLevel(inputOwner, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)), "Embedded mouse cooperative level failed");
    fakeMouse = mouse;
    Check(SUCCEEDED(local::RecoverMouse(fakeMouse)), "Initial mouse acquisition failed");
    input->Release();
    Write(fs::path(root) / (host ? "p1.pid" : "p2.pid"), std::to_string(GetCurrentProcessId()));
    const auto eventName = Utf8ToWide(ini.Get("local", "ready_event", ""));
    bool signaled = false;
    for (;;) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return 0;
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (!signaled && (!host || fs::exists(fs::path(root) / "allow-host"))) {
            HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
            Check(ready && SetEvent(ready), "Host/partner readiness event failed");
            CloseHandle(ready); signaled = true;
        }
        Sleep(10);
    }
}
int wmain(int argc, wchar_t** argv) {
    try {
        const auto root = ModuleDir();
        const auto name = fs::path(argv[0]).filename().wstring();
        if (name == L"coop-server.exe") { Sleep(INFINITE); return 0; }
        if (name == L"gta_sa.exe") return FakeGame(root);
        SetProcessDPIAware();
        WSADATA network{};
        Check(!WSAStartup(MAKEWORD(2, 2), &network), "WSAStartup failed");
        local::Session session(root);
        HWND controls = CreateWindowW(L"STATIC", L"Session test controls", WS_OVERLAPPEDWINDOW,
                                      0, 0, 640, 360, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        session.SetControlsWindow(controls);
        Write(fs::path(root) / "profile.txt", WideToUtf8(session.Directory()));
        local::Setup setup;
        setup.Connect(0, true); setup.Assign(0, 0); setup.Assign(local::kKeyboard, 1);
        setup.ToggleReady(0); setup.ToggleReady(local::kKeyboard);
        SetEnvironmentVariableW(L"SACOOP_CONFIG", L"parent-config-must-not-be-used");
        fs::rename(fs::path(root) / "coop.asi", fs::path(root) / "coop.disabled");
        Check(!session.Start(setup, -1) && !session.Active(), "Missing mod must reject start");
        fs::rename(fs::path(root) / "coop.disabled", fs::path(root) / "coop.asi");
        if (!session.SaveExists(8)) Check(!session.Start(setup, 8) && !session.Active(), "Empty save must reject start");
        fs::rename(fs::path(root) / "gta_sa.exe", fs::path(root) / "gta_sa.saved");
        Write(fs::path(root) / "gta_sa.exe", "not an executable");
        Check(!session.Start(setup, -1) && !session.Active(), "Invalid game executable must reject start");
        fs::remove(fs::path(root) / "gta_sa.exe");
        fs::rename(fs::path(root) / "gta_sa.saved", fs::path(root) / "gta_sa.exe");
        const bool crash = argc > 1 && std::wstring(argv[1]) == L"crash";
        for (int attempt = 0; attempt < (crash ? 1 : 2); ++attempt) {
            if (attempt == 1) {
                setup.Assign(local::kKeyboard, 0); setup.Assign(0, 1);
                setup.ToggleReady(local::kKeyboard); setup.ToggleReady(0);
            }
            setup.horizontal = attempt == 0;
            for (const char* file : {"p1.pid", "p2.pid", "server.pid", "allow-host"}) fs::remove(fs::path(root) / file);
            const auto requested = local::Split(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), setup.horizontal);
            if (requested[0].width < 640 || requested[0].height < 360) {
                Check(!session.Start(setup, -1) && !session.Active() && !session.ContainerWindow(),
                      "Unsupported monitor size must reject the layout and clean up its container");
                std::cout << "Display is too small for vertical GTA viewports; rejection checked, restarting horizontally\n";
                setup.horizontal = true;
            }
            if (!session.Start(setup, -1)) throw std::runtime_error("Session start failed: " + WideToUtf8(session.Status()));
            Until([&] { session.Tick(); return Pid(fs::path(root) / "p1.pid") != 0; }, "P1 never started");
            for (int i = 0; i < 15; ++i) { session.Tick(); Pump(); Sleep(20); }
            Check(!fs::exists(fs::path(root) / "p2.pid"), "P2 started before host readiness (or reused stale event)");
            Ini one, two;
            Check(one.Load(session.Directory() + L"player1\\local.ini") && two.Load(session.Directory() + L"player2\\local.ini"), "Missing profiles");
            Check(one.Get("local", "input", "") == (attempt ? "keyboard" : "gamepad") &&
                  two.Get("local", "input", "") == (attempt ? "gamepad" : "keyboard"), "Seat mapping changed");
            Check(one.Get("game", "user_dir", "") != two.Get("game", "user_dir", ""), "Shared save directory");
            Check(one.GetInt("connect", "port", 0) == two.GetInt("connect", "port", -1), "Mismatched server ports");
            Write(fs::path(root) / "allow-host", "ready");
            Until([&] { session.Tick(); return session.Running(); }, "Session did not reach Running");
            Check(session.KeyboardWindow() != nullptr && session.TakeFocusRequest() && !session.TakeFocusRequest(), "Focus handoff failed");
            HWND container = session.ContainerWindow();
            Check(local::IsContainer(container, GetCurrentProcessId()), "Missing authenticated container");
            Check(!local::IsContainer(container, GetCurrentProcessId() + 1), "Accepted a foreign session");
            RECT bounds{}; GetWindowRect(container, &bounds);
            Check(bounds.left == 0 && bounds.top == 0 && bounds.right == GetSystemMetrics(SM_CXSCREEN) &&
                  bounds.bottom == GetSystemMetrics(SM_CYSCREEN), "Container must cover the whole primary display");
            const auto views = local::Split(bounds.right, bounds.bottom, setup.horizontal);
            int children = 0;
            for (HWND child = GetWindow(container, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
                DWORD pid = 0; GetWindowThreadProcessId(child, &pid);
                int seat = pid == Pid(fs::path(root) / "p1.pid") ? 0 : 1;
                RECT rect{}; GetWindowRect(child, &rect);
                MapWindowPoints(nullptr, container, reinterpret_cast<POINT*>(&rect), 2);
                Check(rect.left == views[seat].x && rect.top == views[seat].y &&
                      rect.right - rect.left == views[seat].width && rect.bottom - rect.top == views[seat].height,
                      "Embedded viewport has a gap, frame or overlap");
                Check((GetWindowLongW(child, GWL_STYLE) & WS_CHILD) && !(GetWindowLongW(child, GWL_EXSTYLE) & WS_EX_APPWINDOW),
                      "Child can create a separate taskbar entry");
                ++children;
            }
            Check(children == 2, "Both players must be inside one window");
            session.Resume();
            Until([&] { session.Tick(); return local::PlayerFocused(session.KeyboardWindow(), container); }, "Keyboard focus did not enter its child window");
            Until([&] {
                for (HWND child = GetWindow(container, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
                    if (!GetPropW(child, L"TEST_ACTIVATED")) return false;
                return true;
            }, "Native activation was not forwarded to both games");
            Check(!IsWindowVisible(controls), "Setup must be hidden while playing");
            PostMessageW(session.KeyboardWindow(), WM_KEYDOWN, VK_F10, 0);
            Until([&] { return IsWindowVisible(controls) && IsIconic(container); }, "F10 did not open session controls");
            Check(!local::ContainerFocused(container), "Minimized game still accepts controller input");
            session.Resume();
            Until([&] { session.Tick(); return local::PlayerFocused(session.KeyboardWindow(), container); }, "Keyboard focus did not return after restoring");
            HWND keyboard = session.KeyboardWindow();
            HWND host = nullptr, partner = nullptr;
            for (HWND child = GetWindow(container, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
                DWORD pid = 0; GetWindowThreadProcessId(child, &pid);
                (pid == Pid(fs::path(root) / "p1.pid") ? host : partner) = child;
            }
            Check(host && partner, "Missing presentation windows");
            // Only the known host may request cinematic presentation.
            Property(partner, local::kCinematicProperty, 1);
            session.Tick();
            RECT hostBounds{}; GetClientRect(host, &hostBounds);
            Check(hostBounds.right == views[0].width && hostBounds.bottom == views[0].height, "Partner took over presentation");
            RemovePropW(partner, local::kCinematicProperty);
            for (int scene = 0; scene < 3; ++scene) {
                const auto resized = Property(host, L"TEST_RESIZES");
                SendMessageW(host, kCinematic, TRUE, 0);
                Until([&] { session.Tick(); return GetWindow(container, GW_CHILD) == host && local::PlayerFocused(keyboard, container); },
                      "Cinematic lost keyboard focus or host view");
                GetClientRect(host, &hostBounds);
                Check(hostBounds.right == bounds.right && hostBounds.bottom == bounds.bottom &&
                      Property(host, L"TEST_RESIZES") > resized, "Host must resize its render viewport, not stretch a half-screen image");
                Check(IsWindowVisible(partner) && !IsIconic(partner), "Cinematic stopped the partner window");
                // Activation/focus of the keyboard child must not expose it over the host.
                SetWindowPos(partner, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
                Until([&] { return GetWindow(container, GW_CHILD) == partner; }, "Could not simulate partner activation over host");
                Until([&] { session.Tick(); return GetWindow(container, GW_CHILD) == host; }, "Partner obscured cinematic");
                SendMessageW(host, kCinematic, FALSE, 0); // normal end, skip and abort all clear the same state
                Until([&] { session.Tick(); return local::PlayerFocused(keyboard, container); }, "Split restoration lost keyboard focus");
                GetClientRect(host, &hostBounds);
                Check(!GetPropW(host, local::kCinematicProperty) && hostBounds.right == views[0].width &&
                      hostBounds.bottom == views[0].height, "Cutscene did not restore the selected split");
            }
            for (int cycle = 0; cycle < 6; ++cycle) {
                SendMessageW(keyboard, kLoseMouse, 0, 0);
                ShowWindow(controls, SW_RESTORE);
                SetForegroundWindow(controls);
                if (cycle % 2 == 0) ShowWindow(container, SW_MINIMIZE);
                Until([&] { session.Tick(); return GetForegroundWindow() == controls; }, "Could not switch away from game");
                // Neither a pending retry nor an activation message may steal another app's focus.
                for (int i = 0; i < 15; ++i) { session.Tick(); Pump(); Sleep(10); }
                Check(GetForegroundWindow() == controls && !local::PlayerFocused(keyboard, container), "Session stole background focus");
                const auto recovered = Property(keyboard, L"TEST_MOUSE_RECOVERED");
                Property(keyboard, L"TEST_DROP_FOCUS", 2);
                const auto requests = Property(keyboard, L"TEST_FOCUS_REQUESTS");
                // Simulate taskbar restore/Alt+Tab, without using Session::Resume/F10.
                ShowWindow(container, SW_RESTORE);
                SetForegroundWindow(container);
                SetFocus(container);
                Until([&] {
                    session.Tick();
                    return local::PlayerFocused(keyboard, container) && Property(keyboard, L"TEST_MOUSE_RECOVERED") > recovered;
                }, "Repeated restore did not recover keyboard AND mouse");
                Check(Property(keyboard, L"TEST_FOCUS_REQUESTS") >= requests + 3, "Missed focus messages were not retried");
                Property(keyboard, L"TEST_ACTIVATED", 0);
                PostMessageW(keyboard, local::kSessionActivation, FALSE, 0);  // stale deactivation arriving late
                Until([&] { session.Tick(); return Property(keyboard, L"TEST_ACTIVATED") != 0; }, "Stale activation disabled a restored game");
            }
            wchar_t inherited[128]{};
            GetEnvironmentVariableW(L"SACOOP_CONFIG", inherited, 128);
            Check(std::wstring(inherited) == L"parent-config-must-not-be-used", "Parent environment was mutated");
            if (crash) {
                Write(fs::path(root) / "running", "ready");
                Sleep(INFINITE);  // Python kills this launcher, then verifies all owned descendants exit.
            }
            HANDLE p1 = OpenProcess(SYNCHRONIZE, FALSE, Pid(fs::path(root) / "p1.pid"));
            HANDLE p2 = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, Pid(fs::path(root) / "p2.pid"));
            HANDLE server = OpenProcess(SYNCHRONIZE, FALSE, Pid(fs::path(root) / "server.pid"));
            Check(p1 && p2 && server, "Could not inspect owned process tree");
            if (attempt == 0) {
                TerminateProcess(p2, 0);
                Until([&] { session.Tick(); return !session.Active(); }, "P2 exit did not stop session");
            } else session.Stop();
            Check(!IsWindow(container) && !session.ContainerWindow(), "Fullscreen container survived session shutdown");
            for (HANDLE process : {p1, p2, server}) {
                Check(WaitForSingleObject(process, 5000) == WAIT_OBJECT_0, "Orphaned child after stop");
                CloseHandle(process);
            }
        }
        DestroyWindow(controls);
        std::cout << "Session: cinematic resizing/restoration, repeated taskbar/Alt+Tab restores, dropped focus messages, mouse reacquire, both keyboard seats and cleanup passed\n";
        WSACleanup();
    } catch (const std::exception& e) { std::cerr << e.what() << " (Windows " << GetLastError() << ")\n"; return 1; }
}
