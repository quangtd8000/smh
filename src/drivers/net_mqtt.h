#pragma once

#include "config.h"

#if ENABLE_WIFI

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Quản lý kết nối Wi-Fi & EMQX MQTT Broker (Port 1883 TCP).
// Tự động kết nối lại không gây lock hệ thống, hỗ trợ Last Will & Testament (LWT)
// và nhận lệnh điều khiển từ EMQX.
// ---------------------------------------------------------------------------

class NetMqtt {
 public:
  typedef void (*CommandCallback)(const char *topic, const char *payload);

  NetMqtt();

  void begin(const char *ssid, const char *password, const char *broker,
             uint16_t port = 1883, const char *user = "", const char *pass = "",
             CommandCallback cmdCb = nullptr);

  // Gọi liên tục trong loop() để duy trì kết nối không nghẽn
  void loop();

  bool isConnected();

  // Gửi số đo cảm biến (JSON payload) lên EMQX
  bool publishTelemetry(const char *jsonPayload);

  // Gửi trạng thái thiết bị lên EMQX
  bool publishStatus(const char *statusMsg);

  const char *clientId() const { return _clientId; }

 private:
  void connectWiFi();
  bool connectMqtt();
  static void internalCallback(char *topic, byte *payload, unsigned int length);

  const char *_ssid = nullptr;
  const char *_password = nullptr;
  const char *_broker = nullptr;
  uint16_t _port = 1883;
  const char *_user = nullptr;
  const char *_pass = nullptr;
  CommandCallback _cmdCb = nullptr;

  WiFiClient _wifiClient;
  PubSubClient _mqttClient;

  char _clientId[48];
  char _telemetryTopic[64];
  char _cmdTopic[64];
  char _statusTopic[64];

  uint32_t _lastReconnectAttempt = 0;
  uint32_t _lastWiFiCheck = 0;
};

#endif  // ENABLE_WIFI
