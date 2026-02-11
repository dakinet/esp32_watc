#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>
#include <vector>
#include "touch_driver.h"

enum WiFiManagerState {
    WM_SCANNING,
    WM_SHOW_NETWORKS,
    WM_KEYBOARD,
    WM_CONNECTING,
    WM_CONNECTED,
    WM_FAILED
};

struct WiFiNetwork {
    String ssid;
    int32_t rssi;
    bool secured;
};

class WiFiManager {
public:
    void begin(LGFX_Device* display, TouchDriver* touch);
    bool loop();  // Returns true when connected
    bool autoConnect();  // Try saved credentials
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    String getSSID() const { return _selectedSSID; }
    String getIP() const { return WiFi.localIP().toString(); }

private:
    LGFX_Device* _tft = nullptr;
    TouchDriver* _touch = nullptr;
    Preferences _prefs;

    WiFiManagerState _state = WM_SCANNING;
    std::vector<WiFiNetwork> _networks;
    int _scrollOffset = 0;
    String _selectedSSID;
    String _password;
    int _kbRow = 0;
    int _kbShift = 0;
    int _kbSymbol = 0;
    unsigned long _connectStart = 0;

    void _scan();
    void _drawNetworkList();
    void _drawKeyboard();
    void _drawConnecting();
    void _drawConnected();
    void _drawFailed();
    void _handleNetworkSelect();
    void _handleKeyPress();
    void _saveCredentials();
    bool _loadCredentials();
    int _rssiToPercent(int32_t rssi);
    char _getKeyAt(int px, int py);
    void _drawMagnifier(int px, int py, char ch);
    void _eraseMagnifier();
    bool _magnifierShown = false;
};
