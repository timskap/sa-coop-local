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
    LRESULT result = 0;
    if (local::PlayerWindowMessage(h, GetParent(h), m, w, l, result)) return result;
    if (m == WM_ACTIVATE) { SetPropW(h, L"TEST_ACTIVATED", reinterpret_cast<HANDLE>(static_cast<uintptr_t>(w))); return 0; }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
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
    HWND inputOwner = local::BackgroundInputWindow();
    Check(inputOwner && !GetParent(inputOwner) && !IsWindowVisible(inputOwner), "Input owner must be hidden and top-level");
    Check(SUCCEEDED(mouse->SetCooperativeLevel(inputOwner, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)), "Embedded mouse cooperative level failed");
    mouse->Release(); input->Release();
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
            setup.horizontal = attempt == 0;
            for (const char* file : {"p1.pid", "p2.pid", "server.pid", "allow-host"}) fs::remove(fs::path(root) / file);
            Check(session.Start(setup, -1), "Session start failed");
            Until([&] { session.Tick(); return Pid(fs::path(root) / "p1.pid") != 0; }, "P1 never started");
            for (int i = 0; i < 15; ++i) { session.Tick(); Pump(); Sleep(20); }
            Check(!fs::exists(fs::path(root) / "p2.pid"), "P2 started before host readiness (or reused stale event)");
            Ini one, two;
            Check(one.Load(session.Directory() + L"player1\\local.ini") && two.Load(session.Directory() + L"player2\\local.ini"), "Missing profiles");
            Check(one.Get("local", "input", "") == "gamepad" && two.Get("local", "input", "") == "keyboard", "Seat mapping changed");
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
            Until([&] { return local::PlayerFocused(session.KeyboardWindow(), container); }, "Keyboard focus did not enter its child window");
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
            Until([&] { return local::PlayerFocused(session.KeyboardWindow(), container); }, "Keyboard focus did not return after restoring");
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
        std::cout << "Session: fullscreen child layouts, focus/controls, DirectInput ownership, startup ordering and process cleanup passed\n";
        WSACleanup();
    } catch (const std::exception& e) { std::cerr << e.what() << " (Windows " << GetLastError() << ")\n"; return 1; }
}
