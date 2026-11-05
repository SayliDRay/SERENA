#include "sim7600_modem.h"
#include <cstdio>
#include <cstring>
namespace nora {
namespace {
// LTE-only deliberately avoids treating a 2G/3G fallback as validated VoLTE.
const char* const setup[] = {"AT", "ATE0", "AT+IFC=0,0", "AT+CPIN?",
  "AT+CNMP=38", "AT+CEREG=1", "AT+CSDVC=1"};
constexpr int setupCount = sizeof(setup)/sizeof(setup[0]);
}
bool Sim7600Modem::configured() const {
  if (!qualified_ || !number_ || number_[0] != '+') return false;
  size_t n = std::strlen(number_);
  if (n < 9 || n > 16 || number_[1] == '0') return false;
  for (size_t i=1; i<n; ++i) if (number_[i] < '0' || number_[i] > '9') return false;
  return true;
}
void Sim7600Modem::emit(ModemEvent event) {
  // Terminal failures must take priority over progress within a received burst.
  if (event_ == ModemEvent::None || event == ModemEvent::Error ||
      event == ModemEvent::NetworkLost || event == ModemEvent::RemoteHangup ||
      event == ModemEvent::Busy || event == ModemEvent::NoAnswer) event_ = event;
}
void Sim7600Modem::send(Command command, const char* text, uint32_t now) {
  command_ = command; sent_ = now;
  if (command == Command::Registration) registered_ = false;
  if (command == Command::Calls) callSeen_ = false;
  stream_.write(text); stream_.write("\r");
}
void Sim7600Modem::fail() {
  fault_ = true; ready_ = false; emit(ModemEvent::Error);
  // Try to terminate an uncertain session. Do not re-use this UART after an
  // uncorrelated response/timeout: a power-cycle and fresh qualification needed.
  if (active_) stream_.write("AT+CHUP\r");
  active_ = accepted_ = false; command_ = Command::None;
}
void Sim7600Modem::checkNetwork(uint32_t now) {
  if (!configured() || fault_ || stopping_ || command_ != Command::None || active_) {
    emit(ModemEvent::Error); return;
  }
  ready_ = registered_ = simReady_ = false; setupStep_ = 0;
  progress_ = ModemEvent::None; length_ = 0; overflow_ = false;
  send(Command::Setup, setup[setupStep_], now);
}
void Sim7600Modem::dialFamily(uint32_t now) {
  if (!configured() || !ready_ || fault_ || stopping_ || command_ != Command::None) {
    emit(ModemEvent::Error); return;
  }
  char request[32]; std::snprintf(request, sizeof(request), "ATD%s;", number_);
  active_ = true; accepted_ = wasConnected_ = false; callId_ = -1;
  progress_ = ModemEvent::None; send(Command::Dial, request, now);
}
void Sim7600Modem::hangup() {
  event_ = progress_ = ModemEvent::None; ready_ = false;
  if (fault_) return;
  // Drain the current transaction first; do not mistake its OK for CHUP's OK.
  if (active_ || command_ != Command::None) stopping_ = true;
  if (stopping_ && command_ == Command::None) send(Command::Stop, "AT+CHUP", lastNow_);
}
void Sim7600Modem::line(uint32_t now) {
  buffer_[length_] = 0;
  if (overflow_) { fail(); return; }
  if (!length_ || fault_) return;
  if (!std::strcmp(buffer_, "OK")) {
    auto completed = command_; command_ = Command::None;
    if (stopping_) {
      if (completed == Command::Stop) { stopping_ = active_ = accepted_ = false; }
      return;
    }
    if (completed == Command::Setup) {
      if (setupStep_ == 3 && !simReady_) { emit(ModemEvent::Error); return; }
      if (++setupStep_ < setupCount) send(Command::Setup, setup[setupStep_], now);
      else send(Command::Registration, "AT+CEREG?", now);
    } else if (completed == Command::Registration) {
      ready_ = registered_; emit(registered_ ? ModemEvent::NetworkReady : ModemEvent::NoNetwork);
    } else if (completed == Command::Dial) {
      accepted_ = true; lastPoll_ = now; emit(ModemEvent::Accepted);
    } else if (completed == Command::Calls && active_) {
      lastPoll_ = now;
      if (!callSeen_) emit(ModemEvent::RemoteHangup);
    }
    return;
  }
  if (!std::strcmp(buffer_, "ERROR") || !std::strncmp(buffer_, "+CME ERROR:",11)) {
    fail(); return;
  }
  if (stopping_) return; // No progress from the previous call can escape a stop.
  if (!std::strcmp(buffer_, "+CPIN: READY") && command_ == Command::Setup) simReady_ = true;
  int n=-1, stat=-1;
  if (std::sscanf(buffer_, "+CEREG: %d,%d", &n,&stat) == 2) {
    if (command_ == Command::Registration) registered_ = stat == 1 || stat == 5;
  } else if (std::sscanf(buffer_, "+CEREG: %d", &stat) == 1) {
    if (active_ && stat != 1 && stat != 5) emit(ModemEvent::NetworkLost);
  }
  if (!active_) return;
  if (!std::strcmp(buffer_, "BUSY")) {
    emit(ModemEvent::Busy); if (command_ == Command::Dial) command_ = Command::None;
  } else if (!std::strcmp(buffer_, "NO ANSWER")) {
    emit(ModemEvent::NoAnswer); if (command_ == Command::Dial) command_ = Command::None;
  }
  else if (!std::strcmp(buffer_, "NO CARRIER") || !std::strncmp(buffer_, "VOICE CALL: END",15) ||
           !std::strncmp(buffer_, "VOICE CALL:END",14)) {
    emit(ModemEvent::RemoteHangup);
    if (command_ == Command::Dial && !std::strcmp(buffer_, "NO CARRIER")) command_ = Command::None;
  }
  int id=-1, dir=-1, mode=-1, mpty=-1;
  if (std::sscanf(buffer_, "+CLCC: %d,%d,%d,%d,%d", &id,&dir,&stat,&mode,&mpty) == 5 &&
      dir == 0 && mode == 0 && id > 0 && (callId_ < 0 || callId_ == id)) {
    callId_ = id; callSeen_ = true;
    if (stat == 0) { wasConnected_ = true; progress_ = ModemEvent::Answered; }
    else if (stat == 3 && !wasConnected_) progress_ = ModemEvent::Ringing;
    else if (stat == 6) emit(ModemEvent::RemoteHangup);
  }
  // RING is an incoming call and is intentionally not treated as outgoing ringing.
  // VOICE CALL: BEGIN and generic CONNECT are not used to infer an answered call.
}
ModemEvent Sim7600Modem::poll(uint32_t now) {
  lastNow_ = now;
  // Return progress from the preceding batch before accepting more bytes.
  if (event_ != ModemEvent::None) { auto e=event_; event_=ModemEvent::None; return e; }
  for (int budget=0; budget<256; ++budget) {
    int byte = stream_.read(); if (byte < 0) break;
    char c = static_cast<char>(byte);
    if (c == '\r') continue;
    if (c == '\n') {
      line(now); length_=0; overflow_=false;
      // Stop at a response boundary: a new command is sent only after the
      // current final result, and Accepted must precede subsequent progress.
      if (event_ != ModemEvent::None) break;
    } else if (length_ < sizeof(buffer_)-1) buffer_[length_++] = c;
    else overflow_ = true;
  }
  if (!fault_ && command_ != Command::None && elapsed(now, sent_, 10000)) fail();
  if (!fault_ && stopping_ && command_ == Command::None) send(Command::Stop,"AT+CHUP",now);
  if (!fault_ && !stopping_ && accepted_ && active_ && command_ == Command::None && elapsed(now,lastPoll_,1000))
    send(Command::Calls,"AT+CLCC",now);
  if (event_ != ModemEvent::None) { auto e=event_; event_=ModemEvent::None; return e; }
  if (accepted_ && !stopping_ && progress_ != ModemEvent::None) {
    auto e=progress_; progress_=ModemEvent::None; return e;
  }
  return ModemEvent::None;
}
}
