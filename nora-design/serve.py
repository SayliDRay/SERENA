"""Compatibility launcher: the design and calling service now start together."""
from pathlib import Path
import subprocess
import sys

if __name__ == '__main__':
    launcher = Path(__file__).resolve().parent.parent / 'start_nora.py'
    raise SystemExit(subprocess.call([sys.executable, str(launcher)]))
