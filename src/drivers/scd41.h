#pragma once

#include <Arduino.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// SCD41 - CO2 (NDIR quang học) + nhiệt độ + độ ẩm. I2C 0x62, tối đa 100 kHz.
// Chế độ periodic: cảm biến tự đo, ra số liệu mới mỗi 5 giây.
// LƯU Ý: chưa có hàng, driver này chỉ được biên dịch khi ENABLE_SCD41=1.
// ---------------------------------------------------------------------------

class Scd41 {
 public:
  // Dừng đo (phòng khi chip còn đang chạy từ lần cấp nguồn trước) + reinit.
  bool begin(TwoWire &bus, uint8_t addr = 0x62);

  bool startPeriodicMeasurement();
  bool stopPeriodicMeasurement();  // mất 500 ms
  bool reinit();                   // mất 30 ms

  bool isDataReady(bool &ready);
  bool read(uint16_t &co2ppm, float &tempC, float &rh);

  bool getSerialNumber(uint64_t &serial);

 private:
  TwoWire *_bus = nullptr;
  uint8_t _addr = 0x62;
};
