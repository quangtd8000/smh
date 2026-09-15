#include "drivers/relay.h"

void Relay::begin(uint8_t pin, bool activeLow, bool initialOn) {
  _pin = pin;
  _activeLow = activeLow;
  // Đặt mức "tắt" trước rồi mới chuyển sang OUTPUT để relay không bị nháy
  // một nhịp lúc boot.
  digitalWrite(_pin, _activeLow ? HIGH : LOW);
  pinMode(_pin, OUTPUT);
  set(initialOn);
}

void Relay::set(bool on) {
  _on = on;
  digitalWrite(_pin, (_on != _activeLow) ? HIGH : LOW);
}

void Relay::toggle() { set(!_on); }


