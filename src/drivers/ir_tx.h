#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Phát hồng ngoại qua module công suất cao VOKTTA 1W.
//
// Sóng mang 38 kHz sinh bằng LEDC (phần cứng), phần mềm chỉ bật/tắt sóng mang
// theo chuỗi mark/space -> giữ được duty ổn định mà không tốn CPU.
//
// Đây là lớp phát THÔ. Điều hòa không dùng mã kiểu điều khiển TV (1 lệnh =
// 1 mã), mà gửi nguyên khung trạng thái (mode + nhiệt độ + tốc độ quạt...)
// dài 60-280 bit tùy hãng. Giai đoạn sau nên thêm thư viện IRremoteESP8266
// (lib_deps = crankyoldgit/IRremoteESP8266) để có sẵn giao thức của từng hãng;
// lớp này vẫn hữu ích để test phần cứng và phát lại chuỗi raw đã bắt được.
// ---------------------------------------------------------------------------

class IrTx {
 public:
  void begin(uint8_t pin, uint32_t carrierHz = 38000, uint8_t dutyPercent = 33);

  // Chuỗi thời lượng micro-giây, xen kẽ mark (có sóng mang) / space (tắt),
  // bắt đầu bằng mark. Đây chính là định dạng "raw" của các bộ bắt mã IR.
  void sendRaw(const uint16_t *durationsUs, size_t len);

  // Khung NEC chuẩn - chủ yếu để kiểm tra phần cứng phát có hoạt động không.
  void sendNec(uint8_t address, uint8_t command);

 private:
  void carrierOn();
  void carrierOff();

  uint8_t _pin = 255;
  uint32_t _duty = 0;
};
