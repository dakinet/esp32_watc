#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <Preferences.h>
#include "soc/rtc_cntl_reg.h"
#include "touch_driver.h"
#include "wifi_manager.h"
#include "cloud_client.h"
#include "screen_manager.h"

// ---- Display config (I80 parallel, GC9A01 240x240) ----
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Light_PWM _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 16000000;
            cfg.pin_wr = 3;
            cfg.pin_rd = -1;
            cfg.pin_rs = 18;  // DC
            cfg.pin_d0 = 10;
            cfg.pin_d1 = 11;
            cfg.pin_d2 = 12;
            cfg.pin_d3 = 13;
            cfg.pin_d4 = 14;
            cfg.pin_d5 = 15;
            cfg.pin_d6 = 16;
            cfg.pin_d7 = 17;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 2;
            cfg.pin_rst = 21;
            cfg.pin_busy = -1;
            cfg.panel_width = 240;
            cfg.panel_height = 240;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.invert = true;
            cfg.rgb_order = false;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 42;
            cfg.invert = false;
            cfg.freq = 5000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ---- Global objects ----
LGFX tft;
TouchDriver touch;
WiFiManager wifiMgr;
CloudClient cloud;
ScreenManager screenMgr;

// ---- Cloud server config ----
const char* CLOUD_HOST = "192.168.5.20";
const uint16_t CLOUD_PORT = 3000;
const char* DEVICE_TOKEN = "esp32-display-01";

// ---- State ----
enum AppState {
    STATE_WIFI_SETUP,
    STATE_CONNECTING_CLOUD,
    STATE_RUNNING,
    STATE_SLEEP,
    STATE_SETUP_MENU
};

// ---- Setup menu items ----
const int SETUP_ITEMS = 3;
const char* setupLabels[SETUP_ITEMS] = {"Flash Mode", "WiFi Reset", "Back"};
int setupSelected = -1;  // highlighted item

AppState appState = STATE_WIFI_SETUP;
bool displaySleep = false;
unsigned long lastStatusSend = 0;

// ---- Display settings (controlled from browser) ----
int currentBrightness = 255;
int currentTextSize = 1;

// ---- Scroll state ----
int textScrollY = 0;
int textTotalHeight = 0;
const int SCROLL_STEP = 30;

// ---- UTF-8 helpers ----
int utf8CharLen(uint8_t c) {
    if (c < 0x80) return 1;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

int utf8StringCharCount(const String& s) {
    int count = 0;
    for (int i = 0; i < (int)s.length(); ) {
        i += utf8CharLen((uint8_t)s[i]);
        count++;
    }
    return count;
}

// Get the font for the current text size setting
const lgfx::IFont* getFontForSize(int size) {
    switch (size) {
        case 2:  return &lgfx::fonts::efontJA_16;
        case 3:  return &lgfx::fonts::efontJA_24;
        default: return &lgfx::fonts::efontJA_12;
    }
}

// Get line height for the current text size
int getLineHeight(int size) {
    switch (size) {
        case 2:  return 20;
        case 3:  return 28;
        default: return 16;
    }
}

// ---- Text wrapping with hyphenation ----
// If measureOnly=true, just returns total height without drawing.
// scrollOffset shifts all text up by that many pixels (for scrolling).
int drawWrappedText(const String& text, int margin, int startY, int maxWidth, int maxY, int scrollOffset = 0, bool measureOnly = false) {
    int x = margin;
    int y = startY;
    int lineH = getLineHeight(currentTextSize);
    int hyphenW = measureOnly ? 6 : tft.textWidth("-");

    if (!measureOnly) tft.setCursor(x, y - scrollOffset);

    int i = 0;
    int len = (int)text.length();

    // For measurement mode, don't limit by maxY
    int limitY = measureOnly ? 100000 : (maxY + scrollOffset);

    while (i < len && y < limitY) {
        char c = text[i];

        // Handle newlines
        if (c == '\n') {
            y += lineH;
            x = margin;
            if (!measureOnly) tft.setCursor(x, y - scrollOffset);
            i++;
            continue;
        }

        // Handle spaces
        if (c == ' ') {
            int spW = measureOnly ? 4 : tft.textWidth(" ");
            if (x + spW <= margin + maxWidth) {
                x += spW;
            }
            i++;
            continue;
        }

        // Find end of current word (respecting UTF-8)
        int wordStart = i;
        int wordEnd = i;
        while (wordEnd < len && text[wordEnd] != ' ' && text[wordEnd] != '\n') {
            wordEnd += utf8CharLen((uint8_t)text[wordEnd]);
        }

        String word = text.substring(wordStart, wordEnd);
        int wordW = measureOnly ? (utf8StringCharCount(word) * 8) : tft.textWidth(word);

        // Helper: should we actually draw on this line?
        #define IN_VIEW(yy) (!measureOnly && (yy) - scrollOffset >= startY && (yy) - scrollOffset < maxY)

        if (x == margin) {
            // Beginning of line
            if (wordW <= maxWidth) {
                if (IN_VIEW(y)) {
                    tft.setCursor(x, y - scrollOffset);
                    tft.print(word);
                }
                x += wordW;
            } else {
                // Word too long - break with hyphen
                int ci = wordStart;
                int charsOnLine = 0;
                while (ci < wordEnd && y < limitY) {
                    int cLen = utf8CharLen((uint8_t)text[ci]);
                    String ch = text.substring(ci, ci + cLen);
                    int chW = measureOnly ? 8 : tft.textWidth(ch);
                    int charsRemaining = 0;
                    for (int ri = ci + cLen; ri < wordEnd; ) {
                        ri += utf8CharLen((uint8_t)text[ri]);
                        charsRemaining++;
                    }

                    if (x + chW + hyphenW > margin + maxWidth && charsOnLine >= 2) {
                        if (charsRemaining <= 1 && x + chW <= margin + maxWidth) {
                            if (IN_VIEW(y)) {
                                tft.setCursor(x, y - scrollOffset);
                                tft.print(ch);
                            }
                            x += chW;
                            ci += cLen;
                            continue;
                        }
                        if (IN_VIEW(y)) {
                            tft.setCursor(x, y - scrollOffset);
                            tft.print("-");
                        }
                        y += lineH;
                        x = margin;
                        charsOnLine = 0;
                        if (y >= limitY) break;
                    }

                    if (IN_VIEW(y)) {
                        tft.setCursor(x, y - scrollOffset);
                        tft.print(ch);
                    }
                    x += chW;
                    charsOnLine++;
                    ci += cLen;
                }
            }
        } else {
            int spW = measureOnly ? 4 : tft.textWidth(" ");
            if (x + spW + wordW <= margin + maxWidth) {
                if (IN_VIEW(y)) {
                    tft.setCursor(x, y - scrollOffset);
                    tft.print(" ");
                    tft.print(word);
                }
                x += spW + wordW;
            } else if (wordW <= maxWidth) {
                y += lineH;
                x = margin;
                if (y >= limitY) break;
                if (IN_VIEW(y)) {
                    tft.setCursor(x, y - scrollOffset);
                    tft.print(word);
                }
                x += wordW;
            } else {
                y += lineH;
                x = margin;
                if (y >= limitY) break;

                int ci = wordStart;
                int charsOnLine = 0;
                while (ci < wordEnd && y < limitY) {
                    int cLen = utf8CharLen((uint8_t)text[ci]);
                    String ch = text.substring(ci, ci + cLen);
                    int chW = measureOnly ? 8 : tft.textWidth(ch);

                    if (x + chW + hyphenW > margin + maxWidth && charsOnLine >= 2) {
                        if (IN_VIEW(y)) {
                            tft.setCursor(x, y - scrollOffset);
                            tft.print("-");
                        }
                        y += lineH;
                        x = margin;
                        charsOnLine = 0;
                        if (y >= limitY) break;
                    }

                    if (IN_VIEW(y)) {
                        tft.setCursor(x, y - scrollOffset);
                        tft.print(ch);
                    }
                    x += chW;
                    charsOnLine++;
                    ci += cLen;
                }
            }
        }

        #undef IN_VIEW
        i = wordEnd;
    }

    // Return total height of text content
    return y + lineH - startY;
}

// ---- Display helpers ----
void drawScreen(const Screen& screen) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setFont(getFontForSize(currentTextSize));
    tft.setTextSize(1);
    tft.setTextWrap(false);  // We handle wrapping ourselves

    // Margins for round display
    int margin = 30;
    int maxWidth = 240 - 2 * margin;
    int startY = margin + 5;
    int maxY = 205;  // Leave room for page indicator dots

    // Set clip rect to text area to prevent drawing outside
    tft.setClipRect(0, startY, 240, maxY - startY);
    drawWrappedText(screen.text, margin, startY, maxWidth, maxY, textScrollY);
    tft.clearClipRect();

    // Draw scroll indicator if text overflows
    if (textTotalHeight > (maxY - startY)) {
        int barH = maxY - startY;
        int visibleH = maxY - startY;
        int thumbH = max(8, (int)((float)visibleH / textTotalHeight * barH));
        int maxScroll = textTotalHeight - visibleH;
        int thumbY = startY + (maxScroll > 0 ? (int)((float)textScrollY / maxScroll * (barH - thumbH)) : 0);
        tft.fillRoundRect(233, thumbY, 3, thumbH, 1, 0x4208);
    }

    // Page indicator dots at bottom
    int total = screenMgr.totalScreens();
    int current = screenMgr.currentIndex();
    if (total > 1) {
        int dotSize = 4;
        int dotGap = 10;
        int totalWidth = total * dotGap;
        int startX = 120 - totalWidth / 2;
        int dotY = 215;
        for (int i = 0; i < total; i++) {
            uint16_t color = (i == current) ? TFT_WHITE : 0x3186;
            tft.fillCircle(startX + i * dotGap, dotY, dotSize / 2, color);
        }
    }

    // Connection status indicator (small dot top-right area)
    uint16_t statusColor = cloud.isConnected() ? TFT_GREEN : TFT_RED;
    tft.fillCircle(200, 30, 4, statusColor);
}

void drawNoScreens() {
    tft.fillScreen(TFT_BLACK);
    tft.setFont(&lgfx::fonts::efontJA_12);
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(50, 105);
    tft.println("Waiting for");
    tft.setCursor(60, 125);
    tft.println("messages...");

    uint16_t statusColor = cloud.isConnected() ? TFT_GREEN : TFT_RED;
    tft.fillCircle(200, 30, 4, statusColor);
}

void drawCloudConnecting() {
    tft.fillScreen(TFT_BLACK);
    tft.setFont(&lgfx::fonts::efontJA_12);
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(1);
    tft.setCursor(40, 100);
    tft.println("Connecting to");
    tft.setCursor(60, 120);
    tft.println("server...");
}

// ---- Setup Menu ----
void drawSetupMenu() {
    tft.fillScreen(TFT_BLACK);

    // Title
    tft.setFont(&lgfx::fonts::efontJA_16);
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(1);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString("Setup", 120, 40);

    // Draw menu items as rounded rectangles
    tft.setFont(&lgfx::fonts::efontJA_14);
    int itemH = 32;
    int itemW = 150;
    int startY = 75;
    int gap = 10;

    for (int i = 0; i < SETUP_ITEMS; i++) {
        int y = startY + i * (itemH + gap);
        int x = 120 - itemW / 2;

        // Different colors per item
        uint16_t bgColor, txtColor;
        if (i == 0) {
            // Flash Mode - orange/warning
            bgColor = 0x4A00;  // dark orange
            txtColor = TFT_ORANGE;
        } else if (i == 1) {
            // WiFi Reset - red-ish
            bgColor = 0x4000;  // dark red
            txtColor = TFT_YELLOW;
        } else {
            // Back - neutral
            bgColor = 0x2104;  // dark gray
            txtColor = TFT_WHITE;
        }

        tft.fillRoundRect(x, y, itemW, itemH, 8, bgColor);
        tft.drawRoundRect(x, y, itemW, itemH, 8, txtColor);
        tft.setTextColor(txtColor);
        tft.drawString(setupLabels[i], 120, y + itemH / 2);
    }

    tft.setTextDatum(lgfx::top_left);  // reset datum

    // Hint at bottom
    tft.setFont(&lgfx::fonts::efontJA_10);
    tft.setTextColor(0x4208);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString("Long press to open", 120, 220);
    tft.setTextDatum(lgfx::top_left);
}

void handleSetupTap(int x, int y) {
    int itemH = 32;
    int itemW = 150;
    int startY = 75;
    int gap = 10;
    int left = 120 - itemW / 2;

    for (int i = 0; i < SETUP_ITEMS; i++) {
        int iy = startY + i * (itemH + gap);
        if (x >= left && x <= left + itemW && y >= iy && y <= iy + itemH) {
            if (i == 0) {
                // Flash Mode - reboot into download mode
                tft.fillScreen(TFT_BLACK);
                tft.setFont(&lgfx::fonts::efontJA_16);
                tft.setTextColor(TFT_ORANGE);
                tft.setTextDatum(lgfx::middle_center);
                tft.drawString("Entering", 120, 100);
                tft.drawString("Flash Mode...", 120, 125);
                tft.setTextDatum(lgfx::top_left);
                Serial.println("\r\n[App] Rebooting to download mode!");
                delay(1000);
                // Force boot into download mode
                SET_PERI_REG_MASK(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
                esp_restart();
            } else if (i == 1) {
                // WiFi Reset
                tft.fillScreen(TFT_BLACK);
                tft.setFont(&lgfx::fonts::efontJA_16);
                tft.setTextColor(TFT_YELLOW);
                tft.setTextDatum(lgfx::middle_center);
                tft.drawString("WiFi Reset...", 120, 110);
                tft.setTextDatum(lgfx::top_left);
                Serial.println("\r\n[App] WiFi credentials cleared!");
                delay(500);
                Preferences prefs;
                prefs.begin("wifi", false);
                prefs.clear();
                prefs.end();
                delay(500);
                esp_restart();
            } else {
                // Back
                appState = STATE_RUNNING;
                if (screenMgr.hasScreens()) {
                    drawScreen(screenMgr.current());
                } else {
                    drawNoScreens();
                }
            }
            return;
        }
    }
}

void enterSleep() {
    displaySleep = true;
    tft.setBrightness(0);
    Serial.println("\r\n[App] Display sleep");
}

void wakeUp() {
    displaySleep = false;
    tft.setBrightness(currentBrightness);
    Serial.println("\r\n[App] Display wake");
    // Redraw current screen
    if (screenMgr.hasScreens()) {
        drawScreen(screenMgr.current());
    } else {
        drawNoScreens();
    }
}

void applySettings(int brightness, int textSize) {
    Serial.printf("\r\n[App] Settings: brightness=%d, textSize=%d\n", brightness, textSize);
    currentBrightness = brightness;
    currentTextSize = constrain(textSize, 1, 3);

    if (!displaySleep) {
        tft.setBrightness(currentBrightness);
        // Recalculate text height and redraw with new text size
        if (screenMgr.hasScreens()) {
            textScrollY = 0;
            tft.setFont(getFontForSize(currentTextSize));
            textTotalHeight = drawWrappedText(screenMgr.current().text, 30, 35, 180, 205, 0, true);
            drawScreen(screenMgr.current());
        }
    }
}

// ---- Setup ----
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32-S3 Remote Display ===");

    // Init display
    tft.begin();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    // Init touch
    if (!touch.begin(8, 9, 0, 0x15)) {
        Serial.println("\r\n[App] Touch init failed, continuing without touch");
    }

    // Try auto-connect WiFi
    wifiMgr.begin(&tft, &touch);
    if (wifiMgr.autoConnect()) {
        appState = STATE_CONNECTING_CLOUD;
        drawCloudConnecting();
    } else {
        appState = STATE_WIFI_SETUP;
    }
}

// ---- Main loop ----
void loop() {
    touch.update();

    switch (appState) {
        case STATE_WIFI_SETUP: {
            if (wifiMgr.loop()) {
                // WiFi connected, move on
                delay(1000);  // Show connected screen briefly
                appState = STATE_CONNECTING_CLOUD;
                drawCloudConnecting();
            }
            break;
        }

        case STATE_CONNECTING_CLOUD: {
            // Setup cloud callbacks
            cloud.onScreensReceived([](JsonArray& data) {
                Serial.printf("\r\n[App] Received %d screens\n", data.size());
                screenMgr.setScreens(data);
                textScrollY = 0;
                if (screenMgr.hasScreens() && !displaySleep) {
                    // Measure total text height for scrolling
                    tft.setFont(getFontForSize(currentTextSize));
                    textTotalHeight = drawWrappedText(screenMgr.current().text, 30, 35, 180, 205, 0, true);
                    drawScreen(screenMgr.current());
                }
            });

            cloud.onConnectionChange([](bool connected) {
                Serial.printf("\r\n[App] Cloud %s\n", connected ? "CONNECTED" : "DISCONNECTED");
                if (!displaySleep) {
                    if (screenMgr.hasScreens()) {
                        drawScreen(screenMgr.current());
                    }
                    else {
                        drawNoScreens();
                    }
                }
            });

            cloud.onSettingsReceived([](int brightness, int textSize) {
                applySettings(brightness, textSize);
            });

            cloud.begin(CLOUD_HOST, CLOUD_PORT, DEVICE_TOKEN);
            appState = STATE_RUNNING;
            drawNoScreens();
            break;
        }

        case STATE_RUNNING: {
            cloud.loop();

            // Handle touch gestures
            TouchGesture gesture = touch.getGesture();

            if (displaySleep) {
                // Only wake on double tap
                if (gesture == GESTURE_DOUBLE_TAP) {
                    wakeUp();
                }
                break;
            }

            switch (gesture) {
                case GESTURE_SWIPE_RIGHT:
                    if (screenMgr.hasScreens() && screenMgr.next()) {
                        textScrollY = 0;
                        textTotalHeight = 0;
                        drawScreen(screenMgr.current());
                        cloud.sendGesture("swipe_right");
                        cloud.sendStatus(screenMgr.currentIndex(), screenMgr.totalScreens());
                    }
                    break;

                case GESTURE_SWIPE_LEFT:
                    if (screenMgr.hasScreens() && screenMgr.prev()) {
                        textScrollY = 0;
                        textTotalHeight = 0;
                        drawScreen(screenMgr.current());
                        cloud.sendGesture("swipe_left");
                        cloud.sendStatus(screenMgr.currentIndex(), screenMgr.totalScreens());
                    }
                    break;

                case GESTURE_SWIPE_UP:
                    if (screenMgr.hasScreens()) {
                        int visibleH = 205 - 35;  // maxY - startY
                        if (textTotalHeight > visibleH) {
                            int maxScroll = textTotalHeight - visibleH;
                            textScrollY = min(textScrollY + SCROLL_STEP, maxScroll);
                            drawScreen(screenMgr.current());
                        }
                    }
                    break;

                case GESTURE_SWIPE_DOWN:
                    if (screenMgr.hasScreens() && textScrollY > 0) {
                        textScrollY = max(0, textScrollY - SCROLL_STEP);
                        drawScreen(screenMgr.current());
                    }
                    break;

                case GESTURE_DOUBLE_TAP:
                    enterSleep();
                    break;

                case GESTURE_LONG_PRESS:
                    appState = STATE_SETUP_MENU;
                    drawSetupMenu();
                    Serial.println("\r\n[App] Setup menu opened");
                    break;

                default:
                    break;
            }

            // Periodic status update
            if (millis() - lastStatusSend > 30000) {
                cloud.sendStatus(screenMgr.currentIndex(), screenMgr.totalScreens());
                lastStatusSend = millis();
            }
            break;
        }

        case STATE_SETUP_MENU: {
            TouchGesture gesture = touch.getGesture();
            if (gesture == GESTURE_SINGLE_TAP || touch.justTapped()) {
                TouchPoint p = touch.getPoint();
                handleSetupTap(p.x, p.y);
            }
            break;
        }

        case STATE_SLEEP:
            break;
    }

    delay(10);
}
