// ---------------------------------------------------------------------------
// Smart Home prototype - firmware node ESP32-S3
//
// Giai đoạn này chỉ làm 2 việc: khai báo chân và giao tiếp được với từng linh
// kiện. Chưa có WiFi, chưa có MQTT/HTTP, chưa có logic tự động. Toàn bộ đầu ra
// đẩy ra Serial, điều khiển bằng lệnh gõ vào Serial Monitor.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config.h"
#include "pins.h"

#define HAS_ANY_SENSOR (ENABLE_SHT31 || ENABLE_SCD41 || ENABLE_SPS30 || ENABLE_SGP41)

// Khai báo trước: các hàm chẩn đoán ở phía trên cũng cần gỡ kẹt bus.
static bool ensureBus(uint8_t addr);
static void handleCommand(char *line);

#if ENABLE_WIFI
#include <ArduinoJson.h>
#include "drivers/net_mqtt.h"
static NetMqtt netMqtt;
#endif

// Biến lưu giữ số đo cảm biến mới nhất để gửi MQTT JSON
static float lastTempC = 25.0f;
static float lastRh = 50.0f;
static uint16_t lastCo2 = 0;
static int32_t lastVocIndex = -1;
static int32_t lastNoxIndex = -1;
static uint16_t lastRawVoc = 0;
static uint16_t lastRawNox = 0;
static float lastPm1 = 0.0f;
static float lastPm25 = 0.0f;
static float lastPm10 = 0.0f;

#if ENABLE_SHT31
#include "drivers/sht31.h"
static Sht31 sht31;
static bool sht31Ok = false;
#endif

#if ENABLE_SCD41
#include "drivers/scd41.h"
static Scd41 scd41;
static bool scd41Ok = false;
#endif

#if ENABLE_SPS30
#include "drivers/sps30.h"
static Sps30 sps30;
static bool sps30Ok = false;
#endif

#if ENABLE_SGP41
#include "drivers/sensirion_i2c.h"
#include "drivers/sgp41.h"
// Chu kỳ lấy mẫu phải khớp với thuật toán Gas Index.
static Sgp41 sgp41(SAMPLE_PERIOD_MS / 1000.0f);
static bool sgp41Ok = false;
#endif

#if ENABLE_RELAY
#include "drivers/relay.h"
struct RelayChannel {
  const char *name;
  uint8_t pin;
  Relay driver;
};
static RelayChannel relays[] = {
    {"den", PIN_RELAY_LIGHT, Relay()},
    {"quat", PIN_RELAY_FAN, Relay()},
};
static constexpr size_t RELAY_COUNT = sizeof(relays) / sizeof(relays[0]);
#endif

#if ENABLE_IR
#include "drivers/ir_tx.h"
static IrTx irTx;
#endif

// ---------------------------------------------------------------------------
// Tiện ích
// ---------------------------------------------------------------------------

// timeoutMs = 0 nghĩa là giữ nguyên timeout mặc định của driver.
static void i2cScan(uint16_t timeoutMs) {
  Serial.printf("--- quet I2C (timeout %u ms) ---\n",
                timeoutMs ? timeoutMs : Wire.getTimeOut());

  const uint16_t savedTimeout = Wire.getTimeOut();
  if (timeoutMs) Wire.setTimeOut(timeoutMs);

  const uint32_t startedAt = millis();
  uint8_t found = 0;
  uint8_t consecutiveTimeouts = 0;
  bool busDead = false;

  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    const uint8_t code = Wire.endTransmission();

    // Địa chỉ trống trên bus khoẻ trả về mã 2 (NACK) gần như tức thì. Mã 5
    // (hết thời gian chờ) nghĩa là tín hiệu không lên nổi mức cao - thường do
    // thiếu điện trở kéo lên. Gặp liên tiếp thì dừng, quét tiếp cũng vô nghĩa
    // mà lại treo cả phút.
    if (code == 5) {
      if (++consecutiveTimeouts >= 8) {
        busDead = true;
        break;
      }
      continue;
    }
    consecutiveTimeouts = 0;

    if (code == 0) {
      const char *name = "?";
      switch (addr) {
        case ADDR_SHT31: name = "SHT31"; break;
        case ADDR_SCD41: name = "SCD41"; break;
        case ADDR_SGP41: name = "SGP41"; break;
        case ADDR_SPS30: name = "SPS30"; break;
        default: break;
      }
      Serial.printf("  0x%02X  %s\n", addr, name);
      found++;
    }
  }
  Wire.setTimeOut(savedTimeout);

  if (busDead) {
    Serial.println(F("  DUNG SOM: bus khong phan hoi (toan bo bi timeout)"));
    Serial.println(F("  -> gan nhu chac chan thieu dien tro keo len 4.7k ra 3V3"));
    Serial.println(F("  -> hoac chua cam cam bien nao vao SDA=8 / SCL=9"));
  } else if (found == 0) {
    Serial.println(F("  bus khoe (dia chi trong tra ve NACK) nhung khong co ai"));
    Serial.println(F("  -> kiem tra dia chi va nguon cap cho cam bien"));
  }
  Serial.printf("--- xong sau %lu ms ---\n",
                (unsigned long)(millis() - startedAt));
}

static void printBanner() {
  Serial.println();
  Serial.println(F("==========================================="));
  Serial.printf("  Smart Home prototype - node %d\n", NODE_ID);
  Serial.printf("  build: %s %s\n", __DATE__, __TIME__);
  Serial.println(F("-------------------------------------------"));
  Serial.printf("  SHT31 : %s\n", ENABLE_SHT31 ? "bat" : "tat");
  Serial.printf("  SCD41 : %s\n", ENABLE_SCD41 ? "bat" : "tat (chua co hang)");
  Serial.printf("  SPS30 : %s\n", ENABLE_SPS30 ? "bat" : "tat (chua co hang)");
  Serial.printf("  SGP41 : %s\n", ENABLE_SGP41 ? "bat" : "tat");
  Serial.printf("  Relay : %s\n", ENABLE_RELAY ? "bat" : "tat");
  Serial.printf("  IR TX : %s\n", ENABLE_IR ? "bat" : "tat");
  Serial.println(F("==========================================="));
  Serial.println();
}

#if ENABLE_SGP41
// Soi từng bước giao tiếp với SGP41 để biết hỏng ở khâu nào: ghi lệnh bị NACK
// (dây/địa chỉ), hay ghi được mà không đọc nổi kết quả (thường là sụt nguồn khi
// heater bật).
// Giải nghĩa mã trả về của Wire.endTransmission().
static const char *txErrText(uint8_t code) {
  switch (code) {
    case 0: return "OK";
    case 1: return "du lieu qua dai";
    case 2: return "NACK o byte dia chi";
    case 3: return "NACK o byte du lieu";
    case 4: return "loi khac";
    case 5: return "het thoi gian cho";
    default: return "?";
  }
}

// Thử một lệnh có tham số rồi đọc kết quả, in rõ hỏng ở khâu ghi hay khâu đọc.
static void tryCommand(const char *label, uint16_t cmd, const uint16_t *args,
                       size_t nArgs, uint32_t waitMs, size_t nWords) {
  sensirion::generalCallReset(Wire);

  Wire.beginTransmission(ADDR_SGP41);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  for (size_t i = 0; i < nArgs; i++) {
    uint8_t w[2] = {(uint8_t)(args[i] >> 8), (uint8_t)(args[i] & 0xFF)};
    Wire.write(w, 2);
    Wire.write(sensirion::crc8(w, 2));
  }
  const uint8_t txCode = Wire.endTransmission();

  if (txCode != 0) {
    Serial.printf("  %-22s ghi LOI: ma %u (%s)\n", label, txCode,
                  txErrText(txCode));
    return;
  }

  delay(waitMs);
  uint16_t words[3] = {0, 0, 0};
  uint8_t tries = 0;
  if (!sensirion::readWordsRetry(Wire, ADDR_SGP41, words, nWords, 10, 10,
                                 &tries)) {
    Serial.printf("  %-22s ghi OK, doc LOI sau %u lan thu\n", label, tries);
    return;
  }

  Serial.printf("  %-22s OK (%u lan thu):", label, tries);
  for (size_t i = 0; i < nWords; i++) Serial.printf(" %04X", words[i]);
  Serial.println();
}

// Gửi một lệnh đo rồi in nguyên byte thô đọc về, kèm CRC nhận được so với CRC
// tự tính. Đủ để phân biệt: NACK, thiếu byte, hay dữ liệu về nhưng sai CRC.
static void dumpRead(const char *label, uint8_t addr, uint16_t cmd,
                     const uint16_t *args, size_t nArgs, uint32_t waitMs,
                     size_t nBytes) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  for (size_t i = 0; i < nArgs; i++) {
    uint8_t w[2] = {(uint8_t)(args[i] >> 8), (uint8_t)(args[i] & 0xFF)};
    Wire.write(w, 2);
    Wire.write(sensirion::crc8(w, 2));
  }
  const uint8_t tx = Wire.endTransmission();
  Serial.printf("  %s ghi: ma %u (%s)\n", label, tx, txErrText(tx));
  if (tx != 0) return;

  delay(waitMs);

  const size_t got = Wire.requestFrom((int)addr, (int)nBytes);
  uint8_t buf[16];
  size_t n = 0;
  while (Wire.available() && n < sizeof(buf)) buf[n++] = (uint8_t)Wire.read();

  Serial.printf("  %s doc: xin %u byte, nhan %u byte:", label,
                (unsigned)nBytes, (unsigned)got);
  for (size_t i = 0; i < n; i++) Serial.printf(" %02X", buf[i]);
  Serial.println();

  for (size_t i = 0; i + 2 < n; i += 3) {
    const uint8_t calc = sensirion::crc8(&buf[i], 2);
    Serial.printf("    word %u = %02X%02X  crc nhan %02X / tinh %02X  %s\n",
                  (unsigned)(i / 3), buf[i], buf[i + 1], buf[i + 2], calc,
                  calc == buf[i + 2] ? "khop" : "SAI");
  }
}

// Chỉ dò địa chỉ, không gửi lệnh gì, lặp trong 5 giây. Toàn 0 nghĩa là đường
// truyền ổn định (lỗi nằm ở chuỗi lệnh đo); lúc 0 lúc 2 nghĩa là tiếp xúc hoặc
// nguồn chập chờn.
static void diagPoll() {
  Serial.println(F("--- do dia chi lien tuc 5s (0=ACK, 2=NACK) ---"));
  uint8_t ackSht = 0, ackSgp = 0;
  for (int i = 0; i < 25; i++) {
    Wire.beginTransmission(ADDR_SHT31);
    const uint8_t a = Wire.endTransmission();
    Wire.beginTransmission(ADDR_SGP41);
    const uint8_t b = Wire.endTransmission();
    if (a == 0) ackSht++;
    if (b == 0) ackSgp++;
    Serial.printf("  t=%4dms   0x44 -> %u    0x59 -> %u\n", i * 200, a, b);
    delay(200);
  }
  Serial.printf("  tong ket: 0x44 ACK %u/25, 0x59 ACK %u/25\n", ackSht, ackSgp);
}

// Gửi lệnh rồi thử đọc liên tục cho tới khi có dữ liệu hoặc hết maxWaitMs.
// Dùng để trả lời: chip im hẳn, hay chỉ là trả kết quả muộn hơn datasheet?
static void pollRead(const char *label, uint8_t addr, uint16_t cmd,
                     const uint16_t *args, size_t nArgs, size_t nBytes,
                     uint32_t maxWaitMs) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  for (size_t i = 0; i < nArgs; i++) {
    uint8_t w[2] = {(uint8_t)(args[i] >> 8), (uint8_t)(args[i] & 0xFF)};
    Wire.write(w, 2);
    Wire.write(sensirion::crc8(w, 2));
  }
  const uint8_t tx = Wire.endTransmission();
  if (tx != 0) {
    Serial.printf("  %s ghi LOI: ma %u (%s)\n", label, tx, txErrText(tx));
    return;
  }

  const uint32_t startedAt = millis();
  while (millis() - startedAt < maxWaitMs) {
    if (Wire.requestFrom((int)addr, (int)nBytes) == (int)nBytes) {
      Serial.printf("  %s CO DU LIEU sau %lu ms:", label,
                    (unsigned long)(millis() - startedAt));
      while (Wire.available()) Serial.printf(" %02X", (uint8_t)Wire.read());
      Serial.println();
      return;
    }
    while (Wire.available()) Wire.read();  // dọn byte thừa nếu có
    delay(10);
  }
  Serial.printf("  %s im hoan toan sau %lu ms\n", label,
                (unsigned long)maxWaitMs);
}

// ---- deep test: thử mọi biến thể giao tiếp trước khi kết luận chip hỏng ----

static uint8_t writeCmd59(uint16_t cmd, const uint16_t *args, size_t nArgs,
                          bool sendStop) {
  Wire.beginTransmission(ADDR_SGP41);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  for (size_t i = 0; i < nArgs; i++) {
    uint8_t w[2] = {(uint8_t)(args[i] >> 8), (uint8_t)(args[i] & 0xFF)};
    Wire.write(w, 2);
    Wire.write(sensirion::crc8(w, 2));
  }
  return Wire.endTransmission(sendStop);
}

// Quét dải nhưng chừa một địa chỉ ra - để "khuấy" bus (gỡ wedge của driver)
// mà không gửi thêm giao dịch nào tới chính con chip đang giữ dữ liệu.
static void sweepExcept(uint8_t skip) {
  const uint16_t saved = Wire.getTimeOut();
  Wire.setTimeOut(10);
  for (uint8_t a = 0x08; a < 0x78; a++) {
    if (a == skip) continue;
    Wire.beginTransmission(a);
    Wire.endTransmission();
  }
  Wire.setTimeOut(saved);
}

// stir: 0 = đọc thẳng, 1 = khuấy bus (chừa 0x59) trước khi đọc,
//       2 = khuấy cả 0x59.
// preCmd != 0 thì gửi lệnh đó trước (vd heater off), chờ 20 ms rồi mới đo.
static void deepVariant(const char *label, uint16_t preCmd, uint16_t cmd,
                        const uint16_t *args, size_t nArgs, bool sendStop,
                        uint32_t waitMs, uint8_t stir, size_t nBytes) {
  ensureBus(ADDR_SGP41);
  sensirion::generalCallReset(Wire);
  delay(30);

  if (preCmd) {
    writeCmd59(preCmd, nullptr, 0, true);
    delay(20);
  }

  const uint8_t tx = writeCmd59(cmd, args, nArgs, sendStop);
  if (tx != 0 && sendStop) {
    Serial.printf("  %-26s ghi LOI ma %u\n", label, tx);
    return;
  }
  delay(waitMs);
  if (stir == 1) sweepExcept(ADDR_SGP41);
  if (stir == 2) sweepExcept(0);

  const size_t got = Wire.requestFrom((int)ADDR_SGP41, (int)nBytes);
  Serial.printf("  %-26s doc %u/%u byte", label, (unsigned)got,
                (unsigned)nBytes);
  while (Wire.available()) Serial.printf(" %02X", (uint8_t)Wire.read());
  Serial.println();
}

static void diagDeep() {
  Serial.println(F("--- deep test: cac bien the giao tiep voi SGP41 ---"));
  const uint16_t args[2] = {0x8000, 0x6666};

  deepVariant("1. goc: stop + 55ms", 0, 0x2619, args, 2, true, 55, 0, 6);
  // Core 2.x don write+read thanh mot giao dich repeated-start; neu chip biet
  // keo dai clock cho toi khi co so lieu thi kieu nay se an.
  deepVariant("2. repeated-start", 0, 0x2619, args, 2, false, 55, 0, 6);
  // Bit lo hong driver-wedge: NACK dau tien (chip dang do) co the lam driver
  // ket va bien moi lan doc lai thanh NACK gia -> khuay bus roi moi doc.
  deepVariant("3. khuay bus, tranh 0x59", 0, 0x2619, args, 2, true, 55, 1, 6);
  deepVariant("4. khuay bus, ca 0x59", 0, 0x2619, args, 2, true, 55, 2, 6);
  deepVariant("5. khong tham so", 0, 0x2619, nullptr, 0, true, 55, 0, 6);
  deepVariant("6. heater-off truoc", 0x3615, 0x2619, args, 2, true, 55, 0, 6);

  Wire.setClock(400000UL);
  deepVariant("7. bus 400kHz", 0, 0x2619, args, 2, true, 55, 0, 6);
  Wire.setClock(20000UL);
  deepVariant("8. bus 20kHz", 0, 0x2619, args, 2, true, 55, 0, 6);
  Wire.setClock(I2C_FREQ_HZ);

  deepVariant("9. cho im lang 500ms", 0, 0x2619, args, 2, true, 500, 0, 6);
  deepVariant("10. self-test + khuay", 0, 0x280E, nullptr, 0, true, 330, 1, 3);
  deepVariant("11. lenh SGP40 + khuay", 0, 0x260F, args, 2, true, 40, 1, 3);

  // Chip co con song sau tran tra tan nay khong?
  ensureBus(ADDR_SGP41);
  sensirion::generalCallReset(Wire);
  delay(30);
  uint16_t s[3];
  const bool alive = sgp41.getSerialNumber(s);
  Serial.printf("  kiem tra cuoi: serial %s\n",
                alive ? "van doc duoc" : "LOI - khong doc duoc nua");
  Serial.println(F("---------------------------------------------------"));
}

static void diagRaw() {
  Serial.println(F("--- doc tho ---"));
  const uint16_t args[2] = {0x8000, 0x6666};

  ensureBus(ADDR_SHT31);
  dumpRead("SHT31 0x2400", ADDR_SHT31, 0x2400, nullptr, 0, 20, 6);

  // 0x2619 / 0x2612 la lenh cua SGP41; 0x260F la lenh do cua SGP40.
  // Con nao tra loi thi biet dang cam con nao - ca hai deu o dia chi 0x59
  // va deu dap lenh get_serial_number 0x3682.
  ensureBus(ADDR_SGP41);
  dumpRead("SGP4x 0x3682 serial", ADDR_SGP41, 0x3682, nullptr, 0, 5, 9);
  ensureBus(ADDR_SGP41);
  dumpRead("SGP41 0x2619 do   ", ADDR_SGP41, 0x2619, args, 2, 60, 6);
  ensureBus(ADDR_SGP41);
  dumpRead("SGP40 0x260F do   ", ADDR_SGP41, 0x260F, args, 2, 40, 3);
  ensureBus(ADDR_SGP41);
  dumpRead("SGP4x 0x280E test ", ADDR_SGP41, 0x280E, nullptr, 0, 330, 3);

  // Cho that lau xem co phai chi la tra ket qua muon khong.
  ensureBus(ADDR_SGP41);
  pollRead("SGP41 0x2619 cho 2s", ADDR_SGP41, 0x2619, args, 2, 6, 2000);
  ensureBus(ADDR_SGP41);
  pollRead("SGP4x 0x280E cho 3s", ADDR_SGP41, 0x280E, nullptr, 0, 3, 3000);

  Serial.println(F("---------------"));
}

static void diagSgp41() {
  Serial.println(F("--- diag SGP41 ---"));

  Wire.beginTransmission(ADDR_SGP41);
  const uint8_t probe = Wire.endTransmission();
  Serial.printf("  do dia chi 0x59        ma %u (%s)\n", probe,
                txErrText(probe));

  Serial.printf("  general call reset     %s\n",
                sensirion::generalCallReset(Wire) ? "OK" : "LOI");

  const uint16_t args[2] = {0x8000, 0x6666};  // 50 %RH, 25 do C

  tryCommand("get_serial 0x3682", 0x3682, nullptr, 0, 2, 3);
  tryCommand("measure_raw 0x2619", 0x2619, args, 2, 55, 2);
  tryCommand("conditioning 0x2612", 0x2612, args, 2, 55, 1);
  tryCommand("self_test 0x280E", 0x280E, nullptr, 0, 330, 1);

  sensirion::generalCallReset(Wire);
  Serial.println(F("------------------"));
}
#endif

// Gỡ kẹt bus kiểu "sách vở": đánh 9 xung clock cho slave xả nốt byte đang giữ
// rồi phát STOP. Đây là cách chuẩn khi một slave kẹt giữa khung truyền dở.
//
// LƯU Ý: trên bàn thử, kiểu kẹt gặp phải KHÔNG phải slave kẹt (cả hai cảm biến
// "kẹt" đồng thời, và cách này không gỡ được) mà giống trạng thái xấu trong
// driver I2C của core sau một lần requestFrom thất bại - thứ gỡ được là quét
// dải địa chỉ (i2cSweep). Giữ hàm này sau lệnh 'recover' để thử tay khi gặp
// kiểu kẹt khác.
static void i2cRecover() {
  Wire.end();

  pinMode(PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  digitalWrite(PIN_I2C_SCL, HIGH);
  delayMicroseconds(10);

  for (uint8_t i = 0; i < 9; i++) {
    digitalWrite(PIN_I2C_SCL, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_I2C_SCL, HIGH);
    delayMicroseconds(10);
  }

  // STOP = SDA đi từ thấp lên cao trong lúc SCL đang ở mức cao.
  pinMode(PIN_I2C_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_I2C_SDA, LOW);
  delayMicroseconds(10);
  digitalWrite(PIN_I2C_SCL, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_I2C_SDA, HIGH);
  delayMicroseconds(10);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
}

static bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Quét hết dải địa chỉ, bỏ qua kết quả. Thực nghiệm trên bàn cho thấy cách này
// gỡ được kẹt trong khi 9 xung clock thì không.
//
// Hạ timeout trong lúc quét: NACK trả về ngay nên không ảnh hưởng tác dụng gỡ
// kẹt, nhưng tránh được vụ bus bị giữ mức thấp làm mỗi địa chỉ ăn đủ timeout
// (112 x 50 ms ~ 5.6 giây) - mà ensureBus thì gọi hàm này mỗi chu kỳ đọc.
// Timeout liên tiếp nhiều lần thì dừng sớm luôn, bus như vậy quét cũng vô ích.
static void i2cSweep() {
  const uint16_t savedTimeout = Wire.getTimeOut();
  Wire.setTimeOut(10);

  uint8_t consecutiveTimeouts = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 5) {
      if (++consecutiveTimeouts >= 8) break;
    } else {
      consecutiveTimeouts = 0;
    }
  }

  Wire.setTimeOut(savedTimeout);
}

// Kiểm tra slave còn đáp không; nếu không thì gỡ kẹt rồi kiểm tra lại.
//
// Đã thử trên bàn: gọi lại Wire.setTimeOut() không có tác dụng, 9 xung clock
// kiểu chuẩn cũng không. Chỉ quét hết dải địa chỉ mới gỡ được, nên dùng cách đó.
static bool ensureBus(uint8_t addr) {
  if (probe(addr)) return true;
  i2cSweep();
  return probe(addr);
}

// Khởi động lại ngoại vi I2C của ESP32. Dùng để phân biệt "cảm biến chết" với
// "bus phía ESP32 bị kẹt": nếu sau lệnh này quét lại thấy thiết bị thì lỗi nằm
// ở phía ESP32, còn vẫn không thấy gì thì cảm biến mới là thủ phạm.
static void i2cBusReset() {
  Wire.end();
  delay(50);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  Serial.println(F("da khoi dong lai ngoai vi I2C"));
}

static void printHelp() {
  Serial.println(F("Lenh:"));
  Serial.println(F("  help          - danh sach lenh"));
  Serial.println(F("  scan          - quet bus I2C"));
  Serial.println(F("  busreset      - khoi dong lai ngoai vi I2C"));
  Serial.println(F("  recover       - go ket bus I2C"));
#if HAS_ANY_SENSOR
  Serial.println(F("  reinit        - khoi tao lai cam bien sau khi sua day"));
#endif
#if ENABLE_RELAY
  Serial.println(F("  on <kenh>     - bat relay (vd: on den)"));
  Serial.println(F("  off <kenh>    - tat relay"));
  Serial.println(F("  toggle <kenh> - dao trang thai relay"));
#endif
#if ENABLE_IR
  Serial.println(F("  ir            - phat 1 khung NEC de test phan cung"));
#endif
#if ENABLE_SPS30
  Serial.println(F("  clean         - chay quat lam sach SPS30 (10s)"));
#endif
#if ENABLE_SGP41
  Serial.println(F("  diag          - soi tung buoc giao tiep voi SGP41"));
  Serial.println(F("  raw           - in byte tho cua lenh doc so do + CRC"));
  Serial.println(F("  deep          - thu moi bien the giao tiep voi SGP41"));
#endif
}

// ---------------------------------------------------------------------------
// Khởi tạo
// ---------------------------------------------------------------------------

#if HAS_ANY_SENSOR
static void initSensors() {
  // Gỡ kẹt trước mỗi lần khởi tạo: lệnh 'reinit' chạy từ trong loop, lúc đó
  // bus có thể đang kẹt do lần đọc hỏng trước đó.
#if ENABLE_SHT31
  ensureBus(ADDR_SHT31);
  sht31Ok = sht31.begin(Wire, ADDR_SHT31);
  Serial.printf("[init] SHT31 : %s\n", sht31Ok ? "OK" : "LOI");
#endif

#if ENABLE_SCD41
  ensureBus(ADDR_SCD41);
  scd41Ok = scd41.begin(Wire, ADDR_SCD41);
  if (scd41Ok) scd41Ok = scd41.startPeriodicMeasurement();
  Serial.printf("[init] SCD41 : %s\n", scd41Ok ? "OK" : "LOI");
#endif

#if ENABLE_SPS30
  ensureBus(ADDR_SPS30);
  sps30Ok = sps30.begin(Wire, ADDR_SPS30);
  if (sps30Ok) sps30Ok = sps30.startMeasurement();
  Serial.printf("[init] SPS30 : %s\n", sps30Ok ? "OK" : "LOI");
#endif

#if ENABLE_SGP41
  ensureBus(ADDR_SGP41);
  sgp41Ok = sgp41.begin(Wire);
  Serial.printf("[init] SGP41 : %s%s%s\n", sgp41Ok ? "OK" : "LOI",
                sgp41Ok ? "" : " - ", sgp41Ok ? "" : sgp41.lastError());
  if (sgp41Ok) {
    uint16_t testResult = 0;
    if (sgp41.selfTest(testResult)) {
      // Datasheet Table 15: bỏ qua byte cao, chỉ xét 2 bit thấp của byte thấp.
      // bit 0 = pixel VOC, bit 1 = pixel NOx; 1 nghĩa là pixel đó hỏng.
      Serial.printf("[init] SGP41 self test: 0x%04X -> VOC %s, NOx %s\n",
                    testResult, (testResult & 0x0001) ? "HONG" : "dat",
                    (testResult & 0x0002) ? "HONG" : "dat");
    } else {
      Serial.printf("[init] SGP41 self test: LOI - %s\n", sgp41.lastError());
    }
  }
  if (sgp41Ok) {
    // Heater cần làm nóng trước lần đo đầu, gọi 1 lan/giay.
    Serial.printf("[init] SGP41 conditioning %lus ",
                  (unsigned long)(SGP41_CONDITIONING_MS / 1000));
    const uint32_t until = millis() + SGP41_CONDITIONING_MS;
    while ((int32_t)(until - millis()) > 0) {
      uint16_t sraw;
      if (!ensureBus(ADDR_SGP41) || !sgp41.executeConditioning(sraw)) {
        sgp41Ok = false;
        break;
      }
      Serial.print('.');
      delay(950);
      yield();
    }
    if (sgp41Ok) {
      Serial.println(F(" xong"));
    } else {
      Serial.printf(" LOI - %s\n", sgp41.lastError());
    }
  }
#endif
}
#endif  // HAS_ANY_SENSOR

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Tắt Brownout detector tránh sụt áp nhẹ khi bật Wi-Fi RF

  Serial.begin(115200);
  const uint32_t serialDeadline = millis() + 2000;
  while (!Serial && (int32_t)(serialDeadline - millis()) > 0) {
    delay(10);
  }

  printBanner();

  pinMode(PIN_BUTTON, INPUT_PULLUP);

#if ENABLE_RELAY
  for (size_t i = 0; i < RELAY_COUNT; i++) {
    relays[i].driver.begin(relays[i].pin, RELAY_ACTIVE_LOW, false);
    Serial.printf("[init] relay %-5s -> GPIO %d\n", relays[i].name,
                  relays[i].pin);
  }
#endif

#if ENABLE_IR
  irTx.begin(PIN_IR_TX, IR_CARRIER_HZ, IR_DUTY_PERCENT);
  Serial.printf("[init] IR TX -> GPIO %d @ %lu Hz\n", PIN_IR_TX,
                (unsigned long)IR_CARRIER_HZ);
#endif

  // Kiểm tra mức logic 2 dây TRƯỚC khi khởi tạo I2C (lúc này chân vẫn là GPIO
  // thường nên đọc thoải mái). Bus rảnh phải là 1/1. Thấy 0 nghĩa là dây đang bị
  // giữ xuống thấp - chạm mát, hoặc slave treo giữa chừng; cứ khởi tạo I2C lúc
  // đó thì mọi giao dịch sẽ treo cho hết timeout.
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  delayMicroseconds(200);
  const int sdaLevel = digitalRead(PIN_I2C_SDA);
  const int sclLevel = digitalRead(PIN_I2C_SCL);
  Serial.printf("[init] muc logic bus: SDA=%d SCL=%d\n", sdaLevel, sclLevel);
  if (sdaLevel == 0 || sclLevel == 0) {
    Serial.println(F("[init] CANH BAO: day I2C dang bi giu muc thap!"));
    Serial.println(F("       -> kiem tra cham mat, jack long, hoac slave treo"));
  }

  // Bus I2C luôn được bật kể cả node không có cảm biến, để lệnh 'scan' dùng được.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  Serial.printf("[init] I2C -> SDA %d, SCL %d, %lu Hz\n", PIN_I2C_SDA,
                PIN_I2C_SCL, (unsigned long)I2C_FREQ_HZ);
  i2cScan(0);
#if HAS_ANY_SENSOR
  initSensors();
#endif

#if ENABLE_WIFI
  netMqtt.begin(WIFI_SSID, WIFI_PASS, MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS,
               [](const char *topic, const char *payload) {
                 Serial.printf("[MQTT RX] %s -> %s\n", topic, payload);
                 char cmdBuf[128];
                 strncpy(cmdBuf, payload, sizeof(cmdBuf) - 1);
                 cmdBuf[sizeof(cmdBuf) - 1] = '\0';
                 handleCommand(cmdBuf);
               });
#endif

  Serial.println();
  printHelp();
  Serial.println();
}

// ---------------------------------------------------------------------------
// Đọc cảm biến
// ---------------------------------------------------------------------------

#if HAS_ANY_SENSOR
static void sampleSensors() {
  Serial.printf("[%8.1fs]", millis() / 1000.0f);
  bool active = false;

#if ENABLE_SHT31
  if (sht31Ok) {
    active = true;
    float t, rh;
    if (ensureBus(ADDR_SHT31) && sht31.read(t, rh)) {
      lastTempC = t;
      lastRh = rh;
      Serial.printf("  T=%.2fC RH=%.1f%%", t, rh);
    } else {
      Serial.print(F("  SHT31=loi"));
    }
  }
#endif

#if ENABLE_SCD41
  if (scd41Ok) {
    active = true;
    bool ready = false;
    if (ensureBus(ADDR_SCD41) && scd41.isDataReady(ready) && ready) {
      uint16_t co2;
      float scdT, scdRh;  // SCD41 cũng có T/RH nhưng SHT31 chính xác hơn
      if (scd41.read(co2, scdT, scdRh)) {
        lastCo2 = co2;
        Serial.printf("  CO2=%uppm", co2);
#if !ENABLE_SHT31
        lastTempC = scdT;
        lastRh = scdRh;
#endif
      } else {
        Serial.print(F("  CO2=loi"));
      }
    } else {
      Serial.print(F("  CO2=..."));  // SCD41 ra so lieu moi 5s/lan
    }
  }
#endif

#if ENABLE_SPS30
  if (sps30Ok) {
    active = true;
    bool ready = false;
    if (ensureBus(ADDR_SPS30) && sps30.isDataReady(ready) && ready) {
      Sps30Data d;
      if (sps30.read(d)) {
        lastPm1 = d.pm1_0;
        lastPm25 = d.pm2_5;
        lastPm10 = d.pm10;
        Serial.printf("  PM1=%.1f PM2.5=%.1f PM10=%.1f ug/m3", d.pm1_0, d.pm2_5,
                      d.pm10);
      } else {
        Serial.print(F("  PM=loi"));
      }
    } else {
      Serial.print(F("  PM=..."));
    }
  }
#endif

#if ENABLE_SGP41
  if (sgp41Ok) {
    active = true;
    Sgp41Reading g;
    // Bù nhiệt-ẩm bằng số đo SHT31 mới nhất.
    if (ensureBus(ADDR_SGP41) && sgp41.measure(lastTempC, lastRh, g)) {
      lastVocIndex = g.vocIndex;
      lastNoxIndex = g.noxIndex;
      lastRawVoc = g.srawVoc;
      lastRawNox = g.srawNox;
      Serial.printf("  VOC=%ld NOx=%ld (raw %u/%u)", (long)g.vocIndex,
                    (long)g.noxIndex, g.srawVoc, g.srawNox);
    } else {
      Serial.printf("  SGP41=loi(%s)", sgp41.lastError());
    }
  }
#endif

  if (!active) {
    Serial.print(F("  chua co cam bien nao khoi tao duoc - go 'scan'"));
  }
  Serial.println();

#if ENABLE_WIFI
  if (netMqtt.isConnected()) {
    StaticJsonDocument<384> doc;
    doc["node_id"] = NODE_ID;
    doc["uptime"] = millis() / 1000;
#if ENABLE_SHT31
    if (sht31Ok) {
      doc["temp"] = lastTempC;
      doc["rh"] = lastRh;
    }
#endif
#if ENABLE_SCD41
    if (scd41Ok && lastCo2 > 0) {
      doc["co2"] = lastCo2;
    }
#endif
#if ENABLE_SGP41
    if (sgp41Ok && lastVocIndex >= 0) {
      doc["voc_index"] = lastVocIndex;
      doc["nox_index"] = lastNoxIndex;
      doc["sraw_voc"] = lastRawVoc;
      doc["sraw_nox"] = lastRawNox;
    }
#endif
#if ENABLE_SPS30
    if (sps30Ok) {
      doc["pm1_0"] = lastPm1;
      doc["pm2_5"] = lastPm25;
      doc["pm10"] = lastPm10;
    }
#endif
    char jsonBuf[384];
    serializeJson(doc, jsonBuf);
    netMqtt.publishTelemetry(jsonBuf);
  }
#endif
}
#endif  // HAS_ANY_SENSOR

// ---------------------------------------------------------------------------
// Console qua Serial
// ---------------------------------------------------------------------------

static void handleCommand(char *line) {
  char *cmd = strtok(line, " ");
  if (cmd == nullptr) return;
  char *arg = strtok(nullptr, " ");
  (void)arg;  // chỉ vài lệnh cần tham số, tùy cấu hình build

  if (strcmp(cmd, "help") == 0) {
    printHelp();
    return;
  }
  if (strcmp(cmd, "scan") == 0) {
    i2cScan(arg ? (uint16_t)atoi(arg) : 0);
    return;
  }
  if (strcmp(cmd, "busreset") == 0) {
    i2cBusReset();
    return;
  }
#if HAS_ANY_SENSOR
  if (strcmp(cmd, "reinit") == 0) {
    initSensors();
    return;
  }
#endif
  if (strcmp(cmd, "recover") == 0) {
    i2cRecover();
    Serial.println(F("da danh 9 xung clock + STOP de go ket bus"));
    return;
  }
#if ENABLE_SGP41
  if (strcmp(cmd, "diag") == 0) {
    diagSgp41();
    return;
  }
  if (strcmp(cmd, "raw") == 0) {
    diagRaw();
    return;
  }
  if (strcmp(cmd, "deep") == 0) {
    diagDeep();
    return;
  }
  if (strcmp(cmd, "poll") == 0) {
    diagPoll();
    return;
  }
#endif

#if ENABLE_RELAY
  const bool isOn = strcmp(cmd, "on") == 0;
  const bool isOff = strcmp(cmd, "off") == 0;
  const bool isToggle = strcmp(cmd, "toggle") == 0;
  if (isOn || isOff || isToggle) {
    if (arg == nullptr) {
      Serial.println(F("thieu ten kenh (den / quat)"));
      return;
    }
    for (size_t i = 0; i < RELAY_COUNT; i++) {
      if (strcmp(arg, relays[i].name) != 0) continue;
      if (isToggle) {
        relays[i].driver.toggle();
      } else {
        relays[i].driver.set(isOn);
      }
      Serial.printf("relay %s -> %s\n", relays[i].name,
                    relays[i].driver.isOn() ? "BAT" : "TAT");
      return;
    }
    Serial.printf("khong co kenh '%s'\n", arg);
    return;
  }
#endif

#if ENABLE_IR
  if (strcmp(cmd, "ir") == 0) {
    irTx.sendNec(0x00, 0x45);
    Serial.println(F("da phat 1 khung NEC (addr 0x00, cmd 0x45)"));
    return;
  }
#endif

#if ENABLE_SPS30
  if (strcmp(cmd, "clean") == 0) {
    Serial.println(sps30.startFanCleaning() ? F("SPS30 dang lam sach 10s")
                                            : F("SPS30 loi"));
    return;
  }
#endif

  Serial.printf("lenh la: '%s' (go 'help')\n", cmd);
}

static void pollSerial() {
  static char buf[64];
  static size_t len = 0;

  while (Serial.available()) {
    const char c = (char)Serial.read();
    // Nhận cả \r lẫn \n làm kết thúc dòng: pio device monitor gửi CR khi nhấn
    // Enter, script/terminal khác gửi LF. Với CRLF, ký tự thứ hai tạo ra dòng
    // rỗng và bị bỏ qua.
    if (c == '\r' || c == '\n') {
      buf[len] = '\0';
      if (len > 0) handleCommand(buf);
      len = 0;
      continue;
    }
    if (len < sizeof(buf) - 1) buf[len++] = c;
  }
}

void loop() {
  pollSerial();

#if ENABLE_WIFI
  netMqtt.loop();
#endif

#if HAS_ANY_SENSOR
  // Lần lấy mẫu đầu lùi lại 1 chu kỳ, cho cảm biến ổn định sau khi khởi tạo.
  static uint32_t nextSample = SAMPLE_PERIOD_MS;
  if ((int32_t)(millis() - nextSample) >= 0) {
    nextSample = millis() + SAMPLE_PERIOD_MS;
    sampleSensors();
  }
#endif

  delay(5);
}
