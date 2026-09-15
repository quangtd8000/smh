// ---------------------------------------------------------------------------
// Env đối chứng - KHÔNG phải firmware thật.
//   pio run -e libtest -t upload -t monitor
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <SensirionI2CSgp41.h>
#include <Wire.h>

#include "config.h"
#include "pins.h"

static SensirionI2CSgp41 sgp41;

static void printError(const char *label, uint16_t error) {
  if (error == 0) {
    Serial.printf("  %-22s OK\n", label);
    return;
  }
  char msg[128];
  errorToString(error, msg, sizeof(msg));
  Serial.printf("  %-22s LOI: %s\n", label, msg);
}

// Cùng cách gỡ kẹt như firmware chính: quét hết dải địa chỉ.
static void sweep() {
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    Wire.endTransmission();
  }
}

static bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void ensureBus(uint8_t addr) {
  if (probe(addr)) return;
  sweep();
}

static void runOnce() {
  Serial.println(F("=== thu vien chinh hang Sensirion I2C SGP41 ==="));

  ensureBus(ADDR_SGP41);
  Serial.printf("  dia chi 0x59 ACK       %s\n", probe(ADDR_SGP41) ? "co" : "KHONG");

  uint16_t serial[3] = {0, 0, 0};
  ensureBus(ADDR_SGP41);
  uint16_t err = sgp41.getSerialNumber(serial);
  printError("getSerialNumber", err);
  if (err == 0) {
    Serial.printf("  serial                 %04X %04X %04X\n", serial[0],
                  serial[1], serial[2]);
  }

  uint16_t testResult = 0;
  ensureBus(ADDR_SGP41);
  err = sgp41.executeSelfTest(testResult);
  printError("executeSelfTest", err);
  if (err == 0) {
    Serial.printf("  ket qua self test      0x%04X (%s)\n", testResult,
                  (testResult & 0x00FF) == 0 ? "dat" : "khong dat");
  }

  uint16_t srawVoc = 0;
  ensureBus(ADDR_SGP41);
  err = sgp41.executeConditioning(0x8000, 0x6666, srawVoc);
  printError("executeConditioning", err);
  if (err == 0) Serial.printf("  SRAW_VOC               %u\n", srawVoc);

  uint16_t srawNox = 0;
  ensureBus(ADDR_SGP41);
  err = sgp41.measureRawSignals(0x8000, 0x6666, srawVoc, srawNox);
  printError("measureRawSignals", err);
  if (err == 0) {
    Serial.printf("  SRAW_VOC / SRAW_NOX    %u / %u\n", srawVoc, srawNox);
  }

  ensureBus(ADDR_SGP41);
  printError("turnHeaterOff", sgp41.turnHeaterOff());

  Serial.println(F("=============================================="));
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("--- env libtest: doi chung bang thu vien chinh hang ---"));

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  sgp41.begin(Wire);

  runOnce();
}

void loop() {
  delay(5000);
  runOnce();
}
