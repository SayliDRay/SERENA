#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include "sim7600_modem.h"
#include "controller.h"
using namespace nora;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__func__) + ": " #x); } while (0)
class ScriptedUart : public ByteStream {
 public:
  std::deque<char> rx;
  std::string tx;
  int read() override { if (rx.empty()) return -1; char c=rx.front(); rx.pop_front(); return c; }
  void write(const char* text) override { tx += text; }
  void receive(const std::string& text) { for (char c : text) rx.push_back(c); }
};
struct Rig {
  ScriptedUart uart;
  Sim7600Modem modem;
  uint32_t now=100;
  Rig() : modem(uart,"+33600000000",true) {} // Test string only, never connected to a modem.
  ModemEvent reply(const std::string& text) { uart.receive(text); return modem.poll(now+=20); }
  void ready(bool registered=true) {
    modem.checkNetwork(now);
    CHECK(uart.tx == "AT\r");
    for (int step=0; step<7; ++step)
      CHECK(reply(step==3 ? "+CPIN: READY\r\nOK\r\n" : "OK\r\n") == ModemEvent::None);
    CHECK(uart.tx.find("AT+CNMP=38\r") != std::string::npos);
    CHECK(uart.tx.find("AT+CSDVC=1\r") != std::string::npos);
    CHECK(reply(registered ? "+CEREG: 1,1\r\nOK\r\n" : "+CEREG: 1,2\r\nOK\r\n") ==
      (registered ? ModemEvent::NetworkReady : ModemEvent::NoNetwork));
  }
  void dial() { ready(); modem.dialFamily(now); CHECK(reply("OK\r\n") == ModemEvent::Accepted); }
  void query() { now+=1001; CHECK(modem.poll(now)==ModemEvent::None); }
};
void config_gate() {
  ScriptedUart uart;
  for (const char* number : {"", "112", "+33;ATD112", "+0123456789", "+3312\rATH"}) {
    Sim7600Modem m(uart,number,true); m.checkNetwork(0);
    CHECK(m.poll(1)==ModemEvent::Error); CHECK(uart.tx.empty());
  }
  Sim7600Modem m(uart,"+33600000000",false); m.checkNetwork(0);
  CHECK(m.poll(1)==ModemEvent::Error); CHECK(uart.tx.empty());
  Controller controller(m);
  controller.tick(10,false,false); controller.tick(20,true,true);
  controller.tick(3020,true,false); CHECK(controller.state()==State::CANCEL_WINDOW);
  controller.tick(8020,false,false); CHECK(controller.state()==State::CHECKING_NETWORK);
  controller.tick(8021,false,false); CHECK(controller.state()==State::FAILED);
  CHECK(uart.tx.empty());
}
void voice_progress() {
  Rig r; r.dial();
  CHECK(r.uart.tx.find("ATD+33600000000;\r") != std::string::npos);
  CHECK(r.reply("RING\r\nCONNECT\r\nVOICE CALL: BEGIN\r\n") == ModemEvent::None);
  r.query();
  CHECK(r.reply("+CLCC: 2,1,4,0,0\r\n+CLCC: 1,0,3,0,0\r\nOK\r\n") == ModemEvent::Ringing);
  r.query(); CHECK(r.reply("+CLCC: 1,0,0,0,0\r\nOK\r\n") == ModemEvent::Answered);
  r.query(); CHECK(r.reply("+CLCC: 1,0,2,1,0\r\nOK\r\n") == ModemEvent::RemoteHangup);
}
void no_network_sim_fault() {
  Rig r; r.ready(false);
  r.modem.dialFamily(r.now); CHECK(r.modem.poll(++r.now)==ModemEvent::Error);
  CHECK(r.uart.tx.find("ATD")==std::string::npos);
  Rig sim; sim.modem.checkNetwork(sim.now);
  for (int i=0;i<3;++i) CHECK(sim.reply("OK\r\n")==ModemEvent::None);
  CHECK(sim.reply("+CPIN: SIM PIN\r\nOK\r\n")==ModemEvent::Error);
}
void terminal_results() {
  struct Example { const char* text; ModemEvent event; };
  Example examples[] = {{"BUSY\r\n",ModemEvent::Busy},{"NO ANSWER\r\n",ModemEvent::NoAnswer},
    {"NO CARRIER\r\n",ModemEvent::RemoteHangup},{"VOICE CALL: END: 000001\r\n",ModemEvent::RemoteHangup},
    {"+CEREG: 0\r\n",ModemEvent::NetworkLost},{"+CME ERROR: 30\r\n",ModemEvent::Error}};
  for (const auto& item : examples) { Rig r; r.dial(); CHECK(r.reply(item.text)==item.event); }
}
void stop_serialization() {
  Rig r; r.ready(); r.modem.dialFamily(r.now); r.modem.hangup();
  CHECK(r.modem.stopping()); CHECK(r.uart.tx.find("AT+CHUP")==std::string::npos);
  CHECK(r.reply("OK\r\n")==ModemEvent::None); // Dial's OK is never reported as acceptance.
  CHECK(r.uart.tx.find("AT+CHUP\r")!=std::string::npos); CHECK(r.modem.stopping());
  CHECK(r.reply("+CLCC: 1,0,0,0,0\r\nVOICE CALL: END: 000000\r\nOK\r\n")==ModemEvent::None);
  CHECK(!r.modem.stopping());
  r.modem.dialFamily(r.now); CHECK(r.modem.poll(++r.now)==ModemEvent::Error); // Requires fresh check.
  Rig query; query.dial(); query.query();
  CHECK(query.reply("NO CARRIER\r\nOK\r\n")==ModemEvent::RemoteHangup);
  query.modem.hangup(); CHECK(query.uart.tx.find("AT+CHUP")==std::string::npos);
  CHECK(query.modem.poll(++query.now)==ModemEvent::None); // Drains CLCC's trailing OK.
  CHECK(query.uart.tx.find("AT+CHUP\r")!=std::string::npos); CHECK(query.modem.stopping());
  CHECK(query.reply("OK\r\n")==ModemEvent::None); CHECK(!query.modem.stopping());
}
void timeout_rollover() {
  Rig r; r.now=0xfffffff0; r.modem.checkNetwork(r.now);
  CHECK(r.modem.poll(r.now+9999)==ModemEvent::None);
  CHECK(r.modem.poll(r.now+10000)==ModemEvent::Error); CHECK(r.modem.faulted());
  std::string before=r.uart.tx; r.modem.checkNetwork(r.now+20000);
  CHECK(r.modem.poll(r.now+20001)==ModemEvent::Error); CHECK(r.uart.tx==before);
}
void fragmented_and_oversize() {
  Rig r; r.modem.checkNetwork(r.now);
  CHECK(r.reply("O")==ModemEvent::None); CHECK(r.reply("K\r")==ModemEvent::None);
  CHECK(r.reply("\n")==ModemEvent::None); CHECK(r.uart.tx=="AT\rATE0\r");
  CHECK(r.reply(std::string(220,'x')+"\r\n")==ModemEvent::Error); CHECK(r.modem.faulted());
}
int main() {
  struct Test { const char* name; void (*run)(); };
  Test tests[] = {{"configuration blocks unqualified dialing",config_gate},
    {"voice progress and incoming/data rejection",voice_progress},
    {"network and SIM faults",no_network_sim_fault},{"terminal call outcomes",terminal_results},
    {"serialized stop discards old progress",stop_serialization},{"timeout rollover and fault lock",timeout_rollover},
    {"fragmented and oversized UART lines",fragmented_and_oversize}};
  int failures=0;
  for (const auto& t : tests) {
    try { t.run(); std::cout<<"PASS "<<t.name<<'\n'; }
    catch (const std::exception& e) { ++failures; std::cerr<<"FAIL "<<e.what()<<'\n'; }
  }
  std::cout<<"7 SIM7600 transcript groups, "<<failures<<" failures\n";
  return failures ? 1 : 0;
}
