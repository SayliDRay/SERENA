#pragma once
#include "config.h"
#include "modem.h"
namespace nora {
enum class State { READY, HOLDING_SOS, CANCEL_WINDOW, CHECKING_NETWORK,
  DIALING, CONNECTED, CALL_ENDED, FAILED };
enum class Source { Manual, Sensor, Injected };
enum class DialStage { Requesting, Accepted, Ringing };
const char* stateName(State state);
class Controller {
 public:
  explicit Controller(ModemTransport& modem, Config config = Config())
    : modem_(modem), config_(config) {}
  // now is an injected monotonic uint32 millisecond clock (millis() on device).
  // down/pressed must come from the debouncer. fall is a one-tick event.
  void tick(uint32_t now, bool down, bool pressed, bool fall = false,
            Source source = Source::Sensor);
  State state() const { return state_; }
  Source source() const { return source_; }
  DialStage dialStage() const { return dialStage_; }
  const char* reason() const { return reason_; }
  bool armed() const { return armed_; }
  uint32_t stateSince() const { return since_; }
 private:
  void enter(State state, uint32_t now, const char* reason);
  void finish(State state, uint32_t now, const char* reason);
  ModemTransport& modem_;
  Config config_;
  State state_ = State::READY;
  Source source_ = Source::Manual;
  DialStage dialStage_ = DialStage::Requesting;
  uint32_t since_ = 0;
  bool armed_ = false, freshPress_ = false, stopHeld_ = false;
  uint32_t stopSince_ = 0;
  const char* reason_ = "boot: release SOS to arm";
};
}
