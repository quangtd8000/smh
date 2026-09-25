#pragma once

#include "config.h"

#if ENABLE_WIFI

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#if ENABLE_MQTT_TLS
#include <WiFiClientSecure.h>
#include "ca_cert.h"
#endif

// ---------------------------------------------------------------------------
// Quản lý kết nối Wi-Fi & EMQX MQTT Broker (Port 8883 MQTTS / 1883 TCP).
// Tự động kết nối lại không gây lock hệ thống, hỗ trợ Last Will & Testament (LWT)
// và nhận lệnh điều khiển từ EMQX.
// ---------------------------------------------------------------------------

class NetMqtt {
 public:
  typedef void (*CommandCallback)(const char *topic, const char *payload);
  typedef void (*StatusCallback)(const char *status, const char *ip);

  NetMqtt();

  // Khởi tạo Device UID và các MQTT Topics (có thể gọi trước begin)
  void initDevice();

  void begin(const char *ssid, const char *password, const char *broker,
             uint16_t port = MQTT_PORT, const char *user = "", const char *pass = "",
             CommandCallback cmdCb = nullptr);

  // Lắng nghe sự kiện kết nối mạng ("wifi_connected", "connected", "error_wifi_connect", v.v.)
  void setStatusCallback(StatusCallback cb) { _statusCb = cb; }

  // Gọi liên tục trong loop() để duy trì kết nối không nghẽn
  void loop();

  bool isConnected();
  bool isStarted() const { return _started; }

  // Gửi số đo cảm biến (JSON payload) lên EMQX
  bool publishTelemetry(const char *jsonPayload);

  // Gửi trạng thái thiết bị & relay (JSON payload) lên EMQX
  bool publishState(const char *jsonPayload);

  // Gửi trạng thái kết nối thiết bị lên EMQX (online/offline)
  bool publishStatus(const char *statusMsg);

  const char *clientId() const { return _clientId; }
  const char *deviceUid() const { return _deviceUid; }

 private:
  void connectWiFi();
  bool connectMqtt();
  static void internalCallback(char *topic, byte *payload, unsigned int length);

  String _ssid;
  String _password;
  String _broker;
  uint16_t _port = MQTT_PORT;
  String _user;
  String _pass;
  CommandCallback _cmdCb = nullptr;
  StatusCallback _statusCb = nullptr;
  uint32_t _wifiConnectStartTime = 0;
  bool _wifiConnectReported = false;
  bool _printedIp = false;
  bool _started = false;
  bool _deviceInitialized = false;

#if ENABLE_MQTT_TLS
  WiFiClientSecure _wifiClient;
#else
  WiFiClient _wifiClient;
#endif
  PubSubClient _mqttClient;

  char _clientId[48];
  char _deviceUid[48];

  // Các topic chuẩn Smart Home — chỉ dùng dạng smart_home/<uid>/...
  char _telemetryTopic[64];
  char _cmdTopic[64];
  char _stateTopic[64];
  char _statusTopic[64];

  uint32_t _lastReconnectAttempt = 0;
  uint32_t _lastWiFiCheck = 0;
  uint32_t _lastConnectedTime = 0;
};

#endif  // ENABLE_WIFI
