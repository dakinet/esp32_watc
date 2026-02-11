#include "cloud_client.h"

void CloudClient::begin(const String& host, uint16_t port, const String& deviceToken) {
    _deviceToken = deviceToken;

    String path = "/ws?token=" + deviceToken + "&role=device";
    _ws.begin(host.c_str(), port, path.c_str());
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        _handleEvent(type, payload, length);
    });
    _ws.setReconnectInterval(5000);
}

void CloudClient::loop() {
    _ws.loop();
}

void CloudClient::sendStatus(int currentScreen, int totalScreens) {
    if (!_connected) return;

    JsonDocument doc;
    doc["type"] = "status";
    doc["screen"] = currentScreen;
    doc["total"] = totalScreens;
    doc["online"] = true;

    String json;
    serializeJson(doc, json);
    _ws.sendTXT(json);
}

void CloudClient::sendGesture(const String& gesture) {
    if (!_connected) return;

    JsonDocument doc;
    doc["type"] = "touch";
    doc["gesture"] = gesture;

    String json;
    serializeJson(doc, json);
    _ws.sendTXT(json);
}

void CloudClient::_handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Disconnected");
            _connected = false;
            if (_connectCb) _connectCb(false);
            break;

        case WStype_CONNECTED:
            Serial.printf("[WS] Connected to %s\n", (char*)payload);
            _connected = true;
            if (_connectCb) _connectCb(true);
            break;

        case WStype_TEXT: {
            Serial.printf("[WS] Received: %s\n", (char*)payload);

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload, length);
            if (err) {
                Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
                break;
            }

            String msgType = doc["type"].as<String>();

            if (msgType == "screens" && _screensCb) {
                JsonArray data = doc["data"].as<JsonArray>();
                _screensCb(data);
            } else if (msgType == "settings" && _settingsCb) {
                int brightness = doc["brightness"] | 255;
                int textSize = doc["textSize"] | 1;
                _settingsCb(brightness, textSize);
            }
            break;
        }

        default:
            break;
    }
}
