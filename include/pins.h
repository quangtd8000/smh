#pragma once

// ---------------------------------------------------------------------------
// Sơ đồ chân - ESP32-S3-DevKitC-1 (N16R8)
//
// TRÁNH dùng: GPIO 0/3/45/46 (strapping), 19/20 (USB D-/D+),
//             26..32 (SPI flash), 33..37 (Octal PSRAM trên bản R8),
//             43/44 (UART0 - cổng debug).
// Các chân dưới đây đều nằm ngoài danh sách trên.
// ---------------------------------------------------------------------------

// --- I2C dùng chung cho SHT31 / SCD41 / SGP41 / SPS30 ---
// Cần điện trở kéo lên 4.7k ra 3V3 (pull-up nội của ESP32 quá yếu cho 4 slave).
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

// --- Relay AC (3 kênh) ---
// Module 3 kênh: Đèn (Kênh 1), Quạt (Kênh 2), Công tắc 3 (Kênh 3)
#define PIN_RELAY_LIGHT 4
#define PIN_RELAY_FAN 5
#define PIN_RELAY_3 6


// --- Phát hồng ngoại (VOKTTA 1W) ---
// Chân này chỉ là tín hiệu điều khiển; LED công suất lấy nguồn riêng 5V.
#define PIN_IR_TX 17

// --- Dự phòng ---
// Nút nhấn tại chỗ, dùng pull-up nội, nhấn = LOW.
#define PIN_BUTTON 18

// ---------------------------------------------------------------------------
// Địa chỉ I2C (7-bit)
// ---------------------------------------------------------------------------
#define ADDR_SHT31 0x44  // 0x45 nếu chân ADDR kéo lên cao
#define ADDR_SCD41 0x62
#define ADDR_SGP41 0x59
#define ADDR_SPS30 0x69
