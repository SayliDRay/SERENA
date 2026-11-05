"""Serial-to-HTTP integration tests. A fake provider prevents real calls."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import threading
import time
import unittest
import urllib.request
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import wokwi_bridge

spec = importlib.util.spec_from_file_location('relay_test_server', ROOT/'server.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class RelayTests(unittest.TestCase):
    def setUp(self):
        self.commands, self.requests = [], []
        self.state = {'status':'ringing'}
        owner = self
        class FakeApi:
            def post(self, action, request_id):
                owner.requests.append((action,request_id))
                return {}
            def status(self):
                return {**owner.state,'requestId':owner.bridge.request_id}
        self.bridge = wokwi_bridge.Bridge(FakeApi(), self.commands.append, lambda _: None)

    def test_only_explicit_marker_dials_once(self):
        for line in ('STATE DIALING | call REQUESTED','SERENA_BOOT:1234abcd','READY','random output'):
            self.bridge.line(line)
        self.assertEqual(self.requests, [])
        self.bridge.line('SERENA_DIAL:1234abcd:1')
        self.bridge.line('SERENA_DIAL:1234abcd:1')
        self.assertEqual(len(self.requests),1)
        self.assertEqual(self.requests[0][0],'arm')

    def test_actual_progress_is_forwarded_to_firmware(self):
        self.bridge.line('SERENA_DIAL:1234abcd:1')
        for status in ('ringing','ringing','in-progress','completed'):
            self.state['status']=status
            self.bridge.poll()
        self.assertEqual(self.commands,['RING','ANSWER','HANGUP'])
        self.assertIsNone(self.bridge.active)

    def test_stale_stop_does_not_hang_up_current_call(self):
        self.bridge.line('SERENA_DIAL:1234abcd:2')
        self.bridge.line('SERENA_STOP:1234abcd:1')
        self.assertEqual(len(self.requests),1)
        self.bridge.line('SERENA_STOP:1234abcd:2')
        self.assertEqual(self.requests[-1][0],'hangup')

    def test_refused_request_is_not_retried(self):
        with patch.object(self.bridge.api,'post',side_effect=TimeoutError) as post:
            self.bridge.line('SERENA_DIAL:1234abcd:1')
            self.bridge.line('SERENA_DIAL:1234abcd:1')
            self.assertEqual(post.call_count,1)
        self.assertEqual(self.commands,['ERROR'])


class HttpIntegration(unittest.TestCase):
    def test_serial_event_reaches_fake_provider_without_countdown(self):
        server.STATE.update(status='idle',sid=None,deadline=0,errorCode=None)
        server.SEEN_REQUESTS.clear()
        server.STOP_REQUESTED=False
        settings={'TWILIO_ACCOUNT_SID':'AC'+'a'*32,'TWILIO_AUTH_TOKEN':'fake',
                  'TWILIO_FROM_NUMBER':'+33123456789','NORA_TO_NUMBER':'+33712345678'}
        http=server.ThreadingHTTPServer(('127.0.0.1',0),server.Handler)
        base='http://127.0.0.1:'+str(http.server_address[1])
        actions=[]
        def provider(suffix='',fields=None,resource='Calls'):
            actions.append((suffix,fields))
            return {'sid':'CAtest','status':'completed' if suffix else 'queued'}
        with patch.dict(os.environ,settings), patch.object(server,'PORT',http.server_address[1]), \
             patch.object(wokwi_bridge,'BASE',base), patch.object(server,'provider',side_effect=provider), \
             patch.object(server.time,'sleep',return_value=None):
            threading.Thread(target=http.serve_forever,daemon=True).start()
            bridge=wokwi_bridge.Bridge(wokwi_bridge.Api(),lambda _:None,lambda _:None)
            before=time.monotonic()
            bridge.line('SERENA_DIAL:face0123:7')
            for _ in range(10000):
                if server.STATE['status']=='completed': break
                threading.Event().wait(.001)
            self.assertEqual(server.STATE['status'],'completed')
            self.assertLess(time.monotonic()-before,2)
            self.assertEqual(actions[0][1]['To'],settings['NORA_TO_NUMBER'])
            self.assertEqual(sum(fields is not None for _,fields in actions),1)
            bridge.line('SERENA_DIAL:face0123:7')
            self.assertEqual(sum(fields is not None for _,fields in actions),1)
            for path in ('/phone','/app.js','/style.css'):
                try: urllib.request.urlopen(base+path,timeout=2)
                except Exception as error: self.assertEqual(error.code,404)
                else: self.fail('Retired test page is still exposed')
            with urllib.request.urlopen(base+'/',timeout=2) as response:
                self.assertTrue(response.url.endswith('/bracelet/'))
            http.shutdown()
        http.server_close()

    def test_late_stop_cannot_touch_another_call(self):
        server.STATE.update(status='in-progress',sid='CAnew',requestId='new')
        with patch.object(server,'provider') as provider:
            server.stop_call('old')
            provider.assert_not_called()
        self.assertEqual(server.STATE['sid'],'CAnew')


if __name__=='__main__':
    unittest.main()
