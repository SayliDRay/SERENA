#pragma once
#include "config.h"
namespace nora {
class Button {
 public:
  explicit Button(uint32_t debounce = 35) : debounce_(debounce) {}
  void update(uint32_t now, bool rawPressed) {
    pressedEdge_ = releasedEdge_ = false;
    if (!initialized_) {
      initialized_ = true; raw_ = stable_ = rawPressed; changed_ = now; return;
    }
    if (rawPressed != raw_) { raw_ = rawPressed; changed_ = now; }
    if (raw_ != stable_ && elapsed(now, changed_, debounce_)) {
      stable_ = raw_; pressedEdge_ = stable_; releasedEdge_ = !stable_;
    }
  }
  bool down() const { return stable_; }
  bool pressed() const { return pressedEdge_; }
  bool released() const { return releasedEdge_; }
 private:
  uint32_t debounce_, changed_ = 0;
  bool initialized_ = false, raw_ = false, stable_ = false, pressedEdge_ = false, releasedEdge_ = false;
};
}
