#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Relay đóng cắt tải AC (đèn, quạt).
//
// AN TOÀN: phía tiếp điểm là điện lưới 220V. Khi thử nghiệm nên đấu tải giả
// (bóng đèn nhỏ) và giữ phần cao áp tách biệt hoàn toàn với phần ESP32.
// Module relay rời thường kích mức thấp -> activeLow = true.
// ---------------------------------------------------------------------------

class Relay {
 public:
  void begin(uint8_t pin, bool activeLow = true, bool initialOn = false);
  void set(bool on);
  void toggle();
  bool isOn() const { return _on; }
  uint8_t pin() const { return _pin; }

 private:
  uint8_t _pin = 255;
  bool _activeLow = true;
  bool _on = false;
};
