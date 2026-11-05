"""One command starts the 3D bracelet and its calling API on the same server."""
from pathlib import Path
import json
import os
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parent
NATIVE = ROOT / '.tools/python-native/python.exe'
if NATIVE.exists() and Path(sys.executable).resolve() != NATIVE.resolve():
    raise SystemExit(subprocess.call([str(NATIVE), str(Path(__file__).resolve()), *sys.argv[1:]]))

sys.path.insert(0, str(ROOT / 'nora-call-demo'))
from local_config import load_config

try:
    with urllib.request.urlopen('http://127.0.0.1:5176/api/status', timeout=2) as response:
        status = json.load(response)
    if status.get('configured'):
        print('NORA 3D + phone calls already running: http://localhost:5176/bracelet/', flush=True)
        raise SystemExit(0)
    raise SystemExit('Port 5176 has an unconfigured server. Stop that server with Ctrl+C, then run this command again.')
except (OSError, ValueError):
    pass

settings = load_config()
allowed = ('TWILIO_ACCOUNT_SID','TWILIO_API_KEY_SID','TWILIO_API_KEY_SECRET',
           'TWILIO_AUTH_TOKEN','TWILIO_FROM_NUMBER','NORA_TO_NUMBER')
for name in allowed:
    if name in settings and isinstance(settings[name], str):
        os.environ[name] = settings[name]
if not os.environ.get('TWILIO_ACCOUNT_SID') or not (os.environ.get('TWILIO_AUTH_TOKEN') or os.environ.get('TWILIO_API_KEY_SECRET')):
    print('3D preview mode: real calls are disabled until calling settings are configured.', flush=True)
print('SÉRÉNA 3D: http://localhost:5176/bracelet/', flush=True)
print('Keep this terminal open. Ctrl+C stops both the 3D site and the calling API.', flush=True)
import runpy
sys.argv = [str(ROOT / 'nora-call-demo/launch-phone.py')]
runpy.run_path(sys.argv[0], run_name='__main__')
