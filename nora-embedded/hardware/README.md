# NORA+ physical calling reference — France

Open [system.svg](system.svg) to see all subsystems and [wiring.csv](wiring.csv) for the bench connections. This is a **draft interface schematic**, not a fabrication-ready PCB. No physical hardware, charging circuit, antenna or audio path was tested. The executable Wokwi circuit remains a mock. Country: **France**; operator: **not supplied**.

## What is added

- A separate `xiao_sim7600_bench` build using a nonblocking SIM7600 UART adapter instead of `MockModem`.
- A reference bench modem: **Waveshare SIM7600E-H 4G HAT**, with its onboard codec/audio connector, SIM slot and UART level translation. This is a temporary external development board, not the bracelet modem selection. Its documented outline is 56.21 × 65.15 mm, so it is not claimed to fit the bracelet. It must be a voice-capable exact variant/firmware qualified with the intended operator.
- LTE main/diversity antenna connections, compatible headset microphone/earpiece, real motor/piezo driver wiring, and a rechargeable-battery/charger/power-distribution plan.
- A BOM separating usable bench parts from still-undecided battery, regulator, eSIM and production audio/PCB parts. Nothing was ordered.

The default environment and `wokwi.toml` still select **`xiao_esp32c3`**, which never sends real AT commands. Wokwi's reference labels do not simulate the added hardware. Do not use the simulation's yellow-LED and buzzer pin connections on the hardware profile: those pins become the UART.

## Bench wiring

Use the **level-shifted HAT control header**, not bare SIM7600 module pins. Configure the HAT VCCIO jumper for **3.3 V** and route its UART to the external controller, with no second UART transmitter driving the same RX net. Confirm the exact HAT revision's labels/jumpers against its manual. `TXD` means the HAT's output; `RXD` means its input.

| XIAO header / GPIO | Physical connection | Rail / role |
| --- | --- | --- |
| D1 / 3 | SOS switch to GND, firmware INPUT_PULLUP | 3.3 V input, same hold/debounce behavior |
| D4 / 6 | MPU6050 SDA, 4.7 kΩ pull-up to 3V3 | Shared I2C data |
| D5 / 7 | MPU6050 SCL, 4.7 kΩ pull-up to 3V3 | I2C clock, 100 kHz |
| 3V3 / GND | MPU6050 VCC/GND; AD0 tied to GND | 0x68 sensor; XDA/XCL/INT unused |
| D6 / 21 (UART TX) | HAT **RXD**, 3.3 V level-shifted header | 115200 baud, 8N1 |
| D7 / 20 (UART RX) | HAT **TXD**, 3.3 V level-shifted header | Never connect a 5 V UART directly |
| GND | HAT GND and all regulator/driver returns | Common reference; high-current returns routed separately on PCB |
| D10 / 10 | 330 Ω → green LED anode; cathode GND | READY heartbeat / CONNECTED steady |
| D3 / 5 | 330 Ω → red LED; also 100 Ω → Q1 gate, 100 kΩ gate-to-source pull-down | Shared alert/vibration signal to conserve pins |
| D2 / 4 | 100 Ω → Q2 gate, 100 kΩ gate-to-source pull-down | Separate local passive-piezo warning output; **no fall test button** |
| USB | Programming/debug console | No smartphone needed; UART is separate |
| GPIO2/8/9 | Leave unconnected | Boot straps avoided |

Q1/Q2 reference: AO3400A logic-level N-MOSFET (verify actual package pins). Sources go to GND. Q1 drain goes to motor negative; motor positive goes to a **regulated rail within the chosen motor's rating**. A flyback diode goes across the ERM motor, cathode at motor positive, anode at Q1 drain; select diode current/voltage for the motor. Q2 drain switches the piezo's negative side; positive side goes to its rated supply. A passive piezo gets the 1.8 kHz firmware waveform; a magnetic buzzer needs its own inductive protection and load check. Neither load is powered by a GPIO. Motor current, rated voltage, acoustic output and gate/EMI behavior still need measurement. The shared red/motor signal pulses during countdown, holding/network/dialing progress and faults in the hardware profile.

For initial audio, use a **headset with microphone compatible with the HAT's 3.5 mm earphone/mic connector**, which uses the HAT codec. The firmware requests `AT+CSDVC=1`. Do not connect a bare loudspeaker to ESP32 GPIO or assume a headset output can drive an arbitrary speaker. A production loudspeaker/microphone needs the chosen modem/codec reference circuit, a rated amplifier if required, acoustic design and echo testing. The exact jack pin order and headset compatibility must be checked on the purchased board revision; no CTIA/OMTP assumption is made here.

For initial modem power, use the HAT's specified **5 V input from a regulated bench supply**, with supply/connector/cable capability checked under transmit bursts, independently of the XIAO's USB supply. Join grounds only; do not power the HAT from XIAO 3V3. Attach the correct regional LTE antennas before testing. Use the HAT's own power-key procedure; MCU-controlled startup/reset is not implemented in this bench profile.

## Rechargeable power architecture (reference only)

`USB 5 V charging input → ESD/input protection and correct USB current negotiation → BQ24074 charger/power-path candidate → SYS → properly sized regulated rails`.

A protected **one-cell Li-ion/Li-polymer pack**, with a thermistor and a connector of verified polarity, connects to the charger BAT path. Cell size, capacity, charge rate, discharge-pulse rating, protection limits and thermal behavior are **not selected**. Set charge/input-current resistors and thermistor network using the chosen cell and charger datasheets; the IC's maximum charge rating is not a recommended charge current for a small bracelet cell.

- The BQ24074 reference uses a 4.2 V cell charge voltage and can have a **4.4 V SYS output**. SYS is therefore **not** a 3.3 V rail and **not** the HAT's required regulated 5 V supply. Do not wire it directly to ESP32 3V3 or to a bare modem supply assuming compatibility.
- A **5 V regulator**, still to be sized, supplies the HAT. Its output peak current must cover measured transmit bursts, input voltage at low battery, conversion loss and cable drop. Calculate battery current as approximately `Pout / (efficiency × Vbattery)`; a converter's switch-current headline is not its available output current.
- A separate controller rail or an isolated regulated 5 V feed supplies the XIAO according to its board power guide. Add explicit source selection/reverse-current protection before combining USB programming power and battery-derived power. Until that is designed, power the XIAO by USB for bench tests and do not attach a second source.
- A motor rail within the selected actuator's voltage rating is separate from MCU GPIO power. TPS63020 is a possible regulated-rail family to evaluate, not a confirmed 5 V modem supply. Check the relevant load curves, inductor, capacitors, thermal loss and PCB layout.
- Leave the XIAO battery pads unconnected in this external-charger architecture: do not parallel two chargers. Cell protection is separate from charging and remains required. Charging status comes from the charger status pins through appropriate indicators; it is not implemented by the current firmware.

The diagram shows the complete power path concept, not selected passive values or an electrically checked charging board. Power integrity, runtime, charging operation and battery fit are not validated by Wokwi or these firmware tests.

## SIM/eSIM and France

The bench HAT uses a removable SIM. **No eSIM has been added or provisioned.** The final eUICC/modem/operator combination is pending: a SIM socket or AT driver does not provide eSIM service. A consumer phone eSIM cannot be assumed to transfer to an embedded modem.

Before purchasing a modem or enabling the hardware contact, obtain the exact French operator, its voice-capable subscription, support for this module/firmware's IMS/VoLTE profile, and required regional bands/certifications. LTE data registration is not proof of voice service. The driver selects **LTE-only** mode, so it does not hide a 2G/3G voice fallback. No operator compatibility or future-network availability is claimed. A compact Quectel EG915U-EU is another production candidate to investigate, not a replacement selected or supported by this SIM7600-specific driver.

## Build and configure the physical profile

From the parent workspace, using the tools already provisioned here:

```powershell
.\nora-embedded\scripts\build-local.ps1 -Environment xiao_sim7600_bench
python nora-embedded/tests/run_tests.py
```

With a normal PlatformIO installation, from `nora-embedded`:

```powershell
pio run -e xiao_sim7600_bench
```

The hardware build compiles with calls **disabled**. To configure a real bench assembly, copy `include/hardware_config.example.h` to ignored `include/hardware_config.local.h`, enter a consented family contact as `+33...` (omit the leading French `0`), and set `NORA_OPERATOR_VOICE_VERIFIED` true only after real operator/module/firmware/audio qualification. The empty default cannot dial. The parser rejects nonnumeric characters, semicolons and control characters in the number. The contact is not printed to the console, though a configured number will be present in the resulting firmware binary.

Only after physical wiring and qualification, upload deliberately with `pio run -e xiao_sim7600_bench -t upload`, then use `pio device monitor -b 115200`. No upload or call was performed by this task. The console accepts **STATUS** and **HELP** only in the hardware profile; injected falls and mocked ANSWER/NET commands are disabled. Manual SOS and sensor-derived experimental events can request calls when the profile is configured. The physical SOS hold/cancel process is the same. This remains a bench prototype, not a deployed elder-safety device.

## AT adapter behavior and limits

`src/sim7600_modem.cpp` uses SIMCom **SIM7500/SIM7600 Series AT Command Manual V3.00, 2021-11-18**. UART commands terminate in CR; received lines use CR/LF. Transactions are serialized. The setup sequence is `AT`, `ATE0`, `AT+IFC=0,0`, `AT+CPIN?`, `AT+CNMP=38`, `AT+CEREG=1`, `AT+CSDVC=1`, then `AT+CEREG?`. A ready SIM and EPS registration state 1 or 5 are required; IMS support is a separate prequalification condition.

Voice dialing uses `ATD<number>;`. Its `OK` emits **Accepted**, never Answered. `AT+CLCC` samples outgoing voice-call records every 1 s when idle: state 3 emits Ringing, state 0 emits Answered, and state 6 or a completed empty list indicates remote end. Incoming `RING`, incoming call records and data bearer records cannot connect the SOS session. BUSY, NO ANSWER, NO CARRIER, voice end records and registration-loss URCs have separate outcomes. Unsolicited `VOICE CALL: BEGIN` or generic CONNECT alone is not taken as answer proof.

Local termination queues `AT+CHUP` after the pending transaction and suppresses old progress. Until its OK, `STATUS` reports `stopPending=1` and new operations are refused. The controller's CALL_ENDED result means termination was **requested**, not that the physical network release was measured. Inspect stop status on the bench. On command timeout/overflow/error, the adapter tries CHUP for an uncertain call, locks out further use and requires modem/controller recovery; this best-effort stop is not a guarantee that the network hung up. A physical power/reset control and stronger recovery design remain production work.

Explicit application deviations: each AT transaction has a **10-second host watchdog**, shorter than some manual maxima (e.g. CLCC/CHUP up to 120 seconds); the host aborts and reports a fault rather than waiting the entire modem maximum. Controller network/acceptance ceilings are 120 seconds in the bench build, and the answer window remains 30 seconds. No retry/fallback calling, emergency-number routing, automatic PIN entry, IMS provisioning, audio calibration, unsolicited incoming-call answering, power-key automation or battery telemetry is implemented. The HAT must be powered and registered before SOS qualification; the driver is not a full modem power manager.

## Verification and next physical test

Run `python tests/run_tests.py`: it exercises the original controller/heuristic and the SIM7600 adapter with **scripted UART transcripts**, not a cellular simulator. Tests check config rejection, outgoing voice state parsing, distinction from incoming/data events, network/SIM faults, terminal outcomes, serialized stop, timeouts/rollover and fragmented/oversized input.

The next physical test is to qualify the selected operator with the exact HAT firmware and compatible headset while measuring the 5 V rail during transmit. Then check SOS cancellation before any dialing, acceptance/ringing/answer distinctions, both directions of audio, busy/no-answer, local stop confirmation and modem disconnect. Battery operation follows only after the rail and charger design is specified. Hardware components, operator subscription, family number, eSIM solution and electrical/acoustic/fit qualification are remaining requirements.

## Official sources checked 2026-11-03

- [Waveshare HAT manual Rev1.0](https://files.waveshare.com/upload/6/6d/SIM7600E-H-4G-HAT-Manual-EN.pdf): 5 V input, VCCIO selection, header, codec/headset, antennas, power-key and development-board dimensions.
- [SIMCom AT manual V3.00](https://files.waveshare.com/upload/a/af/SIM7500_SIM7600_Series_AT_Command_Manual_V3.00.pdf): §§2.2.2, 2.2.14, 2.2.16, 3.2.2, 4.2.11, 4.2.16, 5.2.2, 5.2.7, 5.2.47. The precise commands, events and host-watchdog deviations are identified above; no mock commands are represented as SIMCom commands.
- [SIMCom product/download page](https://www.simcom.com/product/SIM7600.html): exact hardware/firmware documentation must match the purchased variant.
- [TI BQ24074](https://www.ti.com/product/BQ24074) and [TI TPS63020](https://www.ti.com/product/TPS63020): charger/power-path and converter operating ranges; output capability still needs design verification.
- [AO3400A manufacturer datasheet](https://www.aosmd.com/res/data_sheets/AO3400A.pdf): logic-level MOSFET candidate, package and ratings.
- [Seeed XIAO guide](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/): controller GPIO and power-source constraints.
- [Quectel EG915U hardware design V1.1](https://quectel.com/content/uploads/2024/02/Quectel_EG915U_Series_Hardware_Design_V1.1.pdf): example compact-module interface study only; no wiring or driver interchangeability with the HAT is implied. Obtain the latest revision before a production design.

€10 remains a future volume target. No prototype price, battery life, enclosure fit or manufacturing-ready design is claimed.
