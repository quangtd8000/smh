#include "drivers/sgp41.h"

#if ENABLE_SGP41

#include <string.h>

namespace {
// Giá trị mặc định khi không có bù nhiệt-ẩm: 50 %RH và 25 độ C.
constexpr uint16_t DEFAULT_RH_TICKS = 0x8000;
constexpr uint16_t DEFAULT_T_TICKS = 0x6666;

uint16_t rhToTicks(float rh) {
  if (rh < 0.0f) rh = 0.0f;
  if (rh > 100.0f) rh = 100.0f;
  return (uint16_t)lroundf(rh * 65535.0f / 100.0f);
}

uint16_t tempToTicks(float tempC) {
  if (tempC < -45.0f) tempC = -45.0f;
  if (tempC > 130.0f) tempC = 130.0f;
  return (uint16_t)lroundf((tempC + 45.0f) * 65535.0f / 175.0f);
}
}  // namespace

Sgp41::Sgp41(float samplingIntervalS) : _voc(samplingIntervalS) {
  _lastError[0] = '\0';
}

bool Sgp41::check(uint16_t error) {
  if (error == 0) {
    _lastError[0] = '\0';
    return true;
  }
  errorToString(error, _lastError, sizeof(_lastError));
  return false;
}

bool Sgp41::begin(TwoWire &bus) {
  _dev.begin(bus);
  uint16_t serial[3];
  return getSerialNumber(serial);
}

bool Sgp41::getSerialNumber(uint16_t serial[3]) {
  return check(_dev.getSerialNumber(serial));
}

bool Sgp41::selfTest(uint16_t &result) {
  return check(_dev.executeSelfTest(result));
}

bool Sgp41::executeConditioning(uint16_t &srawVoc) {
  return check(_dev.executeConditioning(DEFAULT_RH_TICKS, DEFAULT_T_TICKS,
                                        srawVoc));
}

bool Sgp41::measure(float tempC, float rh, Sgp41Reading &out) {
  if (!check(_dev.measureRawSignals(rhToTicks(rh), tempToTicks(tempC),
                                    out.srawVoc, out.srawNox))) {
    return false;
  }
  out.vocIndex = _voc.process((int32_t)out.srawVoc);
  out.noxIndex = _nox.process((int32_t)out.srawNox);
  return true;
}

bool Sgp41::turnHeaterOff() { return check(_dev.turnHeaterOff()); }

#endif  // ENABLE_SGP41
