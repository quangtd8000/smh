# Smart Home — prototype

Firmware ESP32-S3 giai đoạn sơ khai: **khai báo chân + giao tiếp được với từng linh kiện**.
Chưa có WiFi, chưa có MQTT/HTTP, chưa có server, chưa có logic tự động — những thứ đó để giai đoạn sau.

Mọi số liệu in ra Serial (115200 baud), điều khiển bằng lệnh gõ vào Serial Monitor.

## Linh kiện

| Chức năng | Linh kiện | SL | Giao tiếp | Trạng thái |
|---|---|---|---|---|
| Nhiệt độ, độ ẩm | SHT31 | 1 | I2C `0x44` | ✅ có hàng |
| CO2 | SCD41 | 1 | I2C `0x62` | ⏳ **chưa có hàng** |
| Bụi mịn PM2.5 | SPS30 | 1 | I2C `0x69` | ⏳ **chưa có hàng** |
| TVOC / NOx (đa khí) | SGP41 | 1 | I2C `0x59` | ✅ có hàng |
| Bật tắt đèn, quạt AC | Relay | 1 | GPIO | ✅ có hàng |
| Node điều khiển | ESP32-S3 | 3 | — | ✅ có hàng |
| Điều khiển điều hòa | Module IR VOKTTA 1W | 1 | GPIO + LEDC 38 kHz | ✅ có hàng |

SCD41 và SPS30 đang để `ENABLE_* = 0` trong `platformio.ini`. Driver đã viết đủ và **đã kiểm tra biên dịch được**; khi hàng về chỉ cần đổi cờ thành `1`.

## Sơ đồ chân — ESP32-S3-DevKitC-1

| Tín hiệu | GPIO | Ghi chú |
|---|---|---|
| I2C SDA | 8 | dùng chung 4 cảm biến, cần điện trở kéo lên 4.7k ra 3V3 |
| I2C SCL | 9 | như trên |
| Relay đèn | 4 | mặc định kích mức thấp |
| Relay quạt | 5 | mặc định kích mức thấp |
| IR TX | 17 | tín hiệu điều khiển module VOKTTA |
| Nút nhấn | 18 | pull-up nội, nhấn = LOW (dự phòng) |

Các chân **tránh dùng** trên board này: 0/3/45/46 (strapping), 19/20 (USB D-/D+), 26–32 (SPI flash), 33–37 (Octal PSRAM bản N16R8), 43/44 (UART0 debug).

## Lưu ý phần cứng

- **Tốc độ I2C = 100 kHz.** SCD41 và SPS30 đều chỉ chịu tối đa 100 kHz, nên cả bus phải chạy ở mức này. Đừng nâng lên 400 kHz kể cả khi chỉ có SHT31 trên bus, vì sau này 2 con kia sẽ cắm chung.
- **Pull-up:** pull-up nội của ESP32 khoảng 45k, quá yếu cho 4 slave trên cùng bus. Phải hàn 4.7k ra 3V3 cho cả SDA và SCL.
- **SPS30** cần 3 thứ, thiếu một là không thấy chip:
  1. Nguồn **5V** (không phải 3V3), dòng đỉnh ~80 mA lúc quạt khởi động.
  2. Chân **SEL (pin 4) nối GND *trước khi* cấp nguồn** để chọn chế độ I2C. Thả nổi = chế độ UART.
  3. Mức logic I2C của SPS30 tham chiếu theo VDD 5V — kiểm tra lại datasheet bản đang cầm, cần thì thêm level shifter thay vì nối thẳng vào ESP32 3V3.
- **SGP41** phải chạy conditioning 10 giây (làm nóng heater) trước lần đo đầu tiên. Firmware đã tự làm trong `setup()`, nên node có SGP41 sẽ mất ~10 s mới bắt đầu in số liệu.
- **Relay:** phía tiếp điểm là điện lưới 220V. Lúc thử nghiệm nên đấu tải giả (bóng đèn nhỏ) và giữ phần cao áp tách biệt hoàn toàn với phần ESP32.
- **Serial / 2 cổng USB:** DevKitC-1 có cổng **USB** (USB gốc của ESP32-S3) và cổng **UART** (qua chip cầu CH343/CP2102). Mặc định `platformio.ini` đang bật `ARDUINO_USB_CDC_ON_BOOT=1` nên console ra **cổng USB**. Nếu muốn cắm cổng UART thì bỏ 2 dòng `ARDUINO_USB_*` trong `platformio.ini`, khi đó `Serial` là UART0 trên GPIO 43/44. Cắm nhầm cổng thì nạp vẫn được nhưng màn hình trống trơn.

## Build & nạp

Có 4 env, cùng một firmware, khác nhau ở cờ bật/tắt module:

| Env | Vai trò | Module bật |
|---|---|---|
| `node-air` | node cảm biến không khí | SHT31, SGP41 (+ SCD41, SPS30 khi có hàng) |
| `node-relay` | node công tắc AC | relay đèn + quạt |
| `node-ir` | node điều khiển điều hòa | IR TX |
| `bench` | test trên bàn | bật hết những gì đang có |

Build một env:

```bash
pio run -e bench
```

Nạp và mở Serial Monitor:

```bash
pio run -e bench -t upload -t monitor
```

Build cả 4 env để kiểm tra:

```bash
pio run
```

## Lệnh trên Serial Monitor

```
help          - danh sách lệnh
scan [ms]     - quét bus I2C; [ms] là timeout, bỏ trống = mặc định của driver
busreset      - khởi động lại ngoại vi I2C của ESP32
on <kênh>     - bật relay, vd: on den / on quat
off <kênh>    - tắt relay
toggle <kênh> - đảo trạng thái relay
ir            - phát 1 khung NEC để test module hồng ngoại
diag          - soi từng bước giao tiếp với SGP41
clean         - chạy quạt làm sạch SPS30 (10 s)
```

Lệnh `scan` là thứ nên chạy đầu tiên khi cắm cảm biến mới. Nó phân biệt 2 tình huống khác hẳn nhau:

- **"bus khoe ... nhung khong co ai"** — bus điện tốt, địa chỉ trống trả về NACK ngay. Vấn đề là địa chỉ hoặc nguồn cấp cho cảm biến.
- **"DUNG SOM: bus khong phan hoi"** — mọi địa chỉ đều timeout, tín hiệu không lên nổi mức cao. Gần như luôn là **thiếu điện trở kéo lên**, hoặc chưa cắm gì vào SDA/SCL.

Phân biệt được là nhờ mã trả về của `Wire.endTransmission()`: mã 2 (NACK) = bus khoẻ, mã 5 (timeout) = bus không hoạt động.

## Cấu trúc

```
platformio.ini          4 env, cờ ENABLE_* cho từng module
include/
  config.h              cờ bật/tắt module, tần số I2C, chu kỳ đọc
  pins.h                sơ đồ chân + địa chỉ I2C
src/
  main.cpp              khởi tạo, vòng đọc cảm biến, console Serial
  drivers/
    sensirion_i2c.*     CRC8 + khung lệnh/đọc chung, dùng cho SHT31/SCD41/SPS30
    sht31.*             nhiệt độ, độ ẩm
    scd41.*             CO2
    sps30.*             bụi mịn
    sgp41.*             VOC / NOx Index - bọc quanh thư viện chính hãng
    relay.*             đóng cắt tải AC
    ir_tx.*             sóng mang 38 kHz + gửi raw / NEC
```

SHT31, SCD41, SPS30 dùng driver viết tay, không phụ thuộc thư viện ngoài. Ba con này dùng chung một khung giao thức (lệnh 16-bit MSB trước, dữ liệu chia word 16-bit kèm CRC8 poly 0x31 init 0xFF) nên phần dùng chung nằm hết trong `sensirion_i2c.*`.

**SGP41 thì dùng thư viện chính hãng** (xem mục dưới) vì phần đổi tín hiệu thô sang Index là thuật toán thật, không nên tự viết. `sgp41.*` chỉ là lớp bọc mỏng cho khớp với cách gọi của các driver còn lại. Driver này kéo theo thư viện ngoài nên được bọc trong `#if ENABLE_SGP41` — node relay và node IR không khai báo `lib_deps` nên phải bỏ qua lúc biên dịch.

## Thư viện có sẵn ngoài registry

Đã khảo sát và đối chứng với bản chính hãng. Ghi lại để sau này cần thì lấy dùng ngay:

| Cảm biến | Thư viện | Ghi chú |
|---|---|---|
| SHT31 | `robtillaart/SHT31` | Sensirion không phát hành lib SHT3x cho Arduino (chỉ có SHT4x). `robtillaart/SHT85` cũng chạy được SHT31. |
| SCD41 | `sensirion/Sensirion I2C SCD4x` | Chính hãng, đang bảo trì. |
| SPS30 | `sensirion/Sensirion I2C SPS30` | Chính hãng. `paulvha/sps30` phổ biến hơn, hỗ trợ cả I2C lẫn UART. |
| SGP41 | `sensirion/Sensirion I2C SGP41` | Chính hãng. Còn có `adafruit/Adafruit SGP41` và `7semi-solutions/7Semi_SGP4x`. |
| VOC / NOx Index | `sensirion/Sensirion Gas Index Algorithm` | **Nên dùng khi SGP41 chạy được.** Đây là thuật toán thật, đừng tự viết lại. |
| Tất cả | `sensirion/Sensirion UPT I2C Auto Detection` | Tự dò cảm biến Sensirion trên bus, tiện lúc thử phần cứng. |

Tất cả đều phụ thuộc `sensirion/Sensirion Core` (lo phần đóng khung và CRC — đúng vai trò của `sensirion_i2c.*` trong project này).

### SGP41 đang dùng thư viện chính hãng

Đã chuyển sang `Sensirion I2C SGP41` + `Sensirion Gas Index Algorithm`, khai báo trong section `[sgp41_libs]` của `platformio.ini` và dùng lại cho các env cần.

Lý do chính là **Gas Index Algorithm**: chip chỉ cho ra ticks thô, con số đó không dùng trực tiếp được. Thuật toán biến nó thành Index thang 0–500 và tự học mức nền của phòng:

- **VOC Index**: 100 là mức nền, cao hơn nghĩa là không khí đang xấu đi so với nền, thấp hơn là đang tốt lên.
- **NOx Index**: 1 là mức nền.
- Khoảng **1 giờ đầu** sau khi cấp nguồn thuật toán còn đang học, giá trị chưa đáng tin.

Vài điểm cần biết khi dùng:

- Thư viện **cố định địa chỉ I2C là 0x59**, không truyền địa chỉ vào được.
- `SAMPLE_PERIOD_MS` đã đổi thành **1000 ms** cho khớp: thuật toán VOC được Sensirion kiểm chứng ở 1 s (mặc định) và 10 s, còn phần NOx luôn giả định 1 s. Đổi chu kỳ thì nhớ đổi cả tham số truyền vào constructor `Sgp41`.
- `executeSelfTest` **đạt khi trả về đúng `0xD400`** — theo example chính hãng, không phải chỉ xét byte thấp.
- Lỗi của thư viện được đưa thẳng ra console qua `errorToString`, ví dụ `Not enough data received` thay vì chỉ báo "loi".

Đã so mã nguồn trước khi đổi: driver tự viết gửi đúng cùng lệnh và cùng thời gian chờ như bản chính hãng, nên **các cảm biến còn lại giữ driver tự viết** — ít phụ thuộc và dễ soi khi cần chẩn đoán (chính nhờ nó mà lần ra được vụ kẹt bus).

## Bẫy đã gặp: bus "kẹt" sau một lần đọc thất bại

Trên bàn gặp hiện tượng này và nó tốn khá nhiều thời gian để lần ra, nên ghi lại:

Sau khi khởi tạo xong, mọi giao dịch I2C bắt đầu trả về NACK (`endTransmission` mã 2) dù SDA/SCL vẫn ở mức cao. Chỉ reset ESP32 hoặc quét dải địa chỉ mới hồi phục.

Ba dữ kiện đã đo được:

1. **Cả hai cảm biến "kẹt" đồng thời** — 0x44 và 0x59 cùng ngừng ACK tại cùng một thời điểm, và cùng hồi phục sau một lần quét. Hai chip độc lập không thể cùng kẹt giữa khung truyền như vậy.
2. Thời điểm kẹt luôn là **ngay sau một lần `requestFrom` thất bại** trên con SGP41 hỏng (log core: `i2cRead returned Error -1`).
3. Hiệu quả các cách gỡ:

| Cách gỡ | Kết quả trên phần cứng này |
|---|---|
| Quét hết 0x08–0x77 | ✅ gỡ được, tức thì |
| `Wire.end()` + `Wire.begin()` | ❌ không |
| 9 xung clock + STOP (cách chuẩn cho slave kẹt) | ❌ không |

Nếu slave thật sự kẹt thì 9 xung clock phải là cách hiệu quả nhất. Thực tế ngược lại → thủ phạm khớp hơn là **trạng thái xấu trong driver I2C của Arduino core 2.0.17 phía ESP32** sau lần đọc thất bại, báo NACK giả cho mọi giao dịch; SHT31 chỉ là nạn nhân dây chuyền của con SGP41 hỏng.

Xử lý: `ensureBus()` trong [main.cpp](src/main.cpp) dò địa chỉ trước mỗi lần đọc, thấy NACK thì quét dải (timeout ngắn + dừng sớm nếu bus chết hẳn) rồi dò lại. **Dự đoán kiểm chứng được:** thay SGP41 xong, không còn lần đọc thất bại nào mỗi giây nữa thì hiện tượng kẹt sẽ biến mất; nếu vẫn còn thì cần xem lại kết luận này. Về lâu dài có thể cân nhắc chuyển lên Arduino core 3.x (driver I2C viết lại), nhưng với workaround hiện tại thì chưa cần.

## Tình trạng thực tế trên bàn (lần chạy 01/08/2026)

Đã nạp env `bench` lên board qua COM12 và chạy thử.

**Chạy đúng:**

- Boot, banner, khởi tạo relay và IR, vòng đọc theo chu kỳ 2 s, console Serial.
- Relay: `on den` / `on quat` / `toggle den` / `off quat` đều đổi trạng thái đúng trên GPIO 4 và 5.
- IR: lệnh `ir` phát khung NEC, LEDC lên sóng mang 38 kHz trên GPIO 17.
- Lệnh sai được báo lại thay vì làm chương trình chết.

- **SHT31 đọc ra số thật và ổn định:** T ≈ 26,6 °C, RH ≈ 64,7 %, đều đặn mỗi 2 giây, không trôi.
- Quét I2C thấy cả `0x44` và `0x59`, xong trong 15 ms.

**Chưa chạy — SGP41:**

Chip sống và giao tiếp tốt: `get_serial_number` (0x3682) trả về đủ 9 byte, CRC khớp cả 3 word, serial `0000 0367 ECA1`, lặp lại bao nhiêu lần cũng đúng.

Nhưng **mọi lệnh cần bật heater đều nhận lệnh rồi không bao giờ ra kết quả** — ghi được ACK, đọc về 0 byte:

| Lệnh | Ghi | Đọc |
|---|---|---|
| `0x3682` đọc serial (không dùng heater) | OK | ✅ 9/9 byte, CRC khớp |
| `0x2619` measure_raw_signals (SGP41) | OK | ❌ 0 byte |
| `0x260F` measure_raw (bộ lệnh SGP40) | OK | ❌ 0 byte |
| `0x280E` self test | OK | ❌ 0 byte |

Đã loại trừ: không phải nhầm SGP40 (lệnh SGP40 cũng hỏng), không phải bus (SHT31 chạy ngon trên cùng bus), không phải timing (đã chờ dư và thử lại nhiều lần), không phải relay hay module IR (tắt hẳn vẫn vậy).

**Đã đối chứng bằng thư viện chính hãng.** Env `libtest` chạy `sensirion/Sensirion I2C SGP41` 1.0.0 (không dùng driver tự viết) và cho kết quả y hệt, lặp lại 5 lần liên tiếp:

```
dia chi 0x59 ACK       co
getSerialNumber        OK          serial 0000 0367 ECA1
turnHeaterOff          OK
executeSelfTest        LOI: Not enough data received
executeConditioning    LOI: Not enough data received
measureRawSignals      LOI: Not enough data received
```

So mã nguồn thì hai bên gửi giống hệt nhau: `0x2612` + chờ 50 ms, `0x2619` + chờ 50 ms, `0x280E` + chờ 320 ms, `0x3682` + chờ 1 ms. `turnHeaterOff` là lệnh chỉ ghi không đọc nên vẫn OK — khớp với quy luật: ghi luôn được ACK, chỉ lệnh phải trả về số đo mới hỏng.

**Đã thử tụ 10 µF — không thay đổi gì.** Hàn tụ sát chân VDD rồi test lại: SHT31 vẫn tốt, serial SGP41 vẫn đọc đúng, các lệnh heater vẫn im.

**Đã loại trừ cả chuyện chờ chưa đủ lâu.** Cho chờ 2 giây với lệnh đo (datasheet ghi 50 ms) và 3 giây với self test (datasheet ghi 320 ms) — chip im hoàn toàn, không trả về byte nào.

Tổng hợp những gì đã loại trừ:

| Nghi ngờ | Cách kiểm chứng | Kết quả |
|---|---|---|
| Driver tự viết sai | Chạy thư viện chính hãng | Hỏng y hệt |
| Bus I2C | SHT31 chạy tốt cùng bus, cùng lúc | Bus tốt |
| Thiếu điện trở kéo lên | Đã hàn 4k7 | Đã sửa, không liên quan |
| Chờ chưa đủ lâu | Chờ 2–3 s | Vẫn im |
| Nguồn thiếu tụ lọc | Hàn tụ 10 µF | Không đổi |
| Nhầm SGP40 | Thử lệnh `0x260F` của SGP40 | Cũng im |
| Relay / module IR nhiễu | Tắt hẳn cả hai | Không đổi |
| Cách viết khác trong lib bên thứ ba | Đọc mã Adafruit SGP41 + 7Semi_SGP4x | Cùng lệnh, cùng delay, không có gì khác |
| NACK giả do driver wedge che mất chip | `deep` #3/#4: khuấy bus giữa write và read rồi mới đọc | Vẫn 0 byte |
| Repeated-start / clock stretching | `deep` #2 | 0 byte |
| Tốc độ bus | `deep` #7/#8: 400 kHz và 20 kHz | 0 byte |
| Biến thể lệnh | `deep` #5/#6: không tham số, heater-off trước | 0 byte |
| Chờ im lặng tuyệt đối | `deep` #9: 500 ms không đụng bus, đọc 1 phát | 0 byte |

Lệnh `deep` chạy cả 11 biến thể một lượt và vẫn nằm sẵn trong firmware — dùng để nghiệm thu con SGP41 thay thế (con tốt phải ra byte ngay ở biến thể #1).

### Đối chiếu datasheet chính hãng (SGP41 v1.0, 12/2021)

Đã soát từng thông số với datasheet. Kết quả chốt hạ:

- **Khung byte gửi đi đúng từng byte.** Table 9 in nguyên chuỗi mẫu của lệnh conditioning: `0x26 0x12 0x80 0x00 0xA2 0x66 0x66 0x93` — trùng khớp 100% với thứ firmware gửi, kể cả 2 byte CRC `0xA2`/`0x93`.
- **Hợp đồng giao tiếp (mục 4.5):** trong lúc đo, cảm biến NACK mọi giao dịch; đo xong thì "will acknowledge the reception of the read header and responds with data". Thời gian đo tối đa (Table 8): đo 50 ms, self-test 320 ms, serial 1 ms. Chip của ta NACK read header tới 2000–3000 ms — gấp 40–60 lần trần — tức theo đúng hợp đồng của Sensirion, **phép đo không bao giờ hoàn tất**. Đây là bằng chứng datasheet-level cho kết luận hỏng phần đo.
- **Nghi phạm phần cứng cụ thể:** chip có chân **VDDH (pin 5) cấp nguồn riêng cho hotplate**, phải nối cùng nguồn với VDD (1.7–3.6 V). Footnote 10: sau mỗi lệnh heater, VDDH ăn **xung dòng +20% trong 5 ms**. Đường VDDH trên module đứt/hở hàn → đúng y triệu chứng: phần số sống, phần đo im. Mạch chuẩn (Fig. 6): VDD qua RC 4.7 Ω + 1 µF, VDDH tụ 1 µF riêng.
- **Bảo hành:** Sensirion bảo hành chip 12 tháng, yêu cầu báo lỗi trong 14 ngày kể từ khi phát hiện — thêm cơ sở để khiếu nại nơi bán.

Hai chỗ code đã chỉnh theo datasheet:

1. **Conditioning rút xuống 9 s.** Mục 3.1 đặt giới hạn cứng: quá 10 s là hỏng lớp vật liệu cảm biến. Vòng cũ chạy sát trần ~10,5 s — với chip mới là tự tay phá nó.
2. **Đọc self-test đúng chuẩn Table 15:** bỏ qua byte cao, xét bit 0 (pixel VOC) và bit 1 (pixel NOx) — giờ báo được đích danh pixel nào hỏng thay vì so cứng `0xD400`.

Chỉ còn lại: **module SGP41 hỏng phần hotplate.** Phần số hoàn hảo, phần đo chết.

Trước khi thay module, đo đồng hồ chân VDD của nó một lần: **SGP41 chịu tối đa 3,6 V.** Nếu module trần bị cấp thẳng 5 V thì hỏng vĩnh viễn đúng kiểu này — phần I2C còn sống mà hotplate thì chết.

Sửa dây xong gõ `reinit` là khởi tạo lại, không cần nạp lại firmware. Gõ `raw` để xem lại bảng chẩn đoán.

## Còn thiếu / giai đoạn sau

- **SCD41 và SPS30 chưa về hàng** — code đã sẵn sàng, chỉ chờ cắm vào để test thật.
- **SGP41 mới trả về ticks thô.** Muốn ra VOC Index / NOx Index thang 0–500 thì cần thêm Sensirion Gas Index Algorithm.
- **IR mới có lớp phát thô** (sóng mang + `sendRaw` + NEC để test phần cứng). Điều hòa không dùng mã kiểu điều khiển TV mà gửi nguyên khung trạng thái 60–280 bit tùy hãng, nên khi làm thật nên thêm:
  ```
  lib_deps = crankyoldgit/IRremoteESP8266
  ```
  và dùng class AC tương ứng với hãng điều hòa. `sendRaw` vẫn dùng được để phát lại chuỗi đã bắt từ điều khiển gốc.
- **WiFi / MQTT / server** — chưa động tới, đúng như phạm vi giai đoạn này.
- **Hiệu chuẩn:** SCD41 cần ASC hoặc hiệu chuẩn thủ công ngoài trời (~420 ppm); SPS30 nên chạy `clean` khoảng 1 tuần/lần.
