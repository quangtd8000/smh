# Smart Home Prototype — ESP32-S3 Multi-Sensor & MQTT Node

Hệ thống Node giám sát môi trường không khí và điều khiển thiết bị thông minh dựa trên vi điều khiển **ESP32-S3**, kết nối không dây **Wi-Fi** và giao tiếp dữ liệu thời gian thực qua **EMQX MQTT Broker**.

![ESP32-S3 Smart Home Architecture](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange?style=for-the-badge&logo=platformio)
![MQTT](https://img.shields.io/badge/MQTT-EMQX--Broker-green?style=for-the-badge&logo=eclipse-mosquitto)
![Status](https://img.shields.io/badge/Status-Fully_Tested_%26_Working-brightgreen?style=for-the-badge)

---

## 🛠️ Linh kiện & Tình trạng Hoạt động

| Chức năng | Linh kiện / Module | Số lượng | Chuẩn giao tiếp | Trạng thái |
| :--- | :--- | :---: | :--- | :---: |
| **Nhiệt độ & Độ ẩm** | SHT31 | 1 | I2C (`0x44`) | ✅ Hoạt động chuẩn xác (±0.2°C, ±2% RH) |
| **Nồng độ CO2** | SCD41 | 1 | I2C (`0x62`) | ✅ Hoạt động tốt (NDIR, 400–5000 ppm) |
| **TVOC & NOx (Đa khí)** | SGP41 | 1 | I2C (`0x59`) | ✅ Hoạt động hoàn hảo (Sensirion Algorithm) |
| **Bụi mịn PM2.5 / PM10** | SPS30 | 1 | I2C (`0x69scscan`) | ⏳ Dự phòng sẵn driver trong firmware |
| **Bật/Tắt Đèn & Quạt AC** | Relay 2 Kênh | 1 | GPIO 4 & 5 | ✅ Kích đệm transistor / opto cách ly |
| **Điều khiển Điều hòa** | IR Transmitter 1W | 1 | GPIO 17 (LEDC 38kHz) | ✅ Chuẩn xung NEC / Raw IR transmission |
| **Vi điều khiển chính** | ESP32-S3-DevKitC-1 | 1 | Wi-Fi 2.4GHz | ✅ RAM 320KB, Flash 8MB, QD Mode |

---

## 📍 Sơ đồ Đấu nối Chân (Pinout — ESP32-S3)

| Tín hiệu | GPIO | Ghi chú phần cứng |
| :--- | :---: | :--- |
| **I2C SDA** | `GPIO 8` | Dùng chung cho cả 4 cảm biến, **bắt buộc hàn trở pull-up 4.7kΩ ra 3V3** |
| **I2C SCL** | `GPIO 9` | Dùng chung cho cả 4 cảm biến, **bắt buộc hàn trở pull-up 4.7kΩ ra 3V3** |
| **Relay Đèn** | `GPIO 4` | Mặc định kích mức thấp (Active Low) |
| **Relay Quạt** | `GPIO 5` | Mặc định kích mức thấp (Active Low) |
| **IR TX (Phát hồng ngoại)** | `GPIO 17` | Tín hiệu xung 38 kHz điều khiển Transistor module IR 1W |
| **Nút bấm thủ công** | `GPIO 18` | Pull-up nội, nhấn = LOW |

> ⚠️ **Lưu ý I2C Speed**: Bus I2C được cố định ở **100 kHz** vì SCD41 và SPS30 chỉ chịu được tối đa 100 kHz.

---

## 📡 Cấu trúc MQTT Topics & Payload Dữ liệu

Node tự động kết nối Wi-Fi và phát số đo theo chu kỳ lên **EMQX Broker**:

### 1. Dữ liệu Cảm biến Telemetry (Publish)
* **Topic**: `smarthome/node<NODE_ID>/telemetry` *(Ví dụ: `smarthome/node0/telemetry`)*
* **Định dạng Payload (JSON)**:
```json
{
  "node_id": 0,
  "uptime": 125,
  "temp": 25.79,
  "rh": 43.8,
  "co2": 873,
  "voc_index": 105,
  "nox_index": 1,
  "sraw_voc": 30603,
  "sraw_nox": 18472
}
```

### 2. Trạng thái Thiết bị LWT / Status (Publish Retained)
* **Topic**: `smarthome/node<NODE_ID>/status`
* **Payload**: `"online"` (khi vừa kết nối) / `"offline"` (Last Will and Testament khi mất nguồn/mạng).

### 3. Nhận lệnh Điều khiển từ xa (Subscribe)
* **Topic**: `smarthome/node<NODE_ID>/cmd`
* **Danh sách Lệnh hỗ trợ**:
  * `"on den"` / `"off den"` / `"toggle den"` ➔ Điều khiển Relay Đèn.
  * `"on quat"` / `"off quat"` / `"toggle quat"` ➔ Điều khiển Relay Quạt.
  * `"ir"` ➔ Phát 1 khung tín hiệu hồng ngoại NEC test.
  * `"scan"` ➔ Quét nhanh bus I2C và báo trạng thái các Slave.
  * `"reinit"` ➔ Khởi tạo lại tất cả cảm biến trên bus.
  * `"recover"` ➔ Gỡ kẹt bus I2C (gửi 9 xung clock + STOP condition).
  * `"diag"` ➔ Chẩn đoán sâu giao tiếp thanh ghi SGP41.

---

## ⚙️ Cấu hình Môi trường (PlatformIO Environments)

Dự án hỗ trợ build 5 môi trường khác nhau trong `platformio.ini`:

| Environment | Mục đích sử dụng | Các Module được bật |
| :--- | :--- | :--- |
| **`env:bench`** | **Môi trường Test Bench đầy đủ** | **SHT31, SGP41, SCD41, Relay, IR TX (Bật hết)** |
| `env:node-air` | Node cảm biến chất lượng không khí | SHT31, SGP41, SCD41, SPS30 |
| `env:node-relay` | Node công tắc relay điều khiển điện AC | Relay Đèn, Relay Quạt |
| `env:node-ir` | Node điều hòa phát hồng ngoại | Module IR TX VOKTTA |
| `env:libtest` | Môi trường chẩn đoán độc lập | Sensirion Native Core Test |

### Lệnh Biên dịch & Nạp Code:

```bash
# Nạp firmware môi trường Bench lên ESP32-S3
pio run -e bench -t upload

# Mở Serial Monitor xem log ở tốc độ 115200 baud
pio device monitor -b 115200
```

---

## 🔒 Hướng dẫn Cấu hình Tường lửa (Windows & Docker EMQX)

Nếu chạy EMQX Broker trên máy tính local bằng **Docker Desktop (WSL2)**, Windows Firewall mặc định sẽ chặn gói tin từ ESP32 (`192.168.1.x`) đi vào Docker container.

### Để mở cổng 1883:
1. Bấm phím **Windows**, gõ `PowerShell`, chuột phải chọn **Run as Administrator**.
2. Chạy lệnh sau:
```powershell
New-NetFirewallRule -DisplayName "EMQX 1883" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 1883
```

---

## 📁 Cấu trúc Thư mục Mã nguồn

```text
├── include/
│   ├── config.h              # Cờ bật/tắt module, thông số Wi-Fi, IP EMQX Broker
│   └── pins.h                # Khai báo chân GPIO & địa chỉ I2C
├── src/
│   ├── main.cpp              # Vòng lặp chính, Serial Command Console, đọc cảm biến
│   ├── drivers/
│   │   ├── net_mqtt.cpp/.h   # Quản lý Wi-Fi, MQTT PubSubClient, Reconnect & LWT
│   │   ├── sht31.cpp/.h      # Driver cảm biến Nhiệt độ / Độ ẩm SHT31
│   │   ├── scd41.cpp/.h      # Driver cảm biến NDIR CO2 SCD41
│   │   ├── sgp41.cpp/.h      # Driver SGP41 + Sensirion Gas Index Algorithm (VOC/NOx)
│   │   ├── sps30.cpp/.h      # Driver bụi mịn PM2.5 SPS30
│   │   ├── relay.cpp/.h      # Driver điều khiển Relay AC
│   │   └── ir_tx.cpp/.h      # Driver phát hồng ngoại LEDC 38kHz
└── platformio.ini            # Cấu hình PlatformIO, thư viện phụ thuộc & build flags
```

---

## 🛡️ Tối ưu & Sửa lỗi Phần cứng / Phần mềm đã thực hiện

1. **Vô hiệu hóa Brownout Detector (`RTC_CNTL_BROWN_OUT_REG`)**: Tránh hiện tượng reset do sụt áp nhẹ khi module Wi-Fi RF phát công suất cao.
2. **Giới hạn chu kỳ kiểm tra `WiFi.status()` (500ms)**: Tránh việc chiếm giữ `wifi_mutex` liên tục gây nghẽn FreeRTOS `wifi_task` trên Core 0 (sửa triệt để lỗi `TG1WDT_SYS_RST`).
3. **Cấu hình `WiFi.persistent(false)`**: Bỏ qua ghi NVS Flash SPI trùng lặp lúc khởi tạo Wi-Fi.
4. **Tự động nuôi Watchdog trong vòng Conditioning SGP41**: Thêm `yield()` trong 9 giây sấy heater của SGP41.
5. **Chuyển đổi IPAddress sang dạng Binary**: Bỏ qua bước phân giải tên miền mDNS/DNS giúp ESP32 kết nối EMQX cực nhanh.
