#pragma once

#include "config.h"

// Driver này kéo theo 2 thư viện ngoài, chỉ biên dịch khi node thật sự có
// SGP41. Node relay và node IR không khai báo lib_deps nên phải bỏ qua.
#if ENABLE_SGP41

#include <Arduino.h>
#include <NOxGasIndexAlgorithm.h>
#include <SensirionI2CSgp41.h>
#include <VOCGasIndexAlgorithm.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// SGP41 - cảm biến đa khí. Lớp này bọc quanh 2 thư viện chính hãng Sensirion:
//
//   Sensirion I2C SGP41          - giao tiếp, trả về tín hiệu thô (ticks)
//   Sensirion Gas Index Algorithm - biến ticks thành VOC/NOx Index 0..500
//
// Bản thân chip chỉ cho ra ticks, con số đó không dùng trực tiếp được. Index
// mới là thứ đọc hiểu được: 100 là mức nền của phòng, cao hơn là không khí
// đang xấu đi so với nền, thấp hơn là đang tốt lên. Thuật toán tự học mức nền
// nên khoảng 1 giờ đầu sau khi cấp nguồn giá trị chưa đáng tin.
//
// Thư viện chính hãng cố định địa chỉ I2C là 0x59, không truyền địa chỉ được.
// ---------------------------------------------------------------------------

struct Sgp41Reading {
  uint16_t srawVoc;  // ticks thô
  uint16_t srawNox;  // ticks thô
  int32_t vocIndex;  // 1..500, 100 = mức nền
  int32_t noxIndex;  // 1..500, 1 = mức nền
};

class Sgp41 {
 public:
  // samplingIntervalS phải khớp với chu kỳ gọi measure(). Thuật toán VOC được
  // Sensirion kiểm chứng ở 1 s (mặc định) và 10 s. Phần NOx luôn giả định 1 s.
  explicit Sgp41(float samplingIntervalS = 1.0f);

  bool begin(TwoWire &bus);

  // Đạt khi result == 0xD400 (theo example chính hãng).
  bool selfTest(uint16_t &result);

  // Chạy 1 lần/giây trong 10 giây đầu, trước lần đo thật đầu tiên.
  bool executeConditioning(uint16_t &srawVoc);

  // tempC / rh dùng để bù nhiệt-ẩm; truyền số đo từ SHT31 vào cho chính xác.
  bool measure(float tempC, float rh, Sgp41Reading &out);

  bool turnHeaterOff();
  bool getSerialNumber(uint16_t serial[3]);

  // Mô tả lỗi của lệnh gần nhất, do thư viện Sensirion sinh ra.
  const char *lastError() const { return _lastError; }

 private:
  bool check(uint16_t error);

  SensirionI2CSgp41 _dev;
  VOCGasIndexAlgorithm _voc;
  NOxGasIndexAlgorithm _nox;
  char _lastError[96];
};

#endif  // ENABLE_SGP41
