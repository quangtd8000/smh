#include "drivers/scd41.h"

#include "drivers/sensirion_i2c.h"

namespace {
constexpr uint16_t CMD_START_PERIODIC = 0x21B1;
constexpr uint16_t CMD_READ_MEASUREMENT = 0xEC05;
constexpr uint16_t CMD_STOP_PERIODIC = 0x3F86;
constexpr uint16_t CMD_GET_DATA_READY = 0xE4B8;
constexpr uint16_t CMD_REINIT = 0x3646;
constexpr uint16_t CMD_GET_SERIAL = 0x3682;
}  // namespace

bool Scd41::begin(TwoWire &bus, uint8_t addr) {
  _bus = &bus;
  _addr = addr;

  // Ở chế độ periodic, chip chỉ nhận đúng vài lệnh -> phải dừng trước đã.
  stopPeriodicMeasurement();
  delay(500);

  if (!reinit()) return false;
  delay(30);

  uint64_t serial;
  return getSerialNumber(serial);
}

bool Scd41::startPeriodicMeasurement() {
  return sensirion::sendCommand(*_bus, _addr, CMD_START_PERIODIC);
}

bool Scd41::stopPeriodicMeasurement() {
  return sensirion::sendCommand(*_bus, _addr, CMD_STOP_PERIODIC);
}

bool Scd41::reinit() {
  return sensirion::sendCommand(*_bus, _addr, CMD_REINIT);
}

bool Scd41::isDataReady(bool &ready) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_GET_DATA_READY)) return false;
  delay(2);

  uint16_t status;
  if (!sensirion::readWords(*_bus, _addr, &status, 1)) return false;

  // 11 bit thấp bằng 0 nghĩa là chưa có dữ liệu mới.
  ready = (status & 0x07FF) != 0;
  return true;
}

bool Scd41::read(uint16_t &co2ppm, float &tempC, float &rh) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_READ_MEASUREMENT)) return false;
  delay(2);

  uint16_t words[3];
  if (!sensirion::readWords(*_bus, _addr, words, 3)) return false;

  co2ppm = words[0];  // ppm, dùng trực tiếp
  tempC = -45.0f + 175.0f * (float)words[1] / 65535.0f;
  rh = 100.0f * (float)words[2] / 65535.0f;
  return true;
}

bool Scd41::getSerialNumber(uint64_t &serial) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_GET_SERIAL)) return false;
  delay(1);

  uint16_t words[3];
  if (!sensirion::readWords(*_bus, _addr, words, 3)) return false;

  serial = ((uint64_t)words[0] << 32) | ((uint64_t)words[1] << 16) | words[2];
  return true;
}
