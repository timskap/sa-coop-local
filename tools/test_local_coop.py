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
    exe = Path(directory) / ("local-tests.exe" if os.name == "nt" else "local-tests")
    subprocess.run([str(compiler), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(ROOT / "src"),
                    str(ROOT / "tests/local_coop_tests.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
