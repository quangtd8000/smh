#pragma once

// ---------------------------------------------------------------------------
// Cờ bật/tắt từng module. Giá trị mặc định ở đây, platformio.ini ghi đè bằng
// -D. Linh kiện chưa về -> để 0, firmware vẫn build và chạy bình thường.
// ---------------------------------------------------------------------------

#ifndef NODE_ID
#define NODE_ID 0
#endif

#ifndef ENABLE_SHT31
#define ENABLE_SHT31 1 // nhiệt độ - độ ẩm
#endif

#ifndef ENABLE_SGP41
#define ENABLE_SGP41 1 // TVOC / NOx (đa khí)
#endif

#ifndef ENABLE_SCD41
#define ENABLE_SCD41 1 // CO2
#endif

#ifndef ENABLE_SPS30
#define ENABLE_SPS30 0 // PM2.5 - CHƯA CÓ HÀNG
#endif

#ifndef ENABLE_RELAY
#define ENABLE_RELAY 0 // đèn / quạt AC
#endif

#ifndef ENABLE_IR
#define ENABLE_IR 0 // phát hồng ngoại điều khiển điều hòa
#endif

// ---------------------------------------------------------------------------
// Tham số chung
// ---------------------------------------------------------------------------

// Mọi hằng số dưới đây đều bọc #ifndef để ghi đè được bằng -D trong
// platformio.ini mà không phải sửa file này.

// SCD41 và SPS30 chỉ chịu tối đa 100 kHz -> cả bus chạy 100 kHz cho an toàn.
#ifndef I2C_FREQ_HZ
#define I2C_FREQ_HZ 100000UL
#endif

// Chu kỳ đọc cảm biến (ms). Để 1 s vì thuật toán Gas Index của SGP41 được
// Sensirion kiểm chứng ở 1 s (mặc định) và 10 s, phần NOx luôn giả định 1 s.
// SCD41 chỉ ra số liệu mới mỗi 5 s và SPS30 mỗi 1 s, đọc nhanh hơn thì chúng
// báo "chưa sẵn sàng" chứ không sao.
#ifndef SAMPLE_PERIOD_MS
#define SAMPLE_PERIOD_MS 1000UL
#endif

// SGP41 phải chạy conditioning trước lần đo đầu tiên. Datasheet mục 3.1 đặt
// giới hạn CỨNG: không được quá 10 s, quá là hỏng lớp vật liệu cảm biến.
// Để 9 s cho có biên an toàn (vòng lặp + lần đo đầu còn tốn thêm vài trăm ms).
// Đặt 0 để bỏ qua (chỉ dùng khi cô lập lỗi, số đo sẽ không chuẩn).
#ifndef SGP41_CONDITIONING_MS
#define SGP41_CONDITIONING_MS 9000UL
#endif

// Module relay rời phần lớn là kích mức thấp. Module kích mức cao -> đổi thành
// 0.
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 1
#endif

// Tần số sóng mang hồng ngoại của hầu hết điều hòa dân dụng.
#ifndef IR_CARRIER_HZ
#define IR_CARRIER_HZ 38000UL
#endif
#ifndef IR_DUTY_PERCENT
#define IR_DUTY_PERCENT 33
#endif

// ---------------------------------------------------------------------------
// Cấu hình Wi-Fi & EMQX MQTT Broker (Port 1883)
// ---------------------------------------------------------------------------

#ifndef ENABLE_WIFI
#define ENABLE_WIFI 1
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "Xuan Bach"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "0936265558"
#endif

// Cấu hình kết nối EMQX Broker (TCP Port 1883 Không Mã Hóa)
#ifndef MQTT_BROKER
#define MQTT_BROKER                                                            \
  "192.168.1.28" // Hoặc IP EMQX Server local (vd: "192.168.1.100")
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883 // Port 1883 chuẩn MQTT TCP
#endif

#ifndef MQTT_USER
#define MQTT_USER "" // Nếu EMQX bật Auth thì điền Username, ngược lại để rỗng
#endif

#ifndef MQTT_PASS
#define MQTT_PASS "" // Nếu EMQX bật Auth thì điền Password, ngược lại để rỗng
#endif
