# Local phone API and Wokwi relay

Start from the repository root with `python manage.py run`. The only presentation page is **http://localhost:5176/bracelet/**. The old standalone button-test page is retired.

## Configuration

Windows users can run `./nora-call-demo/start-phone.ps1`. It saves current-user encrypted settings under `.local/`, which is ignored by Git. On other systems, set the process environment using the names in [../.env.example](../.env.example). Real calls are disabled without configuration; the 3D preview still runs.

Required: account SID, an account Auth Token **or** Standard API key/secret, destination, and an authorized caller number. Trial recipients must be verified. The caller number must be the one assigned to that recipient by Twilio's Voice test. Error **573003** means that the From number belongs to a different trial recipient. The launcher attempts to reuse a prior completed call's caller number for the same destination.

The supported request uses `To`, `From`, and `Url=https://webhooks.twilio.com/v1/Voice/Template/voice_speech_recognition`. It plays the provider's trial scenario, not live bracelet audio. Trial prompts are not suppressed. Trial quotas and provider restrictions still apply.

## Runtime

A brief 3D click calls `/api/arm`. There is no programmed countdown. A two-second hold requests `/api/hangup`. Provider states drive the display; acceptance, ringing and answered states are distinct. An answered line can be voicemail. No automatic redial occurs. A network timeout can be uncertain; check the provider call log before another attempt.

`wokwi_bridge.py` waits on RFC2217 port 4000. Jury firmware emits `SERENA_DIAL:<boot>:<sequence>` once per SOS and a matching `SERENA_STOP` for hang-up. The bridge forwards authenticated local requests, suppresses duplicate events, and sends real call statuses back to the simulated modem. Provider secrets never enter ESP32 code. Keep Wokwi visible so its simulation and serial stream continue.

The API binds to loopback, checks local origin/token headers, limits active requests and owns hang-up by request ID. Generated local settings are encrypted at rest on Windows and never served. Do not expose this call-capable local API as a public website.

## Tests and troubleshooting

`python manage.py test` runs the backend/relay suites with fake providers and does not call any phone.

- **No circuit connection:** start Wokwi using the default jury build and verify `rfc2217ServerPort = 4000` in wokwi.toml.
- **Phone not ringing:** inspect the actual API status/error code; verify destination and its assigned trial caller number in Twilio.
- **Unconfirmed hang-up:** hang up on the phone. Do not claim the carrier stopped the call unless confirmed.
- **Port in use:** the combined launcher reuses an existing configured server. Stop it with Ctrl+C before loading changed backend source.

Official references: [Voice trial API](https://www.twilio.com/docs/usage/trials/try-out-voice), [Call resource](https://www.twilio.com/docs/voice/api/call-resource), [Wokwi serial forwarding](https://docs.wokwi.com/vscode/project-config#serial-port-forwarding).
