#include "drivers/net_mqtt.h"

#if ENABLE_WIFI

#include <ArduinoJson.h>

static NetMqtt *s_instance = nullptr;

NetMqtt::NetMqtt() : _mqttClient(_wifiClient) {
  s_instance = this;
  _deviceUid[0] = '\0';
  _clientId[0] = '\0';
  _telemetryTopic[0] = '\0';
  _cmdTopic[0] = '\0';
  _stateTopic[0] = '\0';
  _statusTopic[0] = '\0';
  initDevice();
}

void NetMqtt::initDevice() {
  if (_deviceInitialized) return;
  _deviceInitialized = true;

  // Lấy địa chỉ MAC của ESP32 (dự phòng khi DEVICE_UID rỗng)
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Định danh duy nhất: DEVICE_UID. Nếu rỗng thì suy từ MAC phần cứng.
  if (strlen(DEVICE_UID) > 0) {
    strncpy(_deviceUid, DEVICE_UID, sizeof(_deviceUid) - 1);
    _deviceUid[sizeof(_deviceUid) - 1] = '\0';
  } else {
    snprintf(_deviceUid, sizeof(_deviceUid), "esp32-node-%02X%02X%02X", mac[3], mac[4], mac[5]);
  }

  snprintf(_clientId, sizeof(_clientId), "client-%s", _deviceUid);

  // Mọi topic đều dạng smart_home/<uid>/... — 1 mã duy nhất, khớp DB backend.
  snprintf(_telemetryTopic, sizeof(_telemetryTopic), "smart_home/%s/telemetry", _deviceUid);
  snprintf(_cmdTopic, sizeof(_cmdTopic), "smart_home/%s/command", _deviceUid);
  snprintf(_stateTopic, sizeof(_stateTopic), "smart_home/%s/state", _deviceUid);
  snprintf(_statusTopic, sizeof(_statusTopic), "smart_home/%s/status", _deviceUid);
}

void NetMqtt::internalCallback(char *topic, byte *payload, unsigned int length) {
  if (!s_instance || !s_instance->_cmdCb) return;

  // Payload lệnh IR từ backend dài 213 ký tự (tên ASCII) và tới 267-281 ký tự
  // khi tên thiết bị/lệnh có dấu tiếng Việt (json.dumps escape \uXXXX) — buffer
  // 256 cũ làm JSON bị cắt cụt tại tầng này trước cả callback ở main.cpp.
  char msg[512];
  const size_t len = (length < sizeof(msg) - 1) ? length : (sizeof(msg) - 1);
  if (length >= sizeof(msg)) {
    Serial.printf("[NET] CANH BAO: payload MQTT %u ky tu > %u, bi cat bot!\n",
                  (unsigned)length, (unsigned)sizeof(msg));
  }
  memcpy(msg, payload, len);
  msg[len] = '\0';

  s_instance->_cmdCb(topic, msg);
}

void NetMqtt::begin(const char *ssid, const char *password, const char *broker,
                    uint16_t port, const char *user, const char *pass,
                    CommandCallback cmdCb) {
  initDevice();
  _started = true;

  _ssid = ssid ? ssid : "";
  _password = password ? password : "";
  _broker = broker ? broker : "";
  _port = port;
  _user = user ? user : "";
  _pass = pass ? pass : "";
  _cmdCb = cmdCb;

#if ENABLE_MQTT_TLS
  // Cấu hình chứng chỉ CA cho kết nối MQTTS TLS bảo mật
  _wifiClient.setCACert(EMQX_CA_CERT);
  _wifiClient.setTimeout(5000);
#else
  // Cấu hình socket timeout 5000ms (5s) để đủ thời gian TCP handshake qua Wi-Fi
  _wifiClient.setTimeout(5000);
#endif
  _mqttClient.setBufferSize(2048);

  IPAddress brokerIp;
  if (brokerIp.fromString(_broker)) {
    _mqttClient.setServer(brokerIp, _port);
  } else {
    _mqttClient.setServer(_broker.c_str(), _port);
  }
  _mqttClient.setCallback(NetMqtt::internalCallback);

  Serial.printf("[NET] Khoi tao WiFi: SSID='%s', EMQX Broker='%s:%u' (%s)\n", _ssid.c_str(),
                _broker.c_str(), _port, ENABLE_MQTT_TLS ? "TLS/SSL MQTTS" : "TCP");
  Serial.printf("[NET] Client ID: '%s', Device UID: '%s'\n", _clientId, _deviceUid);

  _wifiConnectStartTime = millis();
  _wifiConnectReported = false;
  _printedIp = false;
  _lastReconnectAttempt = 0;
  _lastWiFiCheck = 0;
  _lastConnectedTime = millis();

  WiFi.disconnect(false);
  delay(50);
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid.c_str(), _password.c_str());
}

bool NetMqtt::connectMqtt() {
  if (_mqttClient.connected()) return true;

  Serial.printf("[NET] Dang ket noi EMQX Broker (%s:%u)... ", _broker.c_str(), _port);

  bool ok = false;
  // Kết nối có kèm Last Will & Testament (LWT) -> Báo offline nếu mất nguồn đột ngột
  if (_user.length() > 0 && _pass.length() > 0) {
    ok = _mqttClient.connect(_clientId, _user.c_str(), _pass.c_str(), _statusTopic, 1, true, "offline");
  } else {
    ok = _mqttClient.connect(_clientId, _statusTopic, 1, true, "offline");
  }

  if (ok) {
    Serial.println(F("THANH CONG!"));
    // Báo trạng thái online lên EMQX (retained)
    _mqttClient.publish(_statusTopic, "online", true);

    // Subscribe topic nhận lệnh điều khiển
    _mqttClient.subscribe(_cmdTopic);

    Serial.printf("[NET] Da subscribe topic: '%s'\n", _cmdTopic);
    Serial.printf("[NET] Topic telemetry: '%s'\n", _telemetryTopic);

    if (_statusCb) {
      _statusCb("connected", WiFi.localIP().toString().c_str());
    }
  } else {
    Serial.printf("THAT BAI (rc=%d)\n", _mqttClient.state());
  }

  return ok;
}

void NetMqtt::loop() {
  if (!_started) return;

  const uint32_t now = millis();

  // Giới hạn tần suất kiểm tra WiFi.status() mỗi 500ms để không làm nghẽn wifi_task trên Core 0
  static uint32_t lastWiFiPoll = 0;
  if (now - lastWiFiPoll < 500) {
    if (_mqttClient.connected()) {
      _lastConnectedTime = now;
      _mqttClient.loop();
    }
    return;
  }
  lastWiFiPoll = now;

  // Network Watchdog: Nếu mất kết nối mạng / MQTT liên tục > 15 phút (900.000 ms),
  // tự động restart ESP32 để xóa sạch trạng thái kẹt của Wi-Fi driver hoặc DHCP
  if (now - _lastConnectedTime > 900000UL) {
    Serial.println(F("\n[NET WDT] Mat ket noi mang / MQTT lien tuc > 15 phut! Tu dong restart ESP32 de tu phuc hoi..."));
    delay(500);
    ESP.restart();
  }

  // Nếu chưa có Wi-Fi, chỉ reconnect sau 20s nếu mất kết nối lâu
  if (WiFi.status() != WL_CONNECTED) {
    if (_printedIp) {
      _printedIp = false;
      if (_statusCb) _statusCb("wifi_disconnected", "");
    }
    // Báo lỗi timeout nếu thử kết nối quá 20s mà chưa bắt được sóng Wi-Fi
    if (!_wifiConnectReported && (now - _wifiConnectStartTime > 20000)) {
      _wifiConnectReported = true;
      if (_statusCb) _statusCb("error_wifi_connect", "");
    }
    if (_lastWiFiCheck == 0) _lastWiFiCheck = now;
    if (now - _lastWiFiCheck > 20000) {
      _lastWiFiCheck = now;
      Serial.println(F("[NET] Thu ket noi lai Wi-Fi..."));
      WiFi.disconnect();
      WiFi.begin(_ssid.c_str(), _password.c_str());
    }
    return;
  }

  // Khi đã có Wi-Fi -> in IP local một lần
  if (!_printedIp) {
    _printedIp = true;
    _wifiConnectReported = true;
    Serial.printf("[NET] Wi-Fi DA KET NOI! IP ESP32 trong LAN: %s\n",
                  WiFi.localIP().toString().c_str());
    if (_statusCb) {
      _statusCb("wifi_connected", WiFi.localIP().toString().c_str());
    }
  }

  // Nếu đã có Wi-Fi mà chưa kết nối MQTT -> Thử kết nối lại EMQX mỗi 5s
  if (!_mqttClient.connected()) {
    if (now - _lastReconnectAttempt > 5000) {
      _lastReconnectAttempt = now;
      connectMqtt();
    }
  } else {
    _lastConnectedTime = now;
    _mqttClient.loop();
  }
}

bool NetMqtt::isConnected() {
  return _started && (WiFi.status() == WL_CONNECTED) && _mqttClient.connected();
}

bool NetMqtt::publishTelemetry(const char *jsonPayload) {
  if (!isConnected()) return false;
  // Chỉ publish 1 topic duy nhất — bản cũ publish 2 topic nên DB ghi trùng 2 lần.
  return _mqttClient.publish(_telemetryTopic, jsonPayload);
}

bool NetMqtt::publishState(const char *jsonPayload) {
  if (!isConnected()) return false;
  return _mqttClient.publish(_stateTopic, jsonPayload, true);
}

bool NetMqtt::publishStatus(const char *statusMsg) {
  if (!isConnected()) return false;
  return _mqttClient.publish(_statusTopic, statusMsg, true);
}

#endif  // ENABLE_WIFI
