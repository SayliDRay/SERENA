"""Cross-platform project commands; no command sends a telephone call."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def executable(name):
    found = shutil.which(name)
    if found:
        return found
    if name == 'git':
        portable = ROOT / '.tools/mingit/cmd/git.exe'
        if portable.exists():
            return str(portable)
    if name == 'node':
        portable = next((ROOT / '.tools/node').glob('*/node.exe'), None)
        if portable:
            return str(portable)
    return None


def run(*args, env=None):
    print('>', ' '.join(str(item) for item in args), flush=True)
    subprocess.run([str(item) for item in args], cwd=ROOT, env=env, check=True)


def tests(require_js=False):
    run(sys.executable, '-m', 'unittest', 'discover', '-s', 'nora-call-demo', '-p', 'test_*.py')
    if not executable(os.environ.get('CXX', 'g++')):
        raise SystemExit('C++ tests need g++ (or set CXX to your compiler).')
    run(sys.executable, 'nora-embedded/tests/run_tests.py')
    node = executable('node')
    if node:
        run(node, '--test', 'nora-design/tests/gesture.test.mjs', 'nora-design/tests/phone-call.test.mjs')
    elif require_js:
        raise SystemExit('JavaScript tests need Node.js 22+.')
    else:
        print('JavaScript tests skipped: install Node.js 22+ to run them locally. CI requires them.')


def build(environment):
    portable = ROOT / '.tools/python-native/python.exe'
    if os.name == 'nt' and portable.exists():
        run('powershell.exe', '-NoProfile', '-File', 'nora-embedded/scripts/build-local.ps1', '-Environment', environment)
    else:
        run(sys.executable, '-m', 'platformio', 'run', '-d', 'nora-embedded', '-e', environment)


def scan():
    run(sys.executable, 'scripts/check_secrets.py')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('run', help='Start the 3D site and optional phone relay')
    commands.add_parser('setup', help='Install the runtime dependency')
    commands.add_parser('setup-dev', help='Install runtime and firmware build dependencies')
    test = commands.add_parser('test', help='Run tests with a fake provider')
    test.add_argument('--require-js', action='store_true')
    compile_parser = commands.add_parser('build', help='Compile ESP32 firmware')
    compile_parser.add_argument('--environment', default='xiao_jury_demo', choices=('xiao_jury_demo','xiao_esp32c3','xiao_sim7600_bench'))
    commands.add_parser('check', help='Scan publishable sources and run tests')
    commands.add_parser('scan', help='Scan publishable files for credentials')
    commands.add_parser('doctor', help='Report available tools without revealing credentials')
    clean = commands.add_parser('clean', help='Remove generated caches, leaving source/settings intact')
    clean.add_argument('--firmware', action='store_true', help='Also remove generated firmware build output')
    args = parser.parse_args()
    if args.command == 'run':
        run(sys.executable, 'start_nora.py')
    elif args.command in ('setup','setup-dev'):
        requirements = 'requirements-dev.txt' if args.command == 'setup-dev' else 'requirements.txt'
        run(sys.executable, '-m', 'pip', 'install', '-r', requirements)
    elif args.command == 'test':
        tests(args.require_js)
    elif args.command == 'build':
        build(args.environment)
    elif args.command == 'check':
        scan()
        tests()
    elif args.command == 'scan':
        scan()
    elif args.command == 'doctor':
        print('Python:',sys.version.split()[0])
        for name in ('git','g++','node','make'):
            print(name+':',executable(name) or 'not installed')
        print('Encrypted local calling settings:',(ROOT/'.local/phone-settings.dpapi').exists())
    elif args.command == 'clean':
        paths = ['__pycache__','nora-call-demo/__pycache__','nora-design/__pycache__',
                 'nora-embedded/tests/__pycache__','scripts/__pycache__']
        if args.firmware:
            paths += ['nora-embedded/build','nora-embedded/.pio/build']
        for name in paths:
            target=(ROOT/name).resolve()
            if not target.is_relative_to(ROOT) or target==ROOT:
                raise SystemExit('Refusing unsafe cleanup path')
            if target.exists():
                shutil.rmtree(target)
                print('Removed generated output:',name)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
    except KeyboardInterrupt:
        raise SystemExit(130)
