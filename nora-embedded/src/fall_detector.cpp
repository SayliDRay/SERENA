#include "fall_detector.h"
#include <cmath>
namespace nora {
bool FallDetector::sample(uint32_t now, float accelerationG, float gyroRad, bool eligible) {
  if (!config_.enabled || !eligible || !std::isfinite(accelerationG) || !std::isfinite(gyroRad)) {
    reset(); return false;
  }
  if (haveSample_ && elapsed(now, lastSample_, config_.maxSampleGapMs)) reset();
  haveSample_ = true; lastSample_ = now;
  if (impact_ && elapsed(now, impactAt_, config_.observationMs)) { impact_ = still_ = false; }
  if (!impact_ && accelerationG >= config_.impactG) {
    impact_ = true; impactAt_ = now; still_ = false; return false;
  }
  if (!impact_ || !elapsed(now, impactAt_, config_.settleMs)) return false;
  if (std::fabs(accelerationG - 1.0f) <= config_.stillGravityToleranceG && gyroRad <= config_.stillGyroRad) {
    if (!still_) { still_ = true; stillAt_ = now; }
    if (elapsed(now, stillAt_, config_.inactivityMs)) { reset(); return true; }
  } else still_ = false;
  return false;
}
}
