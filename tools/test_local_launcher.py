#!/usr/bin/env python3
"""Windows smoke test of the real launcher window, without GTA or physical controllers."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import subprocess
import sys
import time

if sys.platform != "win32":
    raise SystemExit("This UI smoke test requires Windows; use test_local_coop.py for portable tests")

ROOT = Path(__file__).resolve().parent.parent
user = C.WinDLL("user32", use_last_error=True)
callback_type = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
user.EnumWindows.argtypes = [callback_type, W.LPARAM]
user.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
user.GetDlgItem.argtypes = [W.HWND, C.c_int]
user.GetDlgItem.restype = W.HWND
user.IsWindowEnabled.argtypes = [W.HWND]
user.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
user.SendMessageW.restype = C.c_ssize_t
user.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
user.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, C.c_int]

process = subprocess.Popen([str(ROOT / "build/release/bin/coop-local.exe")])
window = None
try:
    for attempt in range(100):
        found = []
        @callback_type
        def visit(hwnd, _):
            pid = W.DWORD()
            user.GetWindowThreadProcessId(hwnd, C.byref(pid))
            title = C.create_unicode_buffer(256)
            user.GetWindowTextW(hwnd, title, len(title))
            if pid.value == process.pid and title.value == "SA:COOP - Local Play":
                found.append(hwnd)
            return True
        user.EnumWindows(visit, 0)
        if found:
            window = found[0]
            break
        assert process.poll() is None, "Launcher exited before opening its window"
        time.sleep(0.1)
    assert window, "Launcher did not open a window within 10 seconds"
    def enabled(control):
        hwnd = user.GetDlgItem(window, control)
        assert hwnd, f"Missing native control {control}"
        return bool(user.IsWindowEnabled(hwnd))
    def click(control):
        user.SendMessageW(window, 0x111, control, user.GetDlgItem(window, control))  # WM_COMMAND / BN_CLICKED
    assert not enabled(100), "Play must start disabled"
    click(200)  # keyboard -> P1
    assert enabled(110) and not enabled(111)
    click(110)  # P1 ready; one player cannot launch
    assert not enabled(100)
    click(201)  # same keyboard -> P2, never both
    assert not enabled(110) and enabled(111) and not enabled(100)
    click(111)
    assert not enabled(100)
    click(101)  # layout toggle is live, not a decorative control
    title = C.create_unicode_buffer(256)
    user.GetWindowTextW(user.GetDlgItem(window, 101), title, len(title))
    assert "LEFT / RIGHT" in title.value
    print("Native launcher: window, controls, device ownership, readiness and layout passed")
finally:
    if window:
        user.PostMessageW(window, 0x10, 0, 0)  # WM_CLOSE: no active game, so no confirmation
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
