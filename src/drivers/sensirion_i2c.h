#pragma once

#include <Arduino.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// Lớp giao tiếp chung cho cảm biến Sensirion (SHT31, SCD41, SGP41, SPS30).
// Cả 4 con đều dùng cùng một khung: lệnh 16-bit MSB trước, dữ liệu chia thành
// các word 16-bit, mỗi word kèm 1 byte CRC8 (poly 0x31, init 0xFF).
// ---------------------------------------------------------------------------

namespace sensirion {

uint8_t crc8(const uint8_t *data, size_t len);

// Gửi lệnh 16-bit không tham số.
bool sendCommand(TwoWire &bus, uint8_t addr, uint16_t cmd);

// Gửi lệnh 16-bit kèm tham số; CRC của từng word được chèn tự động.
bool sendCommandWithArgs(TwoWire &bus, uint8_t addr, uint16_t cmd,
                         const uint16_t *args, size_t nArgs);

// Đọc nWords word (mỗi word 3 byte trên bus: MSB, LSB, CRC).
// Trả về false nếu thiếu byte hoặc sai CRC.
bool readWords(TwoWire &bus, uint8_t addr, uint16_t *words, size_t nWords);

// Như readWords nhưng thử lại khi cảm biến còn đang đo (lúc đó nó NACK địa chỉ).
// Thời gian đo ghi trong datasheet là giá trị điển hình, chip thật có thể chậm
// hơn vài ms. usedAttempts (nếu truyền vào) trả về số lần đã thử.
bool readWordsRetry(TwoWire &bus, uint8_t addr, uint16_t *words, size_t nWords,
                    uint8_t attempts, uint32_t retryDelayMs,
                    uint8_t *usedAttempts = nullptr);

// Ghép 2 word big-endian thành float IEEE-754 (SPS30 dùng kiểu này).
float wordsToFloat(uint16_t hi, uint16_t lo);

// General call reset: ghi 0x06 vào địa chỉ 0x00, đưa MỌI cảm biến Sensirion
// trên bus về trạng thái như vừa cấp nguồn. Dùng để gỡ khi chip treo giữa chừng.
bool generalCallReset(TwoWire &bus);

}  // namespace sensirion
