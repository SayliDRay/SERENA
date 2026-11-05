#pragma once
#include "config.h"
namespace nora {
// Acceleration magnitude in g including gravity; angular velocity magnitude rad/s.
class FallDetector {
 public:
  explicit FallDetector(FallConfig config = FallConfig()) : config_(config) {}
  bool sample(uint32_t now, float accelerationG, float gyroRad, bool eligible);
  void reset() { impact_ = still_ = haveSample_ = false; }
 private:
  FallConfig config_;
  bool impact_ = false, still_ = false, haveSample_ = false;
  uint32_t impactAt_ = 0, stillAt_ = 0, lastSample_ = 0;
};
}
