#pragma once
#include <initializer_list>
#include <cwchar>
#include <windows.h>
#include <xinput.h>

namespace coop::local {
// Resolve only from Windows' system directory; no extra controller DLL is distributed.
inline DWORD ReadPad(DWORD index, XINPUT_STATE& state) {
    using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    static GetState get = []() -> GetState {
        wchar_t system[MAX_PATH]{};
        if (!GetSystemDirectoryW(system, MAX_PATH)) return nullptr;
        for (const wchar_t* name : {L"\\xinput1_4.dll", L"\\xinput1_3.dll", L"\\xinput9_1_0.dll"}) {
            wchar_t path[MAX_PATH]{};
            if (wcslen(system) + wcslen(name) >= MAX_PATH) continue;
            wcscpy(path, system);
            wcscat(path, name);
            if (HMODULE module = LoadLibraryW(path)) {
                auto fn = reinterpret_cast<GetState>(GetProcAddress(module, "XInputGetState"));
                if (fn) return fn;
                FreeLibrary(module);
            }
        }
        return nullptr;
    }();
    state = {};
    return get && index < 4 ? get(index, &state) : ERROR_DEVICE_NOT_CONNECTED;
}
}  // namespace coop::local
