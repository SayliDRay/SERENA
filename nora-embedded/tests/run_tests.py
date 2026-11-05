"""Compile and run the actual portable firmware modules, no Python reimplementation."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CXX", "g++")
if not shutil.which(compiler):
    sys.exit(f"Missing C++ compiler {compiler!r}. Install GCC or set CXX.")
build = root / "build"
build.mkdir(exist_ok=True)
exe = build / ("nora-tests.exe" if os.name == "nt" else "nora-tests")
subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(root / "include"), str(root / "tests/test_main.cpp"),
                *[str(root / "src" / name) for name in
                  ("controller.cpp", "mock_modem.cpp", "fall_detector.cpp")],
                "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
modem_exe = build / ("sim7600-tests.exe" if os.name == "nt" else "sim7600-tests")
subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(root / "include"), str(root / "tests/test_sim7600.cpp"),
                str(root / "src/sim7600_modem.cpp"), str(root / "src/controller.cpp"),
                "-o", str(modem_exe)], check=True)
subprocess.run([str(modem_exe)], check=True)
