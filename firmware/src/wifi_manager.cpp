#include "wifi_manager.h"

// Keyboard layouts
static const char* KB_ROWS_LOWER[] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm.",
};
static const char* KB_ROWS_UPPER[] = {
    "1234567890",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM.",
};
static const char* KB_ROWS_SYMBOL[] = {
    "!\"#$%&'()",
    "@-_=+[]{}",
    "\\|;:*?/~",
    "<>,. ^`",
};

#define KB_NUM_ROWS 4
#define KB_KEY_W 22
#define KB_KEY_H 24
#define KB_START_Y 56

void WiFiManager::begin(LGFX_Device* display, TouchDriver* touch) {
    _tft = display;
    _touch = touch;
    _prefs.begin("wifi", false);
}

bool WiFiManager::autoConnect() {
    if (!_loadCredentials()) return false;

    Serial.printf("\r\n[WiFi] Trying saved: %s\n", _selectedSSID.c_str());
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextColor(TFT_WHITE);
    _tft->setTextSize(1);
    _tft->setCursor(60, 100);
    _tft->printf("Connecting to");
    _tft->setCursor(40, 120);
    _tft->printf("%s...", _selectedSSID.c_str());

    WiFi.begin(_selectedSSID.c_str(), _password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\r\n[WiFi] Connected! IP: %s\n", getIP().c_str());
        return true;
    }

    Serial.println("\r\n[WiFi] Auto-connect failed");
    WiFi.disconnect();
    return false;
}

bool WiFiManager::loop() {
    // touch.update() is called by main loop, don't call again

    switch (_state) {
        case WM_SCANNING:
            _scan();
            _state = WM_SHOW_NETWORKS;
            _drawNetworkList();
            break;

        case WM_SHOW_NETWORKS:
            _handleNetworkSelect();
            break;

        case WM_KEYBOARD:
            _handleKeyPress();
            break;

        case WM_CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                _saveCredentials();
                _state = WM_CONNECTED;
                _drawConnected();
            } else if (millis() - _connectStart > 15000) {
                WiFi.disconnect();
                _state = WM_FAILED;
                _drawFailed();
            }
            break;
        }

        case WM_CONNECTED:
            return true;

        case WM_FAILED: {
            TouchGesture g = _touch->getGesture();
            if (g == GESTURE_SINGLE_TAP || g == GESTURE_DOUBLE_TAP || _touch->justTapped()) {
                _state = WM_SCANNING;
            }
            break;
        }
    }

    return false;
}

void WiFiManager::_scan() {
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextColor(TFT_CYAN);
    _tft->setTextSize(1);
    _tft->setCursor(55, 110);
    _tft->println("Scanning WiFi...");

    int n = WiFi.scanNetworks();
    _networks.clear();
    for (int i = 0; i < n && i < 20; i++) {
        WiFiNetwork net;
        net.ssid = WiFi.SSID(i);
        net.rssi = WiFi.RSSI(i);
        net.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        // Skip duplicates
        bool dup = false;
        for (auto& existing : _networks) {
            if (existing.ssid == net.ssid) { dup = true; break; }
        }
        if (!dup && net.ssid.length() > 0) {
            _networks.push_back(net);
        }
    }
    WiFi.scanDelete();
    _scrollOffset = 0;
    Serial.printf("\r\n[WiFi] Found %d networks\n", _networks.size());
}

void WiFiManager::_drawNetworkList() {
    _tft->fillScreen(TFT_BLACK);

    // Title
    _tft->setTextColor(TFT_CYAN);
    _tft->setTextSize(1);
    _tft->setCursor(55, 20);
    _tft->println("Select WiFi");

    // Draw network list (max 6 visible in circle)
    int maxVisible = 6;
    int y = 45;
    for (int i = _scrollOffset; i < (int)_networks.size() && i < _scrollOffset + maxVisible; i++) {
        bool inView = (y > 30 && y < 210);
        if (!inView) { y += 28; continue; }

        // Background bar
        int barWidth = 180;
        int barX = (240 - barWidth) / 2;
        _tft->fillRoundRect(barX, y, barWidth, 24, 4, 0x1082);

        // Signal strength icon
        int pct = _rssiToPercent(_networks[i].rssi);
        uint16_t sigColor = pct > 60 ? TFT_GREEN : (pct > 30 ? TFT_YELLOW : TFT_RED);
        _tft->fillRect(barX + 4, y + 16, 3, 8, sigColor);
        if (pct > 25) _tft->fillRect(barX + 9, y + 12, 3, 12, sigColor);
        if (pct > 50) _tft->fillRect(barX + 14, y + 8, 3, 16, sigColor);
        if (pct > 75) _tft->fillRect(barX + 19, y + 4, 3, 20, sigColor);

        // Lock icon if secured
        if (_networks[i].secured) {
            _tft->setTextColor(TFT_YELLOW);
            _tft->setCursor(barX + barWidth - 16, y + 6);
            _tft->print("*");
        }

        // SSID name
        _tft->setTextColor(TFT_WHITE);
        _tft->setCursor(barX + 26, y + 6);
        String name = _networks[i].ssid;
        if (name.length() > 16) name = name.substring(0, 15) + "~";
        _tft->print(name);

        y += 28;
    }

    // Scroll indicators
    if (_scrollOffset > 0) {
        _tft->setTextColor(TFT_DARKGREY);
        _tft->setCursor(112, 35);
        _tft->print("^");
    }
    if (_scrollOffset + maxVisible < (int)_networks.size()) {
        _tft->setTextColor(TFT_DARKGREY);
        _tft->setCursor(112, 218);
        _tft->print("v");
    }
}

void WiFiManager::_handleNetworkSelect() {
    TouchGesture g = _touch->getGesture();
    bool tapped = _touch->justTapped();

    if (g == GESTURE_SWIPE_UP && _scrollOffset + 6 < (int)_networks.size()) {
        _scrollOffset++;
        _drawNetworkList();
        return;
    }
    if (g == GESTURE_SWIPE_DOWN && _scrollOffset > 0) {
        _scrollOffset--;
        _drawNetworkList();
        return;
    }

    if (g == GESTURE_SINGLE_TAP || tapped) {
        TouchPoint p = _touch->getPoint();
        int y = 45;
        for (int i = _scrollOffset; i < (int)_networks.size() && i < _scrollOffset + 6; i++) {
            if (p.y >= y && p.y < y + 24 && p.x >= 30 && p.x <= 210) {
                _selectedSSID = _networks[i].ssid;
                Serial.printf("\r\n[WM] Selected: %s\n", _selectedSSID.c_str());
                if (!_networks[i].secured) {
                    _password = "";
                    WiFi.begin(_selectedSSID.c_str());
                    _connectStart = millis();
                    _state = WM_CONNECTING;
                    _drawConnecting();
                } else {
                    _password = "";
                    _kbShift = 0;
                    _kbSymbol = 0;
                    _state = WM_KEYBOARD;
                    _drawKeyboard();
                }
                return;
            }
            y += 28;
        }
    }
}

void WiFiManager::_drawKeyboard() {
    _tft->fillScreen(TFT_BLACK);

    // WiFi name at top
    _tft->setTextColor(TFT_CYAN);
    _tft->setTextSize(1);
    _tft->setCursor(40, 8);
    String ssidDisplay = _selectedSSID;
    if (ssidDisplay.length() > 18) ssidDisplay = ssidDisplay.substring(0, 17) + "~";
    _tft->printf("WiFi: %s", ssidDisplay.c_str());

    // Password field
    _tft->fillRoundRect(30, 26, 180, 20, 4, 0x1082);
    _tft->setTextColor(TFT_WHITE);
    _tft->setCursor(36, 31);
    _tft->print("Pass: ");
    String display = _password;
    if (display.length() > 16) display = display.substring(display.length() - 16);
    _tft->print(display);
    _tft->print("_");

    // Keyboard rows
    const char** rows;
    if (_kbSymbol) {
        rows = KB_ROWS_SYMBOL;
    } else if (_kbShift) {
        rows = KB_ROWS_UPPER;
    } else {
        rows = KB_ROWS_LOWER;
    }

    for (int r = 0; r < KB_NUM_ROWS; r++) {
        int len = strlen(rows[r]);
        int totalW = len * KB_KEY_W;
        int startX = (240 - totalW) / 2;
        int y = KB_START_Y + r * (KB_KEY_H + 2);

        for (int c = 0; c < len; c++) {
            int x = startX + c * KB_KEY_W;
            _tft->fillRoundRect(x, y, KB_KEY_W - 2, KB_KEY_H - 2, 3, 0x2104);
            _tft->setTextColor(TFT_WHITE);
            _tft->setCursor(x + 7, y + 6);
            _tft->printf("%c", rows[r][c]);
        }
    }

    // Bottom row 1: Shift, Sym, Space, Del
    int btn1Y = KB_START_Y + KB_NUM_ROWS * (KB_KEY_H + 2) + 1;

    // Shift
    _tft->fillRoundRect(30, btn1Y, 38, 22, 3, _kbShift ? TFT_BLUE : 0x2104);
    _tft->setTextColor(TFT_WHITE);
    _tft->setCursor(36, btn1Y + 5);
    _tft->print("Shft");

    // Sym
    _tft->fillRoundRect(72, btn1Y, 32, 22, 3, _kbSymbol ? TFT_BLUE : 0x2104);
    _tft->setCursor(78, btn1Y + 5);
    _tft->print("Sym");

    // Space
    _tft->fillRoundRect(108, btn1Y, 52, 22, 3, 0x2104);
    _tft->setCursor(118, btn1Y + 5);
    _tft->print("Space");

    // Del
    _tft->fillRoundRect(164, btn1Y, 38, 22, 3, 0x4800);
    _tft->setCursor(172, btn1Y + 5);
    _tft->print("Del");

    // Bottom row 2: Connect button (wide, green, centered)
    int btn2Y = btn1Y + 26;
    _tft->fillRoundRect(55, btn2Y, 130, 24, 5, TFT_DARKGREEN);
    _tft->setTextColor(TFT_WHITE);
    _tft->setCursor(85, btn2Y + 6);
    _tft->print("Connect");
}

char WiFiManager::_getKeyAt(int px, int py) {
    const char** rows;
    if (_kbSymbol) rows = KB_ROWS_SYMBOL;
    else if (_kbShift) rows = KB_ROWS_UPPER;
    else rows = KB_ROWS_LOWER;

    for (int r = 0; r < KB_NUM_ROWS; r++) {
        int len = strlen(rows[r]);
        int totalW = len * KB_KEY_W;
        int startX = (240 - totalW) / 2;
        int y = KB_START_Y + r * (KB_KEY_H + 2);

        if (py >= y && py < y + KB_KEY_H) {
            int col = (px - startX) / KB_KEY_W;
            if (col >= 0 && col < len) {
                return rows[r][col];
            }
        }
    }
    return 0;
}

void WiFiManager::_drawMagnifier(int px, int py, char ch) {
    // Erase previous magnifier first by redrawing keyboard
    if (_magnifierShown) {
        _drawKeyboard();
    }

    // Draw magnifying bubble above the finger
    int mx = px;
    int my = py - 40;
    if (my < 10) my = py + 40;
    if (mx < 25) mx = 25;
    if (mx > 215) mx = 215;

    _tft->fillCircle(mx, my, 20, TFT_DARKGREY);
    _tft->drawCircle(mx, my, 20, TFT_WHITE);
    _tft->setTextColor(TFT_WHITE);
    _tft->setTextSize(3);
    _tft->setCursor(mx - 8, my - 10);
    _tft->printf("%c", ch);
    _tft->setTextSize(1);
    _magnifierShown = true;
}

void WiFiManager::_eraseMagnifier() {
    if (_magnifierShown) {
        _magnifierShown = false;
        _drawKeyboard();
    }
}

void WiFiManager::_handleKeyPress() {
    // Use gesture-based detection only (not isTouched which has false positives)
    TouchGesture g = _touch->getGesture();
    bool tapped = _touch->justTapped();

    if (g != GESTURE_SINGLE_TAP && !tapped) {
        return;
    }

    TouchPoint p = _touch->getPoint();

    // Erase magnifier if it was shown
    if (_magnifierShown) _eraseMagnifier();

    // Check character keys
    char ch = _getKeyAt(p.x, p.y);
    if (ch != 0) {
        _password += ch;
        Serial.printf("\r\n[WM] Key: %c -> pass='%s'\n", ch, _password.c_str());
        // Show magnifier briefly as visual feedback
        _drawMagnifier(p.x, p.y, ch);
        delay(150);
        _magnifierShown = false;
        _drawKeyboard();
        return;
    }

    // Bottom row 1: Shift, Sym, Space, Del
    int btn1Y = KB_START_Y + KB_NUM_ROWS * (KB_KEY_H + 2) + 1;
    if (p.y >= btn1Y && p.y < btn1Y + 22) {
        if (p.x >= 30 && p.x < 68) {
            _kbShift = !_kbShift;
            _kbSymbol = 0;
            _drawKeyboard();
        } else if (p.x >= 72 && p.x < 104) {
            _kbSymbol = !_kbSymbol;
            _kbShift = 0;
            _drawKeyboard();
        } else if (p.x >= 108 && p.x < 160) {
            _password += ' ';
            _drawKeyboard();
        } else if (p.x >= 164 && p.x < 202) {
            if (_password.length() > 0) {
                _password.remove(_password.length() - 1);
            }
            _drawKeyboard();
        }
        return;
    }

    // Bottom row 2: Connect
    int btn2Y = btn1Y + 26;
    if (p.y >= btn2Y && p.y < btn2Y + 24 && p.x >= 55 && p.x < 185) {
        Serial.printf("\r\n[WM] Connecting to '%s' with pass '%s'\n", _selectedSSID.c_str(), _password.c_str());
        WiFi.begin(_selectedSSID.c_str(), _password.c_str());
        _connectStart = millis();
        _state = WM_CONNECTING;
        _drawConnecting();
    }
}

void WiFiManager::_drawConnecting() {
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextColor(TFT_CYAN);
    _tft->setTextSize(1);
    _tft->setCursor(55, 100);
    _tft->println("Connecting...");
    _tft->setTextColor(TFT_WHITE);
    _tft->setCursor(40, 120);
    _tft->println(_selectedSSID);
}

void WiFiManager::_drawConnected() {
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextColor(TFT_GREEN);
    _tft->setTextSize(1);
    _tft->setCursor(65, 90);
    _tft->println("Connected!");
    _tft->setTextColor(TFT_WHITE);
    _tft->setCursor(50, 115);
    _tft->println(_selectedSSID);
    _tft->setTextColor(TFT_CYAN);
    _tft->setCursor(55, 140);
    _tft->println(getIP());
    Serial.printf("\r\n[WiFi] Connected! IP: %s\n", getIP().c_str());
}

void WiFiManager::_drawFailed() {
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextColor(TFT_RED);
    _tft->setTextSize(1);
    _tft->setCursor(45, 100);
    _tft->println("Connection Failed");
    _tft->setTextColor(TFT_DARKGREY);
    _tft->setCursor(50, 130);
    _tft->println("Tap to retry");
}

void WiFiManager::_saveCredentials() {
    _prefs.putString("ssid", _selectedSSID);
    _prefs.putString("pass", _password);
    Serial.println("\r\n[WiFi] Credentials saved");
}

bool WiFiManager::_loadCredentials() {
    _selectedSSID = _prefs.getString("ssid", "");
    _password = _prefs.getString("pass", "");
    return _selectedSSID.length() > 0;
}

int WiFiManager::_rssiToPercent(int32_t rssi) {
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;
    return 2 * (rssi + 100);
}
