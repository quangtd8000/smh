#include "drivers/sensirion_i2c.h"

#include <string.h>

namespace sensirion {

uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

bool sendCommand(TwoWire &bus, uint8_t addr, uint16_t cmd) {
  bus.beginTransmission(addr);
  bus.write((uint8_t)(cmd >> 8));
  bus.write((uint8_t)(cmd & 0xFF));
  return bus.endTransmission() == 0;
}

bool sendCommandWithArgs(TwoWire &bus, uint8_t addr, uint16_t cmd,
                         const uint16_t *args, size_t nArgs) {
  bus.beginTransmission(addr);
  bus.write((uint8_t)(cmd >> 8));
  bus.write((uint8_t)(cmd & 0xFF));
  for (size_t i = 0; i < nArgs; i++) {
    uint8_t word[2] = {(uint8_t)(args[i] >> 8), (uint8_t)(args[i] & 0xFF)};
    bus.write(word, 2);
    bus.write(crc8(word, 2));
  }
  return bus.endTransmission() == 0;
}

bool readWords(TwoWire &bus, uint8_t addr, uint16_t *words, size_t nWords) {
  const size_t nBytes = nWords * 3;
  if (nBytes == 0) return true;
  // Buffer Wire mặc định của ESP32 là 128 byte -> tối đa 42 word mỗi lần đọc.
  if (nBytes > 126) return false;

  if (bus.requestFrom((int)addr, (int)nBytes) != (int)nBytes) return false;

  for (size_t i = 0; i < nWords; i++) {
    uint8_t word[2];
    word[0] = (uint8_t)bus.read();
    word[1] = (uint8_t)bus.read();
    uint8_t crc = (uint8_t)bus.read();
    if (crc8(word, 2) != crc) return false;
    words[i] = (uint16_t)((uint16_t)word[0] << 8 | word[1]);
  }
  return true;
}

bool readWordsRetry(TwoWire &bus, uint8_t addr, uint16_t *words, size_t nWords,
                    uint8_t attempts, uint32_t retryDelayMs,
                    uint8_t *usedAttempts) {
  if (attempts == 0) attempts = 1;
  for (uint8_t i = 1; i <= attempts; i++) {
    if (readWords(bus, addr, words, nWords)) {
      if (usedAttempts) *usedAttempts = i;
      return true;
    }
    if (i < attempts) delay(retryDelayMs);
  }
  if (usedAttempts) *usedAttempts = attempts;
  return false;
}

bool generalCallReset(TwoWire &bus) {
  bus.beginTransmission(0x00);
  bus.write(0x06);
  const bool ok = bus.endTransmission() == 0;
  delay(20);  // datasheet: chip cần ~10 ms để khởi động lại
  return ok;
}

float wordsToFloat(uint16_t hi, uint16_t lo) {
  uint32_t raw = ((uint32_t)hi << 16) | lo;
  float value;
  memcpy(&value, &raw, sizeof(value));
  return value;
}

}  // namespace sensirion
