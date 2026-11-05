#include "motion_sensor.h"
#include <Wire.h>
#include <cmath>
namespace nora {
bool MotionSensor::begin() {
  Wire.begin(pins::sda, pins::scl, 100000);
  Wire.setTimeOut(20);
  healthy_ = device_.begin(0x68, &Wire);
  if (healthy_) {
    device_.setAccelerometerRange(MPU6050_RANGE_8_G);
    device_.setGyroRange(MPU6050_RANGE_500_DEG);
    device_.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }
  return healthy_;
}
bool MotionSensor::update(uint32_t now, bool eligible) {
  if (!eligible) detector_.reset();
  if (!healthy_ || !elapsed(now, sampled_, 20)) return false;
  sampled_ = now;
  sensors_event_t a, g, t;
  if (!device_.getEvent(&a, &g, &t)) {
    healthy_ = false; detector_.reset(); return false;
  }
  const float acceleration = std::sqrt(a.acceleration.x*a.acceleration.x +
    a.acceleration.y*a.acceleration.y + a.acceleration.z*a.acceleration.z) / 9.80665f;
  const float rotation = std::sqrt(g.gyro.x*g.gyro.x + g.gyro.y*g.gyro.y + g.gyro.z*g.gyro.z);
  return detector_.sample(now, acceleration, rotation, eligible);
}
}
