#include "drivers/sps30.h"

#include <string.h>

#include "drivers/sensirion_i2c.h"

namespace {
constexpr uint16_t CMD_START_MEASUREMENT = 0x0010;
constexpr uint16_t CMD_STOP_MEASUREMENT = 0x0104;
constexpr uint16_t CMD_READ_DATA_READY = 0x0202;
constexpr uint16_t CMD_READ_MEASURED_VALUES = 0x0300;
constexpr uint16_t CMD_START_FAN_CLEANING = 0x5607;
constexpr uint16_t CMD_READ_SERIAL = 0xD033;
constexpr uint16_t CMD_RESET = 0xD304;

// Tham số của start_measurement: byte cao 0x03 = xuất float IEEE-754,
// byte thấp là dummy 0x00.
constexpr uint16_t ARG_OUTPUT_FLOAT = 0x0300;

constexpr uint32_t CMD_DELAY_MS = 20;
}  // namespace

bool Sps30::begin(TwoWire &bus, uint8_t addr) {
  _bus = &bus;
  _addr = addr;

  // Chip có thể còn đang đo từ lần cấp nguồn trước.
  stopMeasurement();
  delay(CMD_DELAY_MS);

  char serial[33];
  return getSerialNumber(serial, sizeof(serial));
}

bool Sps30::startMeasurement() {
  const uint16_t arg = ARG_OUTPUT_FLOAT;
  if (!sensirion::sendCommandWithArgs(*_bus, _addr, CMD_START_MEASUREMENT, &arg,
                                      1)) {
    return false;
  }
  delay(CMD_DELAY_MS);
  return true;
}

bool Sps30::stopMeasurement() {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_STOP_MEASUREMENT)) return false;
  delay(CMD_DELAY_MS);
  return true;
}

bool Sps30::isDataReady(bool &ready) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_READ_DATA_READY)) return false;
  delay(CMD_DELAY_MS);

  uint16_t status;
  if (!sensirion::readWords(*_bus, _addr, &status, 1)) return false;

  ready = (status & 0x00FF) == 0x0001;
  return true;
}

bool Sps30::read(Sps30Data &out) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_READ_MEASURED_VALUES)) {
    return false;
  }
  delay(CMD_DELAY_MS);

  // 10 giá trị float = 20 word = 60 byte trên bus (đã tính cả CRC).
  uint16_t w[20];
  if (!sensirion::readWords(*_bus, _addr, w, 20)) return false;

  out.pm1_0 = sensirion::wordsToFloat(w[0], w[1]);
  out.pm2_5 = sensirion::wordsToFloat(w[2], w[3]);
  out.pm4_0 = sensirion::wordsToFloat(w[4], w[5]);
  out.pm10 = sensirion::wordsToFloat(w[6], w[7]);
  out.nc0_5 = sensirion::wordsToFloat(w[8], w[9]);
  out.nc1_0 = sensirion::wordsToFloat(w[10], w[11]);
  out.nc2_5 = sensirion::wordsToFloat(w[12], w[13]);
  out.nc4_0 = sensirion::wordsToFloat(w[14], w[15]);
  out.nc10 = sensirion::wordsToFloat(w[16], w[17]);
  out.typicalParticleSize = sensirion::wordsToFloat(w[18], w[19]);
  return true;
}

bool Sps30::startFanCleaning() {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_START_FAN_CLEANING)) return false;
  delay(CMD_DELAY_MS);
  return true;
}

bool Sps30::reset() {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_RESET)) return false;
  delay(100);
  return true;
}

bool Sps30::getSerialNumber(char *out, size_t outLen) {
  if (outLen < 33) return false;
  if (!sensirion::sendCommand(*_bus, _addr, CMD_READ_SERIAL)) return false;
  delay(CMD_DELAY_MS);

  // 32 ký tự ASCII kết thúc bằng NUL = 16 word = 48 byte trên bus.
  uint16_t w[16];
  if (!sensirion::readWords(*_bus, _addr, w, 16)) return false;

  for (size_t i = 0; i < 16; i++) {
    out[i * 2] = (char)(w[i] >> 8);
    out[i * 2 + 1] = (char)(w[i] & 0xFF);
  }
  out[32] = '\0';
  return true;
}
