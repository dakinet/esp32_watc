#pragma once
#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <functional>

using ScreensCallback = std::function<void(JsonArray&)>;
using ConnectCallback = std::function<void(bool)>;
using SettingsCallback = std::function<void(int brightness, int textSize)>;

class CloudClient {
public:
    void begin(const String& host, uint16_t port, const String& deviceToken);
    void loop();
    bool isConnected() const { return _connected; }

    void onScreensReceived(ScreensCallback cb) { _screensCb = cb; }
    void onConnectionChange(ConnectCallback cb) { _connectCb = cb; }
    void onSettingsReceived(SettingsCallback cb) { _settingsCb = cb; }

    void sendStatus(int currentScreen, int totalScreens);
    void sendGesture(const String& gesture);

private:
    WebSocketsClient _ws;
    bool _connected = false;
    String _deviceToken;
    ScreensCallback _screensCb;
    ConnectCallback _connectCb;
    SettingsCallback _settingsCb;

    void _handleEvent(WStype_t type, uint8_t* payload, size_t length);
};
