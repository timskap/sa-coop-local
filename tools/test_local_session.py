#!/usr/bin/env python3
"""Exercise the actual launcher Session with fake game processes on Windows."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

if sys.platform != "win32":
    raise SystemExit("Session integration tests require Windows")
ROOT = Path(__file__).resolve().parent.parent
kernel = C.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
kernel.OpenProcess.restype = W.HANDLE
kernel.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
kernel.CloseHandle.argtypes = [W.HANDLE]
kernel.TerminateProcess.argtypes = [W.HANDLE, W.UINT]

with tempfile.TemporaryDirectory(prefix="sa-coop-session-") as directory:
    install = Path(directory)
    exe = install / "session-test.exe"
    compiler = ROOT / "toolchain/llvm-mingw/bin/i686-w64-mingw32-clang++.exe"
    sources = ["tests/local_session_tests.cpp", "src/local/features/session/LocalSession.cpp",
               "src/common/lib/ini/Ini.cpp", "src/common/lib/util/Util.cpp"]
    subprocess.run([str(compiler), "-std=c++20", "-static", "-municode", "-DUNICODE", "-D_UNICODE", "-D_WIN32_WINNT=0x0A00",
                    "-I", str(ROOT / "src"), "-I", str(ROOT / "src/local"),
                    *[str(ROOT / s) for s in sources], "-o", str(exe), "-lws2_32", "-lshell32", "-luser32", "-lgdi32",
                    "-ldinput8", "-ldxguid"], check=True)
    shutil.copy2(exe, install / "gta_sa.exe")
    (install / "coop").mkdir()
    shutil.copy2(exe, install / "coop/coop-server.exe")
    (install / "coop.asi").touch()
    try:
        subprocess.run([str(exe)], check=True, timeout=70)
        process = subprocess.Popen([str(exe), "crash"])
        handles = []
        try:
            deadline = time.monotonic() + 35
            while not (install / "running").exists():
                assert process.poll() is None, "Session harness exited before crash test"
                assert time.monotonic() < deadline, "Session crash test startup timed out"
                time.sleep(0.05)
            for name in ("p1.pid", "p2.pid", "server.pid"):
                pid = int((install / name).read_text())
                handle = kernel.OpenProcess(0x100001, False, pid)  # SYNCHRONIZE | TERMINATE
                assert handle, f"Cannot inspect {name}"
                handles.append(handle)
            process.kill()
            process.wait(timeout=5)
            for handle in handles:
                assert kernel.WaitForSingleObject(handle, 5000) == 0, "Launcher crash orphaned a game/server"
            print("Session: abrupt launcher termination closes both games and the server")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            for handle in handles:
                if kernel.WaitForSingleObject(handle, 0) != 0:
                    kernel.TerminateProcess(handle, 1)
                kernel.CloseHandle(handle)
    finally:
        profile_file = install / "profile.txt"
        if profile_file.exists():
            profile = Path(profile_file.read_text(encoding="utf-8"))
            # Only remove the dedicated profile of this random temporary installation.
            if profile.name.startswith("local-") and profile.parent.name == "SA-COOP":
                shutil.rmtree(profile, ignore_errors=True)
