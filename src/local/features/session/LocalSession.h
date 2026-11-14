#pragma once
#include <windows.h>
#include <array>
#include <string>
#include "common/lib/local-coop/Setup.h"

namespace coop::local {
class Session {
public:
    explicit Session(std::wstring gameDir);
    ~Session();
    bool Start(const Setup& setup, int slot);
    void Tick();
    void Stop();
    bool Active() const { return phase != Phase::Idle; }
    bool Running() const { return phase == Phase::Running; }
    bool TakeFocusRequest();
    const std::wstring& Status() const { return status; }
    const std::wstring& Directory() const { return dataDir; }
    bool SaveExists(int slot) const;
    HWND KeyboardWindow() const;
    HWND ContainerWindow() const { return container; }
    void SetControlsWindow(HWND window) { controls = window; }
    void Resume();
private:
    enum class Phase { Idle, Host, Partner, Running };
    Phase phase = Phase::Idle;
    std::wstring gameDir, dataDir, status;
    std::array<HANDLE, 2> processes{}, ready{};
    HANDLE job = nullptr;
    std::array<DWORD, 2> pids{};
    std::array<std::wstring, 2> configs{}, eventNames{};
    int keyboardSeat = 0;
    ULONGLONG deadline = 0;
    bool focusRequested = false;
    HWND container = nullptr, controls = nullptr;
    bool closing = false, stopping = false;
    bool CreateContainer();
    void FocusKeyboard();
    void NotifyActivation(bool active);
    void ShowControls();
    static LRESULT CALLBACK ContainerProc(HWND window, UINT message, WPARAM wp, LPARAM lp);
    bool Launch(int player);
    bool WriteConfigs(const Setup& setup, int slot, unsigned port);
    void Fail(std::wstring message);
};
}
