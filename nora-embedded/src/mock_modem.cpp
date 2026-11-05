#include "mock_modem.h"
namespace nora {
void MockModem::checkNetwork(uint32_t now) {
  hangup(); pending_ = Pending::Network; sent_ = now;
}
void MockModem::dialFamily(uint32_t now) {
  pending_ = Pending::Dial; sent_ = now; session_ = true; accepted_ = false;
}
void MockModem::hangup() {
  pending_ = Pending::None; event_ = ModemEvent::None;
  session_ = accepted_ = false;
}
void MockModem::setNetwork(bool available) {
  network = available;
  if (!available && session_) event_ = ModemEvent::NetworkLost;
}
bool MockModem::inject(ModemEvent event) {
  if (!session_ || !accepted_ || event_ != ModemEvent::None) return false;
  switch (event) {
    case ModemEvent::Ringing: case ModemEvent::Answered: case ModemEvent::Busy:
    case ModemEvent::NoAnswer: case ModemEvent::RemoteHangup: case ModemEvent::Error:
      event_ = event; return true;
    default: return false;
  }
}
ModemEvent MockModem::poll(uint32_t now) {
  if (event_ != ModemEvent::None) {
    auto event = event_; event_ = ModemEvent::None; return event;
  }
  if (pending_ == Pending::None || dropResponses || !elapsed(now, sent_, responseDelay))
    return ModemEvent::None;
  auto pending = pending_; pending_ = Pending::None;
  if (!network) return ModemEvent::NoNetwork;
  if (pending == Pending::Network) return ModemEvent::NetworkReady;
  accepted_ = true; return ModemEvent::Accepted;
}
}
