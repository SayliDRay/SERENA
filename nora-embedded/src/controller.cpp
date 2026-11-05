#include "controller.h"
namespace nora {
const char* stateName(State state) {
  switch (state) {
    case State::READY: return "READY";
    case State::HOLDING_SOS: return "HOLDING_SOS";
    case State::CANCEL_WINDOW: return "CANCEL_WINDOW";
    case State::CHECKING_NETWORK: return "CHECKING_NETWORK";
    case State::DIALING: return "DIALING";
    case State::CONNECTED: return "CONNECTED";
    case State::CALL_ENDED: return "CALL_ENDED";
    case State::FAILED: return "FAILED";
  }
  return "UNKNOWN";
}
void Controller::enter(State state, uint32_t now, const char* reason) {
  state_ = state; since_ = now; reason_ = reason; freshPress_ = false; stopHeld_ = false;
}
void Controller::finish(State state, uint32_t now, const char* reason) {
  modem_.hangup(); armed_ = false; enter(state, now, reason);
}
void Controller::tick(uint32_t now, bool down, bool pressed, bool fall, Source source) {
  auto event = modem_.poll(now);
  // A release must be observed within each countdown/call before a new press acts.
  if (!down) { freshPress_ = true; stopHeld_ = false; }
  if (config_.shortPressCall && (state_ == State::DIALING || state_ == State::CONNECTED)) {
    if (pressed && freshPress_) { stopHeld_ = true; stopSince_ = now; }
    if (stopHeld_ && down && elapsed(now, stopSince_, config_.hangupHold)) {
      finish(State::CALL_ENDED, now, "local long-press hangup"); return;
    }
  }
  switch (state_) {
    case State::READY:
      if (!down) armed_ = true;
      if (fall) { source_ = source; armed_ = false;
        enter(State::CANCEL_WINDOW, now, source == Source::Injected ?
          "INJECTED suspected fall: 20s cancel" : "SENSOR suspected fall: experimental");
      } else if (armed_ && pressed) {
        source_ = Source::Manual; armed_ = false;
        enter(State::HOLDING_SOS, now, "hold SOS continuously for 3s");
      }
      break;
    case State::HOLDING_SOS:
      if (config_.shortPressCall) {
        if (!down) enter(State::CANCEL_WINDOW, now, "short SOS: call now");
        else if (elapsed(now, since_, config_.hangupHold)) finish(State::CALL_ENDED, now, "long gesture: no call");
      } else if (!down) enter(State::READY, now, "short press ignored");
      else if (elapsed(now, since_, config_.hold))
        enter(State::CANCEL_WINDOW, now, "SOS confirmed: 5s cancel");
      break;
    case State::CANCEL_WINDOW:
      if (pressed && freshPress_) finish(State::CALL_ENDED, now, "cancelled locally");
      else if (elapsed(now, since_, source_ == Source::Manual ? config_.manualCancel : config_.fallCancel)) {
        enter(State::CHECKING_NETWORK, now, "network check requested");
        modem_.checkNetwork(now);
      }
      break;
    case State::CHECKING_NETWORK:
      if (event == ModemEvent::Error) finish(State::FAILED, now, "modem error: check transport/configuration");
      else if (event == ModemEvent::NetworkReady) {
        enter(State::DIALING, now, "call REQUESTED: family slot");
        dialStage_ = DialStage::Requesting; modem_.dialFamily(now);
      } else if (event == ModemEvent::NoNetwork || event == ModemEvent::NetworkLost)
        finish(State::FAILED, now, "network unavailable");
      else if (elapsed(now, since_, config_.networkTimeout))
        finish(State::FAILED, now, "modem network response timeout");
      break;
    case State::DIALING:
      if (event == ModemEvent::Error) finish(State::FAILED, now, "modem communication error");
      else if (event == ModemEvent::NoNetwork || event == ModemEvent::NetworkLost)
        finish(State::FAILED, now, "network lost while dialing");
      else if (event == ModemEvent::Busy) finish(State::FAILED, now, "contact busy");
      else if (event == ModemEvent::NoAnswer) finish(State::FAILED, now, "no answer");
      else if (event == ModemEvent::RemoteHangup) finish(State::CALL_ENDED, now, "remote ended before answer");
      else if (event == ModemEvent::Accepted && dialStage_ == DialStage::Requesting) {
        dialStage_ = DialStage::Accepted; since_ = now; reason_ = "request ACCEPTED, not answered";
      } else if (event == ModemEvent::Ringing && dialStage_ != DialStage::Requesting) {
        dialStage_ = DialStage::Ringing; reason_ = "REMOTE RINGING, not answered";
      } else if (event == ModemEvent::Answered && dialStage_ != DialStage::Requesting)
        enter(State::CONNECTED, now, "remote ANSWERED: voice session");
      else if (elapsed(now, since_, dialStage_ == DialStage::Requesting ? config_.acceptTimeout : config_.answerTimeout))
        finish(State::FAILED, now, dialStage_ == DialStage::Requesting ? "modem dial response timeout" : "no answer timeout");
      break;
    case State::CONNECTED:
      if (event == ModemEvent::Error) finish(State::FAILED, now, "modem communication error");
      else if (event == ModemEvent::NetworkLost || event == ModemEvent::NoNetwork)
        finish(State::FAILED, now, "network lost during call");
      else if (event == ModemEvent::RemoteHangup)
        finish(State::CALL_ENDED, now, "remote hangup");
      else if (!config_.shortPressCall && pressed && freshPress_) finish(State::CALL_ENDED, now, "local hangup");
      break;
    case State::CALL_ENDED: case State::FAILED:
      if (elapsed(now, since_, config_.resultDisplay)) {
        enter(State::READY, now, "release SOS to rearm"); armed_ = false;
      }
      break;
  }
}
}
