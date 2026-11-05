"""Receive launch settings in memory, check Twilio read-only, then serve the demo."""
import argparse
import base64
import json
import os
from pathlib import Path
import runpy
import sys
import re
import urllib.error
import urllib.request
import urllib.parse

parser = argparse.ArgumentParser()
parser.add_argument('--config-pipe')
parser.add_argument('--diagnose', action='store_true')
parser.add_argument('--save-config', action='store_true')
parser.add_argument('--save-only', action='store_true')
args = parser.parse_args()
if args.config_pipe:
    if not args.config_pipe.startswith('\\\\.\\pipe\\nora-phone-'):
        raise SystemExit('Invalid local configuration pipe')
    with open(args.config_pipe, 'rb') as source:
        config = json.loads(source.readline(8192))
    allowed = {'TWILIO_ACCOUNT_SID','TWILIO_API_KEY_SID','TWILIO_API_KEY_SECRET',
               'TWILIO_AUTH_TOKEN','TWILIO_FROM_NUMBER','NORA_TO_NUMBER'}
    for name, value in config.items():
        if name in allowed and isinstance(value,str):
            os.environ[name] = value

if args.save_config or args.save_only:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from local_config import save_config
    names = ('TWILIO_ACCOUNT_SID','TWILIO_API_KEY_SID','TWILIO_API_KEY_SECRET',
             'TWILIO_AUTH_TOKEN','TWILIO_FROM_NUMBER','NORA_TO_NUMBER')
    save_config({name:os.environ[name] for name in names if os.environ.get(name)})
    print('Calling settings saved encrypted for this Windows user.', flush=True)
    if args.save_only:
        raise SystemExit(0)

account = os.environ.get('TWILIO_ACCOUNT_SID','')
username = os.environ.get('TWILIO_API_KEY_SID') or account
password = os.environ.get('TWILIO_API_KEY_SECRET') if username.startswith('SK') else os.environ.get('TWILIO_AUTH_TOKEN','')
auth = base64.b64encode((username+':'+(password or '')).encode()).decode()
if args.diagnose:
    try:
        request = urllib.request.Request(f'https://api.twilio.com/2010-04-01/Accounts/{account}/Calls.json?' + urllib.parse.urlencode({'To':os.environ.get('NORA_TO_NUMBER',''),'PageSize':10}), headers={'Authorization':'Basic '+auth})
        with urllib.request.urlopen(request, timeout=15) as response:
            alerts=json.load(response)
        print(json.dumps([{'status':item.get('status'),'from':item.get('from'),
                           'matchesDestination':item.get('to')==os.environ.get('NORA_TO_NUMBER')}
                          for item in alerts.get('calls',[])], ensure_ascii=False), flush=True)
    except urllib.error.HTTPError as error:
        print('Diagnostics HTTP',error.code,flush=True)
    raise SystemExit(0)

if account and password and os.environ.get('NORA_TO_NUMBER'):
    try:
        request = urllib.request.Request(f'https://api.twilio.com/2010-04-01/Accounts/{account}/OutgoingCallerIds.json?PageSize=100',
                                         headers={'Authorization':'Basic '+auth})
        with urllib.request.urlopen(request, timeout=15) as response:
            result = json.load(response)
        verified = any(item.get('phone_number') == os.environ.get('NORA_TO_NUMBER') for item in result.get('outgoing_caller_ids',[]))
        history_url = f'https://api.twilio.com/2010-04-01/Accounts/{account}/Calls.json?' + urllib.parse.urlencode({'To':os.environ.get('NORA_TO_NUMBER',''),'PageSize':10})
        try:
            history_request = urllib.request.Request(history_url, headers={'Authorization':'Basic '+auth})
            with urllib.request.urlopen(history_request, timeout=15) as response:
                history = json.load(response)
            previous = next((item for item in history.get('calls',[]) if item.get('to')==os.environ.get('NORA_TO_NUMBER') and item.get('status')=='completed' and re.fullmatch(r'\+[1-9]\d{7,14}',item.get('from',''))),None)
            if previous:
                os.environ['TWILIO_FROM_NUMBER'] = previous['from']
                print('Using the caller number from a previous successful call to this destination.', flush=True)
        except Exception:
            pass
        print('Twilio account connection OK. Call eligibility will be checked by Twilio when SOS is pressed.', flush=True)
    except urllib.error.HTTPError as error:
        print(f'Twilio check returned HTTP {error.code}. Verify account credentials locally. No call was placed.', flush=True)
    except Exception as error:
        print(f'Twilio account check unavailable ({type(error).__name__}). No call was placed.', flush=True)

else:
    print('3D preview ready; real phone calls are not configured.', flush=True)

sys.path.insert(0,str(Path(__file__).resolve().parent))
import threading
from wokwi_bridge import run as run_circuit_bridge
threading.Thread(target=run_circuit_bridge,daemon=True).start()
print('Circuit relay ready: start Wokwi to use its SOS button.',flush=True)
runpy.run_path(str(Path(__file__).with_name('server.py')), run_name='__main__')
