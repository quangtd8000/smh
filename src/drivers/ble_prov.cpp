#include "drivers/ble_prov.h"
#include <ArduinoJson.h>

static BleProv *s_provInstance = nullptr;

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0 && s_provInstance) {
      Serial.printf("[BLE] Nhan cau hinh tu Web (%u bytes): %s\n",
                    (unsigned)rxValue.length(), rxValue.c_str());

      DynamicJsonDocument doc(512);
      DeserializationError err = deserializeJson(doc, rxValue);
      if (!err) {
        const char *ssid = doc["ssid"] | "";
        const char *pass = doc["pass"] | "";
        const char *broker = doc["broker"] | "192.168.1.35";
        const char *user = doc["user"] | "inno";
        const char *mqttPass = doc["pass_mqtt"] | "inno123";
        const char *claimToken = doc["claim_token"] | "";

        if (strlen(ssid) > 0) {
          // Lưu vào NVS
          s_provInstance->saveConfig(ssid, pass, broker, user, mqttPass, claimToken);
          s_provInstance->notifyStatus("saving", "", "");

          if (s_provInstance->_configCb) {
            s_provInstance->_configCb(ssid, pass, broker, user, mqttPass, claimToken);
          }
        } else {
          s_provInstance->notifyStatus("error_invalid_ssid", "", "");
        }
      } else {
        Serial.printf("[BLE] Loi parse JSON cau hinh: %s\n", err.c_str());
        s_provInstance->notifyStatus("error_json", "", "");
      }
    }
  }
};

class BleServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    Serial.println(F("[BLE] Web/App da ket noi vao ESP32 qua BLE GATT!"));
  }

  void onDisconnect(NimBLEServer* pServer) {
    Serial.println(F("[BLE] Mat ket noi BLE -> Tiep tuc quang ba (Advertising)..."));
    NimBLEDevice::startAdvertising();
  }
};

BleProv::BleProv() {
  s_provInstance = this;
}

bool BleProv::loadSavedConfig(String &ssid, String &pass, String &broker, String &user,
                              String &mqttPass, String &claimToken) {
  _prefs.begin("smarthome", true);
  ssid = _prefs.getString("wifi_ssid", "");
  pass = _prefs.getString("wifi_pass", "");
  broker = _prefs.getString("mqtt_broker", "");
  user = _prefs.getString("mqtt_user", "");
  mqttPass = _prefs.getString("mqtt_pass", "");
  claimToken = _prefs.getString("claim_token", "");
  _prefs.end();

  return (ssid.length() > 0);
}

bool BleProv::saveConfig(const char *ssid, const char *pass, const char *broker,
                        const char *user, const char *mqttPass, const char *claimToken) {
  _prefs.begin("smarthome", false);
  _prefs.putString("wifi_ssid", ssid);
  _prefs.putString("wifi_pass", pass);
  if (broker && strlen(broker) > 0) _prefs.putString("mqtt_broker", broker);
  if (user && strlen(user) > 0) _prefs.putString("mqtt_user", user);
  if (mqttPass && strlen(mqttPass) > 0) _prefs.putString("mqtt_pass", mqttPass);
  if (claimToken && strlen(claimToken) > 0) _prefs.putString("claim_token", claimToken);
  _prefs.end();

  Serial.printf("[BLE] Da luu cau hinh Wi-Fi '%s' vao Flash NVS!\n", ssid);
  return true;
}

void BleProv::clearConfig() {
  _prefs.begin("smarthome", false);
  _prefs.clear();
  _prefs.end();
  Serial.println(F("[BLE] Da XOA sach cau hinh Wi-Fi trong NVS Flash!"));
}

void BleProv::startBLE(const char *deviceName, ConfigReceivedCallback cb) {
  if (_bleActive) return;

  _configCb = cb;
  Serial.printf("[BLE] Khoi dong BLE Server: '%s'\n", deviceName);

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Công suất phát tối đa
  NimBLEDevice::setMTU(512);

  _pServer = NimBLEDevice::createServer();
  _pServer->setCallbacks(new BleServerCallbacks());
  NimBLEService *pService = _pServer->createService(SERVICE_UUID);

  // Characteristic ghi cấu hình (Web -> ESP32)
  _pConfigChar = pService->createCharacteristic(
      CHAR_CONFIG_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  _pConfigChar->setCallbacks(new ConfigCallbacks());

  // Characteristic đọc / nhận thông báo trạng thái (ESP32 -> Web)
  _pStatusChar = pService->createCharacteristic(
      CHAR_STATUS_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();

  // Đặt Tên thiết bị vào gói quảng bá chính (Primary Advertisement Packet)
  // để mọi máy quét đều thấy ngay lập tức mà không bị cắt bỏ do quá 31 bytes.
  NimBLEAdvertisementData advData;
  advData.setFlags(0x06); // General Discoverable Mode, BR/EDR Not Supported
  advData.setName(deviceName);

  // Đặt Service UUID vào gói Scan Response
  NimBLEAdvertisementData scanData;
  scanData.setCompleteServices(NimBLEUUID(SERVICE_UUID));

  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanData);

  bool ok = pAdvertising->start();
  if (ok) {
    _bleActive = true;
    Serial.printf("[BLE] Dang quang ba (Advertising) voi ten '%s'... San sang cho Web/App ghep noi!\n", deviceName);
  } else {
    Serial.println(F("[BLE] LOI: Khong the bat quang ba Bluetooth!"));
  }
}

void BleProv::stopBLE() {
  if (!_bleActive) return;
  Serial.println(F("[BLE] Tat BLE Server sau khi da ket noi Wi-Fi thanh cong."));
  NimBLEDevice::deinit(true);
  _bleActive = false;
  _pServer = nullptr;
  _pConfigChar = nullptr;
  _pStatusChar = nullptr;
}

void BleProv::notifyStatus(const char *status, const char *ip, const char *uid) {
  if (!_bleActive || !_pStatusChar) return;

  StaticJsonDocument<256> doc;
  doc["status"] = status;
  if (ip && strlen(ip) > 0) doc["ip"] = ip;
  if (uid && strlen(uid) > 0) doc["uid"] = uid;

  char buf[256];
  serializeJson(doc, buf);

  _pStatusChar->setValue((uint8_t*)buf, strlen(buf));
  _pStatusChar->notify();
  Serial.printf("[BLE Notify] -> %s\n", buf);
}
