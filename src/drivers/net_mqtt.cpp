#include "drivers/net_mqtt.h"

#if ENABLE_WIFI

#include <ArduinoJson.h>

static NetMqtt *s_instance = nullptr;

NetMqtt::NetMqtt() : _mqttClient(_wifiClient) {
  s_instance = this;
}

void NetMqtt::internalCallback(char *topic, byte *payload, unsigned int length) {
  if (!s_instance || !s_instance->_cmdCb) return;

  // Tạo string an toàn null-terminated cho payload
  char msg[256];
  const size_t len = (length < sizeof(msg) - 1) ? length : (sizeof(msg) - 1);
  memcpy(msg, payload, len);
  msg[len] = '\0';

  s_instance->_cmdCb(topic, msg);
}

void NetMqtt::begin(const char *ssid, const char *password, const char *broker,
                    uint16_t port, const char *user, const char *pass,
                    CommandCallback cmdCb) {
  _ssid = ssid;
  _password = password;
  _broker = broker;
  _port = port;
  _user = user;
  _pass = pass;
  _cmdCb = cmdCb;

  // Tạo Client ID duy nhất bằng MAC address của ESP32
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(_clientId, sizeof(_clientId), "smarthome-node%d-%02X%02X%02X",
           NODE_ID, mac[3], mac[4], mac[5]);

  // Cấu hình Topic
  snprintf(_telemetryTopic, sizeof(_telemetryTopic), "smarthome/node%d/telemetry", NODE_ID);
  snprintf(_cmdTopic, sizeof(_cmdTopic), "smarthome/node%d/cmd", NODE_ID);
  snprintf(_statusTopic, sizeof(_statusTopic), "smarthome/node%d/status", NODE_ID);

  // Cấu hình socket timeout 5000ms (5s) để đủ thời gian TCP handshake qua Wi-Fi
  _wifiClient.setTimeout(5000);
  _mqttClient.setBufferSize(512);

  IPAddress brokerIp;
  if (brokerIp.fromString(_broker)) {
    _mqttClient.setServer(brokerIp, _port);
  } else {
    _mqttClient.setServer(_broker, _port);
  }
  _mqttClient.setCallback(NetMqtt::internalCallback);

  Serial.printf("[NET] Khoi tao WiFi: SSID='%s', EMQX Broker='%s:%u'\n", _ssid,
                _broker, _port);
  Serial.printf("[NET] Client ID: '%s'\n", _clientId);

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);
}

bool NetMqtt::connectMqtt() {
  if (_mqttClient.connected()) return true;

  Serial.printf("[NET] Dang ket noi EMQX Broker (%s:%u)... ", _broker, _port);

  bool ok = false;
  if (_user && strlen(_user) > 0 && _pass && strlen(_pass) > 0) {
    ok = _mqttClient.connect(_clientId, _user, _pass);
  } else {
    ok = _mqttClient.connect(_clientId);
  }

  if (ok) {
    Serial.println(F("THANH CONG!"));
    // Báo trạng thái online lên EMQX (retained)
    _mqttClient.publish(_statusTopic, "online", true);
    // Subscribe topic nhận lệnh điều khiển
    _mqttClient.subscribe(_cmdTopic);
    Serial.printf("[NET] Da subscribe topic lenh: '%s'\n", _cmdTopic);
    Serial.printf("[NET] Topic telemetry: '%s'\n", _telemetryTopic);
  } else {
    Serial.printf("THAT BAI (rc=%d)\n", _mqttClient.state());
  }

  return ok;
}

void NetMqtt::loop() {
  const uint32_t now = millis();

  // Giới hạn tần suất kiểm tra WiFi.status() mỗi 500ms để không làm nghẽn wifi_task trên Core 0
  static uint32_t lastWiFiPoll = 0;
  if (now - lastWiFiPoll < 500) {
    if (_mqttClient.connected()) {
      _mqttClient.loop();
    }
    return;
  }
  lastWiFiPoll = now;

  // Nếu chưa có Wi-Fi, chỉ reconnect sau 20s nếu mất kết nối lâu
  if (WiFi.status() != WL_CONNECTED) {
    if (_lastWiFiCheck == 0) _lastWiFiCheck = now;
    if (now - _lastWiFiCheck > 20000) {
      _lastWiFiCheck = now;
      Serial.println(F("[NET] Thu ket noi lai Wi-Fi..."));
      WiFi.disconnect();
      WiFi.begin(_ssid, _password);
    }
    return;
  }

  // Khi đã có Wi-Fi -> in IP local một lần
  static bool printedIp = false;
  if (!printedIp) {
    printedIp = true;
    Serial.printf("[NET] Wi-Fi DA KET NOI! IP ESP32 trong LAN: %s\n",
                  WiFi.localIP().toString().c_str());
  }

  // Nếu đã có Wi-Fi mà chưa kết nối MQTT -> Thử kết nối lại EMQX mỗi 5s
  if (!_mqttClient.connected()) {
    if (now - _lastReconnectAttempt > 5000) {
      _lastReconnectAttempt = now;
      connectMqtt();
    }
  } else {
    _mqttClient.loop();
  }
}

bool NetMqtt::isConnected() {
  return (WiFi.status() == WL_CONNECTED) && _mqttClient.connected();
}

bool NetMqtt::publishTelemetry(const char *jsonPayload) {
  if (!isConnected()) return false;
  return _mqttClient.publish(_telemetryTopic, jsonPayload);
}

bool NetMqtt::publishStatus(const char *statusMsg) {
  if (!isConnected()) return false;
  return _mqttClient.publish(_statusTopic, statusMsg, true);
}

#endif  // ENABLE_WIFI
