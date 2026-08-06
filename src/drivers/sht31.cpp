#include "drivers/sht31.h"

#include "drivers/sensirion_i2c.h"

namespace {
constexpr uint16_t CMD_SOFT_RESET = 0x30A2;
constexpr uint16_t CMD_READ_STATUS = 0xF32D;
constexpr uint16_t CMD_MEASURE_HIGH_REP = 0x2400;  // single shot, no stretch
constexpr uint32_t MEASURE_DURATION_MS = 20;       // datasheet: max 15 ms
}  // namespace

bool Sht31::begin(TwoWire &bus, uint8_t addr) {
  _bus = &bus;
  _addr = addr;
  if (!softReset()) return false;
  delay(5);
  uint16_t status;
  return readStatus(status);
}

bool Sht31::softReset() {
  return sensirion::sendCommand(*_bus, _addr, CMD_SOFT_RESET);
}

bool Sht31::readStatus(uint16_t &status) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_READ_STATUS)) return false;
  delay(2);
  return sensirion::readWords(*_bus, _addr, &status, 1);
}

bool Sht31::read(float &tempC, float &rh) {
  if (!sensirion::sendCommand(*_bus, _addr, CMD_MEASURE_HIGH_REP)) return false;
  delay(MEASURE_DURATION_MS);

  uint16_t words[2];
  if (!sensirion::readWords(*_bus, _addr, words, 2)) return false;

  tempC = -45.0f + 175.0f * (float)words[0] / 65535.0f;
  rh = 100.0f * (float)words[1] / 65535.0f;
  return true;
}
