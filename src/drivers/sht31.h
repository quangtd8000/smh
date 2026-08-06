#pragma once

#include <Arduino.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// SHT31 - nhiệt độ + độ ẩm. I2C 0x44 (0x45 nếu kéo chân ADDR lên cao).
// Chế độ single-shot, high repeatability, không clock stretching.
// ---------------------------------------------------------------------------

class Sht31 {
 public:
  bool begin(TwoWire &bus, uint8_t addr = 0x44);
  bool softReset();
  bool readStatus(uint16_t &status);

  // tempC: độ C, rh: %RH. Mất khoảng 20 ms (blocking).
  bool read(float &tempC, float &rh);

 private:
  TwoWire *_bus = nullptr;
  uint8_t _addr = 0x44;
};
