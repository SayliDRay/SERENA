"""Check the Git candidate set; print locations only, never secret contents."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
git=shutil.which('git')
if not git and (ROOT/'.tools/mingit/cmd/git.exe').exists():
    git=str(ROOT/'.tools/mingit/cmd/git.exe')
if not git:
    raise SystemExit('Install Git before running the publishable-source scan.')
result=subprocess.run([git,'ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT,capture_output=True,check=True)
files=sorted(set(name for name in result.stdout.decode().split('\0') if name))
patterns=[re.compile(r'\b(?:AC|SK|AP)[0-9a-fA-F]{32}\b'),
          re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
          re.compile(r'\bgh[pousr]_[A-Za-z0-9]{30,}\b'),
          re.compile(r'''(?:TWILIO_AUTH_TOKEN|TWILIO_API_KEY_SECRET)\s*[:=]\s*["']([A-Za-z0-9]{20,})["']''')]
blocked=[]
for name in files:
    parts=Path(name).parts
    if any(part in ('.local','.tools','.tools-venv','.platformio-local','.pio','__pycache__','.venv') for part in parts) or name.endswith(('.dpapi','.env','.pem','.key','.log')):
        blocked.append(name+': excluded local data would be published')
        continue
    path=ROOT/name
    if not path.is_file():
        continue
    try:
        text=path.read_text(encoding='utf-8')
    except (UnicodeError,OSError):
        continue
    for index,line in enumerate(text.splitlines(),1):
        if any(pattern.search(line) for pattern in patterns):
            blocked.append(f'{name}:{index}: possible credential')
if blocked:
    print('\n'.join(blocked))
    raise SystemExit(1)
print(f'Publishable-source scan passed ({len(files)} files; no credential values printed).')
