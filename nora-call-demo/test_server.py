"""No network or telephony charges: provider and timers are mocked."""
import importlib.util
import os
import io
import json
import urllib.error
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('nora_call_server', Path(__file__).with_name('server.py'))
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class CallTests(unittest.TestCase):
    def setUp(self):
        server.STATE.update(status='idle', sid=None, deadline=0, message='')

    def test_unconfigured_cannot_claim_ready(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertFalse(server.configured())

    def test_international_phone_format_required(self):
        config = dict(TWILIO_ACCOUNT_SID='AC'+'a'*32, TWILIO_AUTH_TOKEN='test',
                      TWILIO_FROM_NUMBER='+33123456789', NORA_TO_NUMBER='0612345678')
        with patch.dict(os.environ, config, clear=True):
            self.assertFalse(server.configured())
            os.environ['NORA_TO_NUMBER'] = '+33612345678'
            self.assertTrue(server.configured())

    def test_canceled_timer_cannot_place_call(self):
        server.STATE.update(status='canceled', deadline=12)
        with patch.object(server, 'provider') as provider:
            server.dispatch(12)
            provider.assert_not_called()

    def test_stale_timer_cannot_dispatch_new_countdown(self):
        server.STATE.update(status='countdown', deadline=15)
        with patch.object(server, 'provider') as provider:
            server.dispatch(12)
            provider.assert_not_called()

    def test_one_dispatch_then_actual_provider_status(self):
        server.STATE.update(status='countdown', deadline=12)
        with patch.dict(os.environ, {'NORA_TO_NUMBER': '+33612345678', 'TWILIO_FROM_NUMBER': '+33123456789'}), \
             patch.object(server, 'provider', side_effect=[{'sid':'CAexample','status':'queued'},
                                                          {'sid':'CAexample','status':'completed'}]) as provider, \
             patch.object(server.time, 'sleep'):
            server.dispatch(12)
            self.assertEqual(server.STATE['status'], 'completed')
            self.assertEqual(provider.call_count, 2)
            request = provider.call_args_list[0].kwargs['fields']
            self.assertEqual(request['To'], '+33612345678')
            self.assertEqual(request['Url'], 'https://webhooks.twilio.com/v1/Voice/Template/voice_speech_recognition')
            self.assertEqual(set(request), {'To','From','Url'})
            server.dispatch(12)
            self.assertEqual(provider.call_count, 2)

    def test_number_verification_uses_read_only_resource(self):
        with patch.dict(os.environ, {'NORA_TO_NUMBER': '+33712345678'}), \
             patch.object(server, 'provider', return_value={'outgoing_caller_ids':[{'phone_number':'+33712345678'}]}) as provider:
            server.verify_destination()
            self.assertTrue(server.VERIFIED)
            provider.assert_called_once_with(resource='OutgoingCallerIds')

    def test_missing_caller_id_is_advisory_only(self):
        with patch.dict(os.environ, {'NORA_TO_NUMBER': '+33712345678'}), \
             patch.object(server, 'provider', return_value={'outgoing_caller_ids':[]}):
            server.verify_destination()
            self.assertFalse(server.VERIFIED)

    def test_trial_caller_mismatch_displays_actual_provider_code(self):
        server.STATE.update(status='countdown',deadline=12)
        error=urllib.error.HTTPError('https://api.twilio.com',403,'Forbidden',{},io.BytesIO(json.dumps({'code':573003,'message':'Wrong From assignment'}).encode()))
        with patch.dict(os.environ, {'NORA_TO_NUMBER':'+33712345678','TWILIO_FROM_NUMBER':'+49123456789'}), patch.object(server,'provider',side_effect=error):
            server.dispatch(12)
        self.assertEqual(server.STATE['status'],'failed')
        self.assertEqual(server.STATE['errorCode'],573003)
        self.assertIn('From',server.STATE['message'])

    def test_network_uncertainty_is_not_retried(self):
        server.STATE.update(status='countdown', deadline=12)
        with patch.dict(os.environ, {'NORA_TO_NUMBER': '+33612345678', 'TWILIO_FROM_NUMBER': '+33123456789'}), \
             patch.object(server, 'provider', side_effect=TimeoutError) as provider:
            server.dispatch(12)
            self.assertEqual(server.STATE['status'], 'unknown')
            self.assertEqual(provider.call_count, 1)


if __name__ == '__main__':
    unittest.main()
