#pragma once
#include <stdint.h>
namespace nora {
// Application event contract. Each transport documents its own wire protocol.
enum class ModemEvent { None, NetworkReady, NoNetwork, Accepted, Ringing,
  Answered, Busy, NoAnswer, RemoteHangup, NetworkLost, Error };
class ModemTransport {
 public:
  virtual ~ModemTransport() {}
  virtual void checkNetwork(uint32_t now) = 0;
  virtual void dialFamily(uint32_t now) = 0;
  virtual void hangup() = 0; // Abort application events; hardware stop is asynchronous.
  virtual ModemEvent poll(uint32_t now) = 0;
};
}
