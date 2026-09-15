#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

// Service & Characteristic UUIDs for Smart Home Provisioning
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_CONFIG_UUID    "12345678-1234-5678-1234-56789abcdef1"
#define CHAR_STATUS_UUID    "12345678-1234-5678-1234-56789abcdef2"

class BleProv {
 public:
  typedef void (*ConfigReceivedCallback)(const char *ssid, const char *pass,
                                         const char *broker, const char *user,
                                         const char *mqttPass,
                                         const char *claimToken);

  BleProv();

  // Khởi tạo Preferences & kiểm tra xem đã có cấu hình Wi-Fi chưa.
  // claimToken: token backend cấp để thiết bị tự đăng ký vào đúng home.
  bool loadSavedConfig(String &ssid, String &pass, String &broker, String &user,
                       String &mqttPass, String &claimToken);

  // Lưu cấu hình mới vào NVS Flash
  bool saveConfig(const char *ssid, const char *pass, const char *broker,
                  const char *user, const char *mqttPass, const char *claimToken);

  // Xoá cấu hình Wi-Fi (Quên mạng / Reset về mặc định)
  void clearConfig();

  // Khởi động BLE Server để chờ Web kết nối cấu hình
  void startBLE(const char *deviceName, ConfigReceivedCallback cb);

  // Tắt BLE Server sau khi đã kết nối Wi-Fi thành công
  void stopBLE();

  // Báo trạng thái kết nối lên Web qua BLE Notify
  void notifyStatus(const char *status, const char *ip = "", const char *uid = "");

  bool isBleActive() const { return _bleActive; }

 private:
  Preferences _prefs;
  NimBLEServer *_pServer = nullptr;
  NimBLECharacteristic *_pConfigChar = nullptr;
  NimBLECharacteristic *_pStatusChar = nullptr;
  bool _bleActive = false;
  ConfigReceivedCallback _configCb = nullptr;

  friend class ConfigCallbacks;
};
