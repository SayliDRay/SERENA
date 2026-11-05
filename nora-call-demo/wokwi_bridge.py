"""Wokwi RFC2217 serial events -> local call API. No provider credentials here."""
import json
import re
import threading
import time
import urllib.error
import urllib.request
import uuid

BASE = 'http://127.0.0.1:5176'
DIAL = re.compile(r'^SERENA_DIAL:([0-9a-fA-F]{8}):(\d+)$')
STOP = re.compile(r'^SERENA_STOP:([0-9a-fA-F]{8}):(\d+)$')


class Api:
    def __init__(self):
        self.token = ''

    def status(self):
        with urllib.request.urlopen(BASE+'/api/status', timeout=4) as response:
            status = json.load(response)
        self.token = status['token']
        return status

    def post(self, action, request_id):
        if not self.token:
            self.status()
        request = urllib.request.Request(BASE+'/api/'+action, data=b'', headers={
            'X-Nora-Token':self.token, 'Origin':BASE, 'X-Request-Id':request_id})
        with urllib.request.urlopen(request, timeout=6) as response:
            return json.load(response)


class Bridge:
    def __init__(self, api, send, log=print, epoch=None):
        self.api, self.send, self.log = api, send, log
        self.epoch = epoch or uuid.uuid4().hex
        self.seen = set()
        self.active = None
        self.request_id = None
        self.last_status = None

    def line(self, text):
        match = DIAL.fullmatch(text.strip())
        if match:
            event = ':'.join(match.groups())
            if event in self.seen:
                return
            self.seen.add(event)
            if self.active:
                self.log('Another call is active; circuit event ignored.')
                self.send('ERROR')
                return
            request_id = self.epoch+':'+event
            try:
                self.api.post('arm', request_id)
                self.active, self.request_id = event, request_id
                self.last_status = None
                self.log('Circuit SOS received. Calling the configured phone.')
            except Exception:
                self.log('Circuit call request rejected or uncertain. No automatic retry.')
                self.send('ERROR')
            return
        match = STOP.fullmatch(text.strip())
        if match and ':'.join(match.groups()) == self.active:
            try:
                self.api.post('hangup', self.request_id)
            except Exception:
                self.log('Hangup unconfirmed; hang up on the phone.')
            self.active = None

    def poll(self):
        if not self.active:
            return
        state = self.api.status()
        if state.get('requestId') != self.request_id:
            self.log('Call ownership changed; circuit relay stopped.')
            self.send('ERROR')
            self.active = None
            return
        status = state['status']
        if status == self.last_status:
            return
        self.last_status = status
        command = {'ringing':'RING', 'in-progress':'ANSWER', 'completed':'HANGUP',
                   'canceled':'HANGUP', 'busy':'BUSY', 'no-answer':'NOANSWER',
                   'failed':'ERROR', 'unknown':'ERROR'}.get(status)
        if command:
            self.send(command)
            self.log('Call status: '+status)
        if status in ('completed','canceled','busy','no-answer','failed','unknown'):
            self.active = None


def run(stop=None):
    import serial
    stop = stop or threading.Event()
    session_epoch = uuid.uuid4().hex
    while not stop.is_set():
        try:
            with serial.serial_for_url('rfc2217://127.0.0.1:4000', baudrate=115200, timeout=.2) as port:
                print('Wokwi circuit connected to the call service.', flush=True)
                bridge = Bridge(Api(), lambda command: port.write((command+'\n').encode()), epoch=session_epoch)
                buffer = b''
                last_poll = 0
                while not stop.is_set():
                    buffer += port.read(512)
                    while b'\n' in buffer:
                        line, buffer = buffer.split(b'\n',1)
                        bridge.line(line.decode('utf-8',errors='replace').strip())
                    buffer = buffer[-4096:]
                    if time.monotonic()-last_poll >= .5:
                        try:
                            bridge.poll()
                        except Exception:
                            pass
                        last_poll = time.monotonic()
        except Exception:
            # The simulator is normally absent until the user starts Wokwi.
            stop.wait(2)


if __name__ == '__main__':
    print('Waiting for Wokwi on port 4000. Only a fresh circuit SOS event places a call.', flush=True)
    try:
        run()
    except KeyboardInterrupt:
        pass
