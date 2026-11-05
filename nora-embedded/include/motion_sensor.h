#pragma once
#include <Adafruit_MPU6050.h>
#include "fall_detector.h"
namespace nora {
class MotionSensor {
 public:
  bool begin();
  bool update(uint32_t now, bool eligible);
  bool healthy() const { return healthy_; }
 private:
  Adafruit_MPU6050 device_;
  FallDetector detector_;
  bool healthy_ = false;
  uint32_t sampled_ = 0;
};
}
