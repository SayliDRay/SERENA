#pragma once
#include <stdint.h>
namespace nora {
// All durations must remain below 2^31 ms. Units: milliseconds unless noted.
inline bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
  return uint32_t(now - since) >= duration;
}
struct Config {
  uint32_t debounce = 35, hold = 3000, manualCancel = 5000, fallCancel = 20000;
  uint32_t networkTimeout = 8000, acceptTimeout = 8000, answerTimeout = 30000;
  uint32_t resultDisplay = 2000;
  bool shortPressCall = false;
  uint32_t hangupHold = 2000;
};
struct FallConfig {
  bool enabled = true;
  float impactG = 2.5f, stillGravityToleranceG = 0.15f, stillGyroRad = 0.20f;
  uint32_t settleMs = 500, inactivityMs = 3000, observationMs = 8000;
  uint32_t maxSampleGapMs = 200;
};
namespace pins {
// XIAO ESP32C3 board labels differ from GPIO numbers. Keep GPIO2/8/9 free
// (boot straps); GPIO18/19 are reserved for the USB serial console.
constexpr int sos = 3, status = 10, alert = 5;
#ifdef NORA_REAL_MODEM
// Physical bench: UART replaces the simulated yellow LED/piezo connections.
// Red LED and external motor MOSFET share GPIO5; no physical fall test button.
constexpr int motorIndicator = alert, buzzer = 4, modemRx = 20, modemTx = 21;
#else
constexpr int fallTest = 4, motorIndicator = 20, buzzer = 21;
#endif
constexpr int sda = 6, scl = 7;
}
}
