#pragma once

#include <Arduino.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// SPS30 - cảm biến bụi mịn PM1.0 / PM2.5 / PM4 / PM10. I2C 0x69, tối đa 100 kHz.
//
// PHẦN CỨNG - đọc kỹ trước khi cắm:
//   - VDD phải là 5V (không phải 3V3), dòng đỉnh ~80 mA lúc quạt khởi động.
//   - Chân SEL (pin 4) phải nối GND TRƯỚC KHI cấp nguồn để chọn chế độ I2C.
//     Thả nổi = chế độ UART, driver này sẽ không thấy chip.
//   - Mức logic I2C tham chiếu theo VDD 5V; kiểm tra lại datasheet bản mình
//     đang cầm, cần thì thêm level shifter thay vì nối thẳng vào ESP32 3V3.
//   - Ra số liệu mới mỗi 1 giây sau khi startMeasurement().
//
// LƯU Ý: chưa có hàng, driver này chỉ được biên dịch khi ENABLE_SPS30=1.
// ---------------------------------------------------------------------------

struct Sps30Data {
  // Nồng độ khối lượng, µg/m3
  float pm1_0;
  float pm2_5;
  float pm4_0;
  float pm10;
  // Nồng độ số hạt, #/cm3
  float nc0_5;
  float nc1_0;
  float nc2_5;
  float nc4_0;
  float nc10;
  // Kích thước hạt điển hình, µm
  float typicalParticleSize;
};

class Sps30 {
 public:
  bool begin(TwoWire &bus, uint8_t addr = 0x69);

  bool startMeasurement();  // định dạng float IEEE-754
  bool stopMeasurement();
  bool isDataReady(bool &ready);
  bool read(Sps30Data &out);

  // Quay quạt hết tốc độ 10 s để thổi bụi bám. Nên chạy 1 tuần/lần.
  bool startFanCleaning();
  bool reset();

  bool getSerialNumber(char *out, size_t outLen);  // outLen >= 33

 private:
  TwoWire *_bus = nullptr;
  uint8_t _addr = 0x69;
};
