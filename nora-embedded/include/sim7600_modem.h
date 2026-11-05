#pragma once
#include "byte_stream.h"
#include "modem.h"
#include "config.h"
namespace nora {
// Experimental bench adapter for SIMCom SIM7500/SIM7600 AT Manual V3.00,
// 2021-11-18. Not used by Wokwi. See hardware/README.md for required setup.
class Sim7600Modem : public ModemTransport {
 public:
  Sim7600Modem(ByteStream& stream, const char* number, bool voiceQualified)
    : stream_(stream), number_(number), qualified_(voiceQualified) {}
  void checkNetwork(uint32_t now) override;
  void dialFamily(uint32_t now) override;
  void hangup() override;
  ModemEvent poll(uint32_t now) override;
  bool configured() const;
  bool faulted() const { return fault_; }
  bool stopping() const { return stopping_; }
 private:
  enum class Command { None, Setup, Registration, Dial, Calls, Stop };
  void send(Command command, const char* text, uint32_t now);
  void line(uint32_t now);
  void emit(ModemEvent event);
  void fail();
  ByteStream& stream_;
  const char* number_;
  bool qualified_, fault_ = false, stopping_ = false, active_ = false;
  bool accepted_ = false, registered_ = false, simReady_ = false, ready_ = false;
  bool callSeen_ = false, wasConnected_ = false, overflow_ = false;
  int setupStep_ = 0, callId_ = -1;
  Command command_ = Command::None;
  uint32_t sent_ = 0, lastPoll_ = 0, lastNow_ = 0;
  char buffer_[192] = {};
  size_t length_ = 0;
  ModemEvent event_ = ModemEvent::None, progress_ = ModemEvent::None;
};
}
