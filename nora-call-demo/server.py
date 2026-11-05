"""Local SOS demonstration. Real calls require the owner's Twilio configuration."""
import base64
import json
import os
import re
import secrets
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TOKEN = secrets.token_urlsafe(32)
PORT = int(os.environ.get('NORA_CALL_PORT', '5176'))
LOCK = threading.Lock()
STOP_REQUESTED = False
SEEN_REQUESTS = set()
STATE = {'status': 'idle', 'sid': None, 'deadline': 0, 'message': 'Prêt.', 'errorCode': None}
VERIFIED = False
VERIFY_MESSAGE = 'Vérifiez le téléphone destinataire dans Twilio, puis cliquez sur Vérifier mon numéro.'
TERMINAL = {'idle', 'completed', 'canceled', 'busy', 'failed', 'no-answer', 'unknown'}


def configured():
    return (bool(re.fullmatch(r'AC[0-9a-fA-F]{32}', os.environ.get('TWILIO_ACCOUNT_SID', '')))
            and (bool(os.environ.get('TWILIO_AUTH_TOKEN')) or (bool(re.fullmatch(r'SK[0-9a-fA-F]{32}',os.environ.get('TWILIO_API_KEY_SID',''))) and bool(os.environ.get('TWILIO_API_KEY_SECRET'))))
            and all(re.fullmatch(r'\+[1-9]\d{7,14}', os.environ.get(k, ''))
                    for k in ('TWILIO_FROM_NUMBER', 'NORA_TO_NUMBER')))


def provider(suffix='', fields=None, resource='Calls'):
    sid = os.environ['TWILIO_ACCOUNT_SID']
    username=os.environ.get('TWILIO_API_KEY_SID') or sid
    password=os.environ.get('TWILIO_API_KEY_SECRET') if username.startswith('SK') else os.environ.get('TWILIO_AUTH_TOKEN')
    credentials = base64.b64encode((username + ':' + password).encode()).decode()
    url = f'https://api.twilio.com/2010-04-01/Accounts/{sid}/{resource}{suffix}.json'
    data = None if fields is None else urllib.parse.urlencode(fields).encode()
    request = urllib.request.Request(url, data=data, headers={
        'Authorization': 'Basic ' + credentials,
        'Content-Type': 'application/x-www-form-urlencoded'})
    with urllib.request.urlopen(request, timeout=15) as response:
        return json.load(response)


def verify_destination():
    global VERIFIED, VERIFY_MESSAGE
    VERIFIED = False
    try:
        result = provider(resource='OutgoingCallerIds')
        VERIFIED = any(item.get('phone_number') == os.environ.get('NORA_TO_NUMBER')
                       for item in result.get('outgoing_caller_ids', []))
        VERIFY_MESSAGE = ('Téléphone vérifié. Vous pouvez maintenir SOS.' if VERIFIED else
                          'Numéro absent de la liste des identifiants appelants. Cela ne suffit pas à déterminer si un appel est autorisé : Twilio validera la demande SOS.')
    except Exception:
        VERIFY_MESSAGE = 'Vérification impossible. Vérifiez Internet et les identifiants Twilio.'


def dispatch(deadline):
    with LOCK:
        if STATE['status'] != 'countdown' or STATE['deadline'] != deadline:
            return
        STATE.update(status='sending', message='Demande envoyée au service téléphonique…')
    try:
        # Reuse the exact supported trial template that worked in Twilio Console.
        result = provider(fields={
            'To': os.environ['NORA_TO_NUMBER'], 'From': os.environ['TWILIO_FROM_NUMBER'],
            'Url': 'https://webhooks.twilio.com/v1/Voice/Template/voice_speech_recognition'})
        with LOCK:
            STATE.update(sid=result['sid'], status=result['status'], message='Demande acceptée par le service.')
        if STOP_REQUESTED:
            stop_call(STATE.get('requestId'))
        # Poll the actual provider; do not invent an answered state or require public webhooks.
        for _ in range(65):
            time.sleep(2)
            result = provider('/' + result['sid'])
            with LOCK:
                if STATE['sid'] != result['sid'] or STATE['status'] in ('canceled','completed'):
                    return
                STATE.update(status=result['status'], message='État retourné par le service téléphonique.')
            if result['status'] in TERMINAL:
                return
        with LOCK:
            STATE.update(status='unknown', message='Suivi interrompu. Vérifiez le journal du fournisseur avant un autre appel.')
    except urllib.error.HTTPError as error:
        try:
            details = json.loads(error.read(65536))
            code = details.get('code')
            explanation = re.sub(r'\+[0-9]{7,15}', '[téléphone]', str(details.get('message', '')))[:500]
        except Exception:
            code = None
            explanation = ''
        hints = {
            573003: 'Le numéro From n’est pas attribué à ce destinataire. Reprenez le From du test Voice pour ce téléphone.',
            21219: 'Twilio exige la vérification du téléphone destinataire dans ce compte.',
            21210: 'Le numéro émetteur n’est pas autorisé dans ce compte.',
            21212: 'Numéro émetteur invalide. Reprenez le From du test Voice pour ce destinataire.',
            21215: 'Les permissions géographiques Twilio bloquent cette destination.',
            21264: 'Twilio refuse ce numéro émetteur pour cet appel.',
            20003: 'Twilio refuse les identifiants. Vérifiez la clé API et son compte.',
            21211: 'Twilio considère le téléphone destinataire invalide.'}
        with LOCK:
            if STATE['deadline'] != deadline:
                return
            STATE.update(status='unknown' if STATE['sid'] or error.code >= 500 else 'failed',
                         errorCode=code,
                         message=f'Twilio HTTP {error.code}, code {code or "inconnu"}. ' +
                                 hints.get(code, explanation or 'Consultez ce code dans les journaux Twilio. Aucun nouvel appel automatique.'))
    except Exception:
        with LOCK:
            if STATE['deadline'] != deadline:
                return
            STATE.update(status='unknown', message='Résultat réseau incertain. Vérifiez le journal du fournisseur avant de réessayer ; aucun renvoi automatique.')


def stop_call(request_id=None):
    global STOP_REQUESTED
    with LOCK:
        if request_id is not None and request_id != STATE.get('requestId'):
            return
        STOP_REQUESTED = True
        sid = STATE['sid']
        status = STATE['status']
        if status in TERMINAL:
            return
        if not sid and status == 'countdown':
            STATE.update(status='canceled', message='Appel annulé.')
            return
    if sid:
        try:
            result = provider('/'+sid, {'Status':'completed' if status=='in-progress' else 'canceled'})
            with LOCK:
                if STATE['sid'] != sid:
                    return
                STATE.update(status=result.get('status','completed'), message='Appel terminé.')
        except Exception:
            with LOCK:
                STATE['message'] = 'Raccrochage non confirmé. Raccrochez sur le téléphone.'


def snapshot():
    with LOCK:
        return {**STATE, 'remaining': max(0, STATE['deadline'] - time.monotonic()),
                'configured': configured(), 'verified': VERIFIED, 'verificationMessage': VERIFY_MESSAGE, 'destination': ('•••• ' + os.environ.get('NORA_TO_NUMBER','')[-4:]) if os.environ.get('NORA_TO_NUMBER') else ''}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # Do not log phone numbers, authorization headers or requests.

    def reply(self, code, value, mime='application/json; charset=utf-8', extra=None):
        body = json.dumps(value, ensure_ascii=False).encode() if mime.startswith('application/json') else value
        self.send_response(code)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('Referrer-Policy', 'no-referrer')
        for name, item in (extra or {}).items():
            self.send_header(name, item)
        self.end_headers()
        self.wfile.write(body)

    def valid_host(self):
        return self.headers.get('Host') in (f'localhost:{PORT}', f'127.0.0.1:{PORT}')

    def do_GET(self):
        if not self.valid_host():
            return self.reply(403, {'error': 'Local host required'})
        route = urllib.parse.urlsplit(self.path).path
        if route in ('/', '/index.html'):
            return self.reply(302, b'', 'text/plain', {'Location':'/bracelet/'})
        if route == '/api/status':
            return self.reply(200, {**snapshot(), 'token': TOKEN})
        if route == '/prototype' or route.startswith('/prototype/'):
            return self.reply(302,b'', 'text/plain', {'Location':'/bracelet/'})
        if route == '/bracelet':
            self.send_response(302)
            self.send_header('Location','/bracelet/')
            self.end_headers()
            return
        if route.startswith('/bracelet/'):
            design_root = ROOT.parent / 'nora-design'
            relative = urllib.parse.unquote(route[len('/bracelet/'):]) or 'index.html'
            resources={'resources/'+name:name for name in ('system.svg','README.md','bom.csv','wiring.csv')}
            if relative in resources:
                hardware=ROOT.parent/'nora-embedded'/'hardware'/resources[relative]
                mime='image/svg+xml' if relative.endswith('.svg') else 'text/plain; charset=utf-8'
                return self.reply(200,hardware.read_bytes(),mime)
            actual = (design_root / relative).resolve()
            if not actual.is_relative_to(design_root.resolve()) or not actual.is_file():
                return self.reply(404, {'error':'Not found'})
            valid = relative in ('index.html','app.js','phone-call.js','model.js','interaction.js','style.css') or relative.startswith(('vendor/','assets/'))
            if not valid:
                return self.reply(404, {'error':'Not found'})
            types={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.svg':'image/svg+xml','.png':'image/png'}
            return self.reply(200,actual.read_bytes(),types.get(actual.suffix,'application/octet-stream'))
        return self.reply(404, {'error':'Page retirée. Ouvrez /bracelet/.'})

    def do_POST(self):
        global STOP_REQUESTED
        origin = self.headers.get('Origin')
        if (not self.valid_host() or self.headers.get('X-Nora-Token') != TOKEN
                or origin not in (f'http://localhost:{PORT}', f'http://127.0.0.1:{PORT}')):
            return self.reply(403, {'error': 'Local request required'})
        if self.path == '/api/check':
            verify_destination()
            return self.reply(200, snapshot())
        if self.path == '/api/hangup':
            request_id=self.headers.get('X-Request-Id')
            if request_id and request_id != STATE.get('requestId'):
                return self.reply(409,{'error':'Cet événement ne possède pas cet appel.'})
            threading.Thread(target=stop_call,args=(STATE.get('requestId'),),daemon=True).start()
            return self.reply(200,snapshot())
        if self.path == '/api/arm':
            with LOCK:
                if not configured():
                    return self.reply(409, {'error': 'Configurez le service téléphonique avant le test réel.'})
                request_id=self.headers.get('X-Request-Id') or secrets.token_hex(16)
                if request_id in SEEN_REQUESTS:
                    return self.reply(409, {'error':'Événement déjà reçu. Aucun nouvel appel.'})
                if len(SEEN_REQUESTS)>=1000:
                    return self.reply(409, {'error':'Redémarrez la démonstration.'})
                if STATE['status'] not in TERMINAL or STATE['status'] == 'unknown':
                    return self.reply(409, {'error': 'Un appel est déjà actif ou son résultat reste incertain.'})
                SEEN_REQUESTS.add(request_id)
                STOP_REQUESTED=False
                STATE.update(status='countdown', sid=None, errorCode=None, requestId=request_id, deadline=time.monotonic(),
                             message='Connexion en cours…')
                timer = threading.Timer(0, dispatch, args=(STATE['deadline'],))
                timer.daemon = True
                timer.start()
        elif self.path == '/api/cancel':
            with LOCK:
                if STATE['status'] != 'countdown':
                    return self.reply(409, {'error': 'La demande est déjà partie. Le bouton annule uniquement le compte à rebours.'})
                STATE.update(status='canceled', message='Annulé avant l’envoi de l’appel.')
        else:
            return self.reply(404, {'error': 'Not found'})
        self.reply(200, snapshot())


if __name__ == '__main__':
    if configured():
        verify_destination()
    print(f'NORA 3D + appels : http://localhost:{PORT}/bracelet/ | configuration appels : {configured()}', flush=True)
    ThreadingHTTPServer(('127.0.0.1', PORT), Handler).serve_forever()
