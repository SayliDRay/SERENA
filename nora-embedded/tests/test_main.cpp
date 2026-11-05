#include <iostream>
#include <stdexcept>
#include <limits>
#include "button.h"
#include "controller.h"
#include "fall_detector.h"
#include "mock_modem.h"
using namespace nora;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__func__) + ": " #x); } while (0)
struct Rig {
  uint32_t now;
  MockModem modem;
  Controller c;
  explicit Rig(uint32_t start = 0) : now(start), c(modem) { tick(); }
  void tick(bool down = false, bool press = false, bool fall = false, Source source = Source::Injected) {
    c.tick(now, down, press, fall, source);
  }
  void advance(uint32_t ms, bool down = false, bool press = false) { now += ms; tick(down, press); }
  void activate() {
    tick(true, true); advance(2999, true); CHECK(c.state() == State::HOLDING_SOS);
    advance(1, true); CHECK(c.state() == State::CANCEL_WINDOW);
  }
  void dialing() {
    activate(); advance(5000); CHECK(c.state() == State::CHECKING_NETWORK);
    advance(500); CHECK(c.state() == State::DIALING);
    advance(500); CHECK(c.dialStage() == DialStage::Accepted);
  }
  void connected() {
    dialing(); CHECK(modem.inject(ModemEvent::Ringing)); tick();
    CHECK(c.dialStage() == DialStage::Ringing); CHECK(c.state() == State::DIALING);
    CHECK(modem.inject(ModemEvent::Answered)); tick(); CHECK(c.state() == State::CONNECTED);
  }
  void returnReadyHeld() {
    advance(2000, true); CHECK(c.state() == State::READY); CHECK(!c.armed());
    advance(5000, true, true); CHECK(c.state() == State::READY);
    tick(); CHECK(c.armed());
  }
};
void short_press() {
  Rig r; r.tick(true,true); r.advance(2999); CHECK(r.c.state() == State::READY);
}
void hold_cancel() {
  Rig r; r.activate(); r.advance(1000, true, true);
  CHECK(r.c.state() == State::CANCEL_WINDOW); // even spurious repeated edge cannot self-cancel
  r.tick(); r.advance(1,true,true); CHECK(r.c.state() == State::CALL_ENDED);
  r.returnReadyHeld();
}
void continuous_hold_to_answer() {
  Rig r; r.activate(); r.advance(5000,true); r.advance(500,true); r.advance(500,true);
  CHECK(r.modem.inject(ModemEvent::Answered)); r.tick(true); CHECK(r.c.state() == State::CONNECTED);
  r.advance(1,true,true); CHECK(r.c.state() == State::CONNECTED);
  r.tick(); r.tick(true,true); CHECK(r.c.state() == State::CALL_ENDED);
}
void unavailable() {
  Rig r; r.modem.network = false; r.activate(); r.advance(5000); r.advance(500);
  CHECK(r.c.state() == State::FAILED); r.returnReadyHeld();
}
void timeouts() {
  Rig r; r.modem.dropResponses = true; r.activate(); r.advance(5000);
  r.advance(7999); CHECK(r.c.state() == State::CHECKING_NETWORK);
  r.advance(1); CHECK(r.c.state() == State::FAILED);
  Rig d; d.activate(); d.advance(5000); d.advance(500);
  d.modem.dropResponses = true; d.advance(8000); CHECK(d.c.state() == State::FAILED);
  Rig a; a.dialing(); a.advance(29999); CHECK(a.c.state() == State::DIALING);
  CHECK(a.modem.inject(ModemEvent::Ringing)); a.tick();
  a.advance(1); CHECK(a.c.state() == State::FAILED); // ringing cannot extend deadline
}
void delayed_response() {
  Rig r; r.modem.responseDelay = 7999; r.activate(); r.advance(5000);
  r.advance(7998); CHECK(r.c.state() == State::CHECKING_NETWORK);
  r.advance(1); CHECK(r.c.state() == State::DIALING);
  Rig late; late.modem.responseDelay = 9000; late.activate(); late.advance(5000);
  late.advance(8000); CHECK(late.c.state() == State::FAILED);
  late.advance(2000); CHECK(late.c.state() == State::READY);
  late.advance(10000); CHECK(late.c.state() == State::READY);
}
void remote_outcomes() {
  for (auto event : {ModemEvent::Busy, ModemEvent::NoAnswer}) {
    Rig r; r.dialing(); CHECK(r.modem.inject(event)); r.tick(); CHECK(r.c.state() == State::FAILED);
  }
  Rig r; r.connected(); CHECK(r.modem.inject(ModemEvent::RemoteHangup)); r.tick();
  CHECK(r.c.state() == State::CALL_ENDED); r.returnReadyHeld();
}
void local_hangup_and_network_loss() {
  Rig r; r.connected(); r.tick(); r.tick(true,true); CHECK(r.c.state() == State::CALL_ENDED);
  r.returnReadyHeld();
  Rig n; n.connected(); n.modem.setNetwork(false); n.tick(); CHECK(n.c.state() == State::FAILED);
}
void fall_workflow() {
  Rig r; r.tick(false,false,true); CHECK(r.c.source() == Source::Injected);
  r.advance(19999); CHECK(r.c.state() == State::CANCEL_WINDOW);
  r.tick(true,true); CHECK(r.c.state() == State::CALL_ENDED);
  Rig f; f.tick(false,false,true,Source::Sensor); CHECK(f.c.source() == Source::Sensor);
  f.advance(19999); f.tick(false,false,true); f.advance(1);
  CHECK(f.c.state() == State::CHECKING_NETWORK); // repeated fall cannot restart countdown
  f.tick(false,false,true); CHECK(f.c.state() == State::CHECKING_NETWORK);
  Rig call; call.connected(); call.tick(false,false,true); CHECK(call.c.state() == State::CONNECTED);
  Rig dial; dial.dialing(); dial.tick(false,false,true); CHECK(dial.c.state() == State::DIALING);
  Rig held; held.tick(true,false,true); held.advance(1000,true,true);
  CHECK(held.c.state() == State::CANCEL_WINDOW);
  held.tick(); held.tick(true,true); CHECK(held.c.state() == State::CALL_ENDED);
  Rig holding; holding.tick(true,true); holding.tick(true,false,true);
  CHECK(holding.c.state() == State::HOLDING_SOS);
}
void rollover() {
  Rig r(std::numeric_limits<uint32_t>::max()-1500); r.activate();
  r.advance(4999); CHECK(r.c.state() == State::CANCEL_WINDOW);
  r.advance(1); r.advance(500); r.advance(500);
  CHECK(r.modem.inject(ModemEvent::Answered)); r.tick(); CHECK(r.c.state() == State::CONNECTED);
  Rig f(std::numeric_limits<uint32_t>::max()-10000); f.tick(false,false,true);
  f.advance(19999); CHECK(f.c.state() == State::CANCEL_WINDOW);
  f.advance(1); CHECK(f.c.state() == State::CHECKING_NETWORK);
}
void debounce_and_boot() {
  Button b; b.update(0,true); CHECK(b.down()); CHECK(!b.pressed());
  MockModem m; Controller boot(m); boot.tick(0,true,false); boot.tick(5000,true,true);
  CHECK(boot.state() == State::READY); CHECK(!boot.armed());
  boot.tick(5001,false,false); boot.tick(5002,true,true); CHECK(boot.state() == State::HOLDING_SOS);
  b.update(10,false); b.update(20,true); b.update(25,false); b.update(59,false);
  CHECK(b.down()); b.update(60,false); CHECK(b.released());
  b.update(61,false); CHECK(!b.released());
  Button wrap; wrap.update(0xffffffd0,false); wrap.update(0xfffffff0,true);
  wrap.update(0x12,true); CHECK(!wrap.pressed()); wrap.update(0x13,true); CHECK(wrap.pressed());
}
void heuristic() {
  FallDetector f;
  for (uint32_t t=0;t<4000;t+=20) CHECK(!f.sample(t,1,0,true));
  CHECK(!f.sample(4000,3,1,true));
  bool detected = false;
  for (uint32_t t=4020;t<=7500;t+=20) detected = f.sample(t,1,0,true) || detected;
  CHECK(detected);
  f.reset(); f.sample(0,3,0,true);
  for (uint32_t t=20;t<=9000;t+=20) CHECK(!f.sample(t,1,1,true)); // rotating is not inactivity
  f.reset(); f.sample(0,3,0,true); CHECK(!f.sample(5000,1,0,true)); // missing samples invalidate impact
  f.reset(); f.sample(0,3,0,true); f.sample(20,1,0,false);
  for (uint32_t t=40;t<5000;t+=20) CHECK(!f.sample(t,1,0,true));
  f.reset(); uint32_t base = 0xfffff800; f.sample(base,3,0,true); detected = false;
  for (uint32_t dt=20;dt<=3500;dt+=20) detected = f.sample(base+dt,1,0,true) || detected;
  CHECK(detected);
}
void jury_gestures() {
  Config config; config.shortPressCall=true; config.manualCancel=0;
  MockModem modem; modem.responseDelay=0; Controller c(modem,config);
  c.tick(0,false,false);
  c.tick(50,true,true); CHECK(c.state()==State::HOLDING_SOS);
  c.tick(150,false,false); CHECK(c.state()==State::CANCEL_WINDOW);
  c.tick(151,false,false); CHECK(c.state()==State::CHECKING_NETWORK);
  c.tick(152,false,false); CHECK(c.state()==State::DIALING);
  c.tick(153,false,false); CHECK(c.dialStage()==DialStage::Accepted);
  c.tick(200,true,true); c.tick(2199,true,false); CHECK(c.state()==State::DIALING);
  c.tick(2200,true,false); CHECK(c.state()==State::CALL_ENDED);
  MockModem other; Controller held(other,config); held.tick(0,false,false);
  held.tick(10,true,true); held.tick(2010,true,false); CHECK(held.state()==State::CALL_ENDED);
}

int main() {
  struct Test { const char* name; void (*run)(); };
  Test tests[] = {{"jury short call / long hangup",jury_gestures},{"short press",short_press},{"hold and fresh cancel",hold_cancel},
    {"continuous hold through answer",continuous_hold_to_answer},{"no network",unavailable},
    {"timeouts",timeouts},{"delayed response",delayed_response},{"remote outcomes",remote_outcomes},
    {"local hangup and network loss",local_hangup_and_network_loss},{"fall workflow",fall_workflow},
    {"timer rollover",rollover},{"debounce and boot",debounce_and_boot},{"experimental heuristic",heuristic}};
  int failures=0;
  for (const auto& t : tests) {
    try { t.run(); std::cout << "PASS " << t.name << '\n'; }
    catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << e.what() << '\n'; }
  }
  std::cout << (sizeof(tests)/sizeof(tests[0])) << " groups, " << failures << " failures\n";
  return failures ? 1 : 0;
}
