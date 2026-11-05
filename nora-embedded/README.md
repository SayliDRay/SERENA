# ESP32 circuit simulation

The default profile is **xiao_jury_demo**, using a Seeed XIAO ESP32C3 in Wokwi. It reads the actual simulated GPIO button, debounces it, runs the C++ state machine, and reports call events to a computer through RFC2217 serial. It is not an LTE, SIM or audio emulator.

## Start

1. Run `python ../manage.py run` from this folder (or `python manage.py run` from the repository root).
2. Build with `python ../manage.py build` or `pio run -e xiao_jury_demo`.
3. Run **Wokwi: Start Simulator** in VS Code with this folder's `wokwi.toml`.
4. Keep the simulator tab visible. Release SOS once after boot, then briefly press/release its red button.
5. A two-second hold during dialing/connection requests hang-up. The manual SOS workflow has no countdown in this profile.

The server terminal prints when Wokwi connects. The circuit uses serial port 4000. No provider credentials are embedded in the firmware, and boot does not place a call. A valid Wokwi license is required. The computer must remain online and the phone service must be configured for a real demonstration call.

## Profiles

| Environment | Purpose |
| --- | --- |
| `xiao_jury_demo` | Short press calls through computer relay; long hold hangs up |
| `xiao_esp32c3` | Pure engineering mock: hold 3 seconds and cancellation window |
| `xiao_sim7600_bench` | Experimental physical modem adapter; requires explicit contact/operator qualification |

`diagram.json` and `wokwi.toml` select the jury profile. Hardware wiring is different from Wokwi; read [hardware/README.md](hardware/README.md) before using physical parts. Do not load the physical bench firmware into the simulated circuit.

## Build and test

From the repository root:

```sh
python manage.py setup-dev
python manage.py build
python manage.py test
```

On the existing Windows workstation, `scripts/build-local.ps1` uses ignored portable tools and a temporary drive mapping to avoid the ESP32 compiler's Windows command-line-length limit. Other workstations can use a normal PlatformIO installation.

Native tests compile the production C++ controller, button/fall logic, mock transport and SIM7600 adapter. Modem tests use scripted UART transcripts, not cellular hardware. Wokwi scenarios are under `scenarios/`; do not run a scenario while a real call-capable relay is connected unless you intend a phone call.

## Console

The engineering mock supports newline-terminated `NET ON`, `NET OFF`, `DELAY <ms>`, `DROP ON/OFF`, `RING`, `ANSWER`, `BUSY`, `NOANSWER`, `HANGUP`, `ERROR`, `FALL`, `STATUS` and `HELP`. Relay-generated status commands use the same mock event interface. The physical profile accepts only `STATUS` and `HELP`.

The possible-fall input is an experimental impact/inactivity heuristic or a clearly labelled injected test event. It is not a validated medical fall detector.

## Hardware references

[Diagram](hardware/system.svg) · [BOM](hardware/bom.csv) · [wiring](hardware/wiring.csv) · [hardware assumptions](hardware/README.md)

The schematic is a draft interface study. No standalone modem/SIM/VoLTE, power/charging/battery, antenna, audio or enclosure-fit claim is established by software tests. Verification details are kept in [../docs/VERIFICATION.md](../docs/VERIFICATION.md).
