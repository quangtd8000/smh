#include "drivers/ir_tx.h"

namespace {
constexpr uint8_t LEDC_RESOLUTION_BITS = 8;
constexpr uint8_t LEDC_CHANNEL = 0;  // chỉ dùng cho Arduino core 2.x

// delayMicroseconds chỉ chính xác với khoảng ngắn; space giữa các khung IR có
// thể dài hàng chục ms nên chia nhỏ ra.
void waitUs(uint32_t us) {
  while (us > 10000) {
    delayMicroseconds(10000);
    us -= 10000;
  }
  if (us) delayMicroseconds(us);
}

// Thông số khung NEC (micro-giây).
constexpr uint16_t NEC_HDR_MARK = 9000;
constexpr uint16_t NEC_HDR_SPACE = 4500;
constexpr uint16_t NEC_BIT_MARK = 560;
constexpr uint16_t NEC_ONE_SPACE = 1690;
constexpr uint16_t NEC_ZERO_SPACE = 560;
}  // namespace

void IrTx::begin(uint8_t pin, uint32_t carrierHz, uint8_t dutyPercent) {
  _pin = pin;
  const uint32_t maxDuty = (1UL << LEDC_RESOLUTION_BITS) - 1;
  _duty = (maxDuty * dutyPercent) / 100;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(_pin, carrierHz, LEDC_RESOLUTION_BITS);
#else
  ledcSetup(LEDC_CHANNEL, carrierHz, LEDC_RESOLUTION_BITS);
  ledcAttachPin(_pin, LEDC_CHANNEL);
#endif
  carrierOff();
}

void IrTx::carrierOn() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(_pin, _duty);
#else
  ledcWrite(LEDC_CHANNEL, _duty);
#endif
}

void IrTx::carrierOff() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(_pin, 0);
#else
  ledcWrite(LEDC_CHANNEL, 0);
#endif
}

void IrTx::sendRaw(const uint16_t *durationsUs, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (i % 2 == 0) {
      carrierOn();
    } else {
      carrierOff();
    }
    waitUs(durationsUs[i]);
  }
  carrierOff();
}

void IrTx::sendNec(uint8_t address, uint8_t command) {
  const uint8_t payload[4] = {address, (uint8_t)~address, command,
                              (uint8_t)~command};

  carrierOn();
  waitUs(NEC_HDR_MARK);
  carrierOff();
  waitUs(NEC_HDR_SPACE);

  for (uint8_t byteIdx = 0; byteIdx < 4; byteIdx++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      carrierOn();
      waitUs(NEC_BIT_MARK);
      carrierOff();
      // NEC gửi bit thấp trước; độ dài space mới là thứ mã hóa 0 hay 1.
      waitUs((payload[byteIdx] >> bit) & 0x01 ? NEC_ONE_SPACE : NEC_ZERO_SPACE);
    }
  }

  // Mark kết thúc khung.
  carrierOn();
  waitUs(NEC_BIT_MARK);
  carrierOff();
}
