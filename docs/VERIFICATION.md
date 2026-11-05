# Verification and scope

Updated 2026-11-05.

## Established in the working prototype

- The user confirmed a real normal-phone demonstration call after the trial caller-number mismatch was corrected.
- ESP32-C3 jury firmware compiles: 271422 bytes flash and 14300 bytes static RAM in the verified local build.
- Native tests pass: 13 controller/heuristic groups and 7 SIM7600 transcript groups.
- The backend/serial relay suite passes 15 tests with a fake provider, including no-countdown serial-event forwarding, duplicate suppression and hang-up ownership.
- The 3D model has finite vertex positions and normals at closed, 90° and 115° lid positions.
- Gesture/adapter tests cover short release, two-second hang-up, cancellation and duplicate requests. CI also requires these JavaScript tests.
- The main page is the 3D presentation; retired button-test routes return 404.
- Wokwi lint reports one informational warning for the undocumented XIAO board type.

No real phone calls are made by automated tests or CI. A completed/answered provider state can include voicemail; it does not prove a person heard the demonstration.

## Limits

The software bridge protocol is tested locally. Interactive Wokwi use requires its own license and an active serial connection. A CLI token was unavailable during automated Wokwi verification; no unattended full Wokwi-to-phone run is claimed.

The latest enclosure revision passed geometry checks, but a complete live-browser visual review was blocked by browser-control URL-confidence restrictions. Review its appearance on the presentation computer before the jury.

The physical electrical bracelet is unvalidated. Operator/VoLTE, modem/antenna, battery/charging/protection, acoustics, power integrity, wrist fit, waterproofing and fabrication require real bench and user testing. The QR patterns are illustrative. The fall heuristic is experimental.

GitHub Actions validates software and compilation; it has no phone-provider secrets and cannot place real calls.
