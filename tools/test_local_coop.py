#!/usr/bin/env python3
"""Run the platform-independent local co-op regression tests on Linux or Windows."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
compiler = (ROOT / "toolchain/llvm-mingw/bin/x86_64-w64-mingw32-clang++.exe") if os.name == "nt" else shutil.which("g++")
if not compiler or not Path(compiler).exists():
    raise SystemExit("Install g++ on Linux, or run tools/get-toolchain.ps1 on Windows first")
with tempfile.TemporaryDirectory(prefix="sa-coop-tests-") as directory:
    for test in ("local_coop_tests", "save_file_tests", "local_audio_tests", "local_render_resize_tests"):
        exe = Path(directory) / (test + (".exe" if os.name == "nt" else ""))
        subprocess.run([str(compiler), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(ROOT / "src"),
                        "-I", str(ROOT / "src/client"),
                        str(ROOT / "tests" / (test + ".cpp")), "-o", str(exe)], check=True)
        subprocess.run([str(exe), directory], check=True)
