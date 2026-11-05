#pragma once
#include "modem.h"
#include "config.h"
namespace nora {
class MockModem : public ModemTransport {
 public:
  bool network = true, dropResponses = false;
  uint32_t responseDelay = 500;
  void checkNetwork(uint32_t now) override;
  void dialFamily(uint32_t now) override;
  void hangup() override;
  ModemEvent poll(uint32_t now) override;
  void setNetwork(bool available);
  bool inject(ModemEvent event);
 private:
  enum class Pending { None, Network, Dial };
  Pending pending_ = Pending::None;
  uint32_t sent_ = 0;
  bool session_ = false, accepted_ = false;
  ModemEvent event_ = ModemEvent::None;
};
}
