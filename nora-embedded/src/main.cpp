#include <Arduino.h>
#include <cstring>
#include <cstdlib>
#include "button.h"
#include "controller.h"
#include "mock_modem.h"
#include "motion_sensor.h"
#ifdef NORA_REAL_MODEM
#include "sim7600_modem.h"
#if __has_include("hardware_config.local.h")
#include "hardware_config.local.h"
#else
#include "hardware_config.example.h"
#endif
#endif
using namespace nora;
#ifdef NORA_REAL_MODEM
HardwareSerial modemSerial(1);
class UartBytes : public ByteStream {
 public:
  int read() override { return modemSerial.available() ? modemSerial.read() : -1; }
  void write(const char* text) override { modemSerial.print(text); }
} uartBytes;
Sim7600Modem modem(uartBytes, NORA_FAMILY_NUMBER, NORA_OPERATOR_VOICE_VERIFIED);
Config benchTiming() { Config c; c.networkTimeout = 120000; c.acceptTimeout = 120000; return c; }
Controller controller(modem, benchTiming());
Button sos;
#else
MockModem modem;
#ifdef NORA_JURY_DEMO
Config juryTiming() { Config c; c.shortPressCall=true; c.manualCancel=0; c.answerTimeout=60000; return c; }
Controller controller(modem, juryTiming());
uint32_t relayBoot = 0, relaySequence = 0;
#else
Controller controller(modem);
#endif
Button sos, fallTest;
#endif
MotionSensor motion;
bool injectFall = false;
char command[64];
size_t commandSize = 0;
bool commandOverflow = false;

void help() {
#ifdef NORA_REAL_MODEM
  Serial.println("PHYSICAL BENCH SIM7600: STATUS | HELP. Mock/injection controls disabled.");
#else
  Serial.println("NORA MOCK ONLY (not AT/LTE/eSIM/audio). Commands, newline terminated:");
  Serial.println("NET ON | NET OFF | DELAY <0..60000 ms> | DROP ON | DROP OFF");
  Serial.println("RING | ANSWER | BUSY | NOANSWER | HANGUP | FALL | STATUS | HELP");
#endif
}
void status() {
#ifdef NORA_REAL_MODEM
  Serial.printf("STATE %s | %s | sensor=%s | HARDWARE configured=%d fault=%d stopPending=%d\n",
    stateName(controller.state()), controller.reason(), motion.healthy() ? "OK" : "UNAVAILABLE",
    modem.configured(), modem.faulted(), modem.stopping());
#else
  Serial.printf("STATE %s | %s | armed=%d sensor=%s | MOCK net=%d delay=%lu drop=%d\n",
    stateName(controller.state()), controller.reason(), controller.armed(),
    motion.healthy() ? "OK" : "UNAVAILABLE", modem.network,
    static_cast<unsigned long>(modem.responseDelay), modem.dropResponses);
#endif
}
void executeCommand() {
#ifdef NORA_REAL_MODEM
  if (!strcmp(command, "STATUS")) status();
  else if (!strcmp(command, "HELP")) help();
  else Serial.println("ERR hardware profile accepts STATUS or HELP only");
#else
  if (!strcmp(command, "NET ON")) modem.setNetwork(true);
  else if (!strcmp(command, "NET OFF")) modem.setNetwork(false);
  else if (!strcmp(command, "DROP ON")) modem.dropResponses = true;
  else if (!strcmp(command, "DROP OFF")) modem.dropResponses = false;
  else if (!strncmp(command, "DELAY ", 6)) {
    char* end; long value = strtol(command + 6, &end, 10);
    if (end == command + 6 || *end || value < 0 || value > 60000) {
      Serial.println("ERR DELAY range 0..60000"); return;
    }
    modem.responseDelay = static_cast<uint32_t>(value);
  } else if (!strcmp(command, "FALL")) injectFall = true;
  else if (!strcmp(command, "HELP")) { help(); return; }
  else if (!strcmp(command, "STATUS")) { status(); return; }
  else {
    ModemEvent event = ModemEvent::None;
    if (!strcmp(command, "RING")) event = ModemEvent::Ringing;
    else if (!strcmp(command, "ANSWER")) event = ModemEvent::Answered;
    else if (!strcmp(command, "BUSY")) event = ModemEvent::Busy;
    else if (!strcmp(command, "NOANSWER")) event = ModemEvent::NoAnswer;
    else if (!strcmp(command, "HANGUP")) event = ModemEvent::RemoteHangup;
    else if (!strcmp(command, "ERROR")) event = ModemEvent::Error;
    if (event == ModemEvent::None) { Serial.println("ERR unknown command; HELP"); return; }
    Serial.println(modem.inject(event) ? "MOCK event queued" : "IGNORED: no accepted call or event pending");
    return;
  }
  Serial.printf("MOCK control: %s\n", command);
#endif
}
void readCommands() {
  // Bound work per tick so a flood cannot starve the state machine.
  for (int budget = 0; budget < 64 && Serial.available(); ++budget) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      command[commandSize] = 0;
      if (commandOverflow) Serial.println("ERR line too long");
      else if (commandSize) executeCommand();
      commandSize = 0; commandOverflow = false;
    } else if (commandSize < sizeof(command)-1) command[commandSize++] = c;
    else commandOverflow = true;
  }
}
void feedback(uint32_t now) {
  State state = controller.state();
  uint32_t age = uint32_t(now - controller.stateSince());
  bool warning = state == State::CANCEL_WINDOW;
  bool pulse = warning && (age % 500 < 180);
  bool confirmation = warning && age < 300;
  // One shared status LED conserves pins: READY heartbeat, CONNECTED steady.
  digitalWrite(pins::status, state == State::CONNECTED ||
    (state == State::READY && controller.armed() && age % 1500 < 150));
#ifdef NORA_REAL_MODEM
  // External motor MOSFET + red LED share this signal to fit the XIAO pin budget.
  digitalWrite(pins::alert, pulse || confirmation ||
    (state == State::FAILED && age % 1000 < 150) ||
    ((state == State::HOLDING_SOS || state == State::CHECKING_NETWORK || state == State::DIALING)
      && age % 1500 < 100));
#else
  digitalWrite(pins::alert, pulse || state == State::FAILED ||
    state == State::HOLDING_SOS || state == State::CHECKING_NETWORK || state == State::DIALING);
  digitalWrite(pins::motorIndicator, pulse || confirmation);
#endif
  bool audible = pulse || confirmation;
  static bool wasAudible = false;
  if (audible != wasAudible) {
    if (audible) ledcWriteTone(0, 1800); else ledcWriteTone(0, 0);
    wasAudible = audible;
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(pins::sos, INPUT_PULLUP);
#ifndef NORA_REAL_MODEM
  pinMode(pins::fallTest, INPUT_PULLUP);
#else
  modemSerial.begin(115200, SERIAL_8N1, pins::modemRx, pins::modemTx);
#endif
  for (int pin : {pins::status, pins::alert, pins::motorIndicator}) {
    pinMode(pin, OUTPUT); digitalWrite(pin, LOW);
  }
  ledcSetup(0, 1800, 8); ledcAttachPin(pins::buzzer, 0); ledcWriteTone(0, 0);
#ifdef NORA_REAL_MODEM
  Serial.println("NORA+ EXPERIMENTAL PHYSICAL BENCH / SIM7600 / NOT WOKWI");
  if (!modem.configured()) Serial.println("CALLS DISABLED: configure family contact and qualify operator VoLTE first");
#else
  Serial.println("NORA+ XIAO ESP32C3 simulation; family contact = MOCK_FAMILY_SLOT");
#endif
  Serial.println(motion.begin() ? "MPU6050 ready: experimental heuristic" :
    "FAULT MPU6050 unavailable: manual SOS and labelled injection remain usable");
  // Seed before the first controller tick: a button held at boot must be released.
  sos.update(millis(), digitalRead(pins::sos) == LOW);
#ifndef NORA_REAL_MODEM
  fallTest.update(millis(), digitalRead(pins::fallTest) == LOW);
#endif
#ifdef NORA_JURY_DEMO
  relayBoot = esp_random(); modem.responseDelay=20;
  Serial.printf("SERENA_BOOT:%08lx\n",static_cast<unsigned long>(relayBoot));
  Serial.println("JURY DEMO: short SOS calls through computer relay; long 2s hangs up. Not cellular hardware.");
#endif
  help(); status();
}
void loop() {
  uint32_t now = millis();
  sos.update(now, digitalRead(pins::sos) == LOW);
#ifndef NORA_REAL_MODEM
  fallTest.update(now, digitalRead(pins::fallTest) == LOW);
#endif
  readCommands();
  bool eligible = controller.state() == State::READY;
  bool sensorFall = motion.update(now, eligible);
  bool injected = false;
#ifndef NORA_REAL_MODEM
  injected = injectFall || fallTest.pressed(); injectFall = false;
#endif
  if (injected) Serial.println(eligible ? "INJECTED suspected fall" : "IGNORED injected fall: workflow active");
  State before = controller.state();
  const char* reasonBefore = controller.reason();
  controller.tick(now, sos.down(), sos.pressed(), injected || sensorFall,
    injected ? Source::Injected : Source::Sensor);
#ifdef NORA_JURY_DEMO
  if (before != controller.state() && controller.state()==State::DIALING)
    Serial.printf("SERENA_DIAL:%08lx:%lu\n",static_cast<unsigned long>(relayBoot),static_cast<unsigned long>(++relaySequence));
  if ((before==State::DIALING || before==State::CONNECTED) &&
      (controller.state()==State::CALL_ENDED || controller.state()==State::FAILED))
    Serial.printf("SERENA_STOP:%08lx:%lu\n",static_cast<unsigned long>(relayBoot),static_cast<unsigned long>(relaySequence));
#endif
  if (before != controller.state() || reasonBefore != controller.reason()) status();
  feedback(now);
}
