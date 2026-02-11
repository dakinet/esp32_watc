#include "touch_driver.h"

// CST816 registers
#define REG_GESTURE_ID   0x01
#define REG_FINGER_NUM   0x02
#define REG_XPOS_H       0x03
#define REG_XPOS_L       0x04
#define REG_YPOS_H       0x05
#define REG_YPOS_L       0x06
#define REG_CHIP_ID      0xA7
#define REG_DIS_AUTOSLEEP 0xFE

// Gesture thresholds
#define SWIPE_MIN_DIST    40
#define TAP_MAX_DIST      20
#define TAP_MAX_TIME      400
#define DOUBLE_TAP_WINDOW 350

bool TouchDriver::begin(int sda, int scl, int rst, uint8_t addr) {
    _addr = addr;
    _rstPin = rst;

    // Reset touch controller
    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(50);

    Wire.begin(sda, scl);

    // Read chip ID
    _chipId = _readReg(REG_CHIP_ID);
    Serial.printf("\r\n[Touch] Chip ID: 0x%02X - %s\n", _chipId, getChipModel().c_str());

    if (_chipId == 0x00 || _chipId == 0xFF) {
        Serial.println("\r\n[Touch] ERROR: Touch chip not found!");
        return false;
    }

    // Disable auto-sleep
    _writeReg(REG_DIS_AUTOSLEEP, 0x01);

    Serial.println("\r\n[Touch] Initialized OK");
    return true;
}

void TouchDriver::update() {
    _gesture = GESTURE_NONE;
    _tapped = false;

    // Read touch data
    uint8_t fingerNum = _readReg(REG_FINGER_NUM);
    bool touching = (fingerNum > 0);

    if (touching) {
        uint8_t xh = _readReg(REG_XPOS_H);
        uint8_t xl = _readReg(REG_XPOS_L);
        uint8_t yh = _readReg(REG_YPOS_H);
        uint8_t yl = _readReg(REG_YPOS_L);

        _point.x = 239 - (((xh & 0x0F) << 8) | xl);  // X is mirrored on CST816S
        _point.y = ((yh & 0x0F) << 8) | yl;
        _point.pressed = true;

        if (!_wasTouched) {
            // Touch started
            _startX = _point.x;
            _startY = _point.y;
            _touchStart = millis();
            Serial.printf("\r\n[Touch] DOWN at (%d, %d)\n", _point.x, _point.y);
        }
    } else {
        _point.pressed = false;

        if (_wasTouched) {
            // Touch released - determine gesture
            unsigned long duration = millis() - _touchStart;
            int16_t dx = _point.x - _startX;
            int16_t dy = _point.y - _startY;
            int16_t absDx = abs(dx);
            int16_t absDy = abs(dy);

            // Use last known position for release point
            dx = _point.x - _startX;
            dy = _point.y - _startY;

            Serial.printf("\r\n[Touch] UP - dx=%d dy=%d duration=%lu ms\n", dx, dy, duration);

            if (absDx > SWIPE_MIN_DIST && absDx > absDy) {
                // Horizontal swipe
                _gesture = (dx > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
                _waitingDoubleTap = false;
                Serial.printf("\r\n[Touch] SWIPE %s\n", dx > 0 ? "RIGHT" : "LEFT");
            } else if (absDy > SWIPE_MIN_DIST && absDy > absDx) {
                // Vertical swipe
                _gesture = (dy > 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
                _waitingDoubleTap = false;
                Serial.printf("\r\n[Touch] SWIPE %s\n", dy > 0 ? "DOWN" : "UP");
            } else if (duration < TAP_MAX_TIME && absDx < TAP_MAX_DIST && absDy < TAP_MAX_DIST) {
                // It's a tap - check for double tap
                if (_waitingDoubleTap && (millis() - _lastTapTime < DOUBLE_TAP_WINDOW)) {
                    _gesture = GESTURE_DOUBLE_TAP;
                    _waitingDoubleTap = false;
                    Serial.println("\r\n[Touch] DOUBLE TAP");
                } else {
                    _waitingDoubleTap = true;
                    _lastTapTime = millis();
                    // Set position for tap detection
                    _point.x = _startX;
                    _point.y = _startY;
                    _point.pressed = true;  // Momentarily mark as pressed for coordinate reading
                }
            }
        }
    }

    // Resolve single tap after double-tap window expires
    if (_waitingDoubleTap && !touching && !_wasTouched && (millis() - _lastTapTime >= DOUBLE_TAP_WINDOW)) {
        _gesture = GESTURE_SINGLE_TAP;
        _tapped = true;
        _waitingDoubleTap = false;
        _point.x = _startX;
        _point.y = _startY;
        _point.pressed = true;
        Serial.printf("\r\n[Touch] SINGLE TAP at (%d, %d)\n", _point.x, _point.y);
    }

    _wasTouched = touching;
}

TouchGesture TouchDriver::getGesture() {
    TouchGesture g = _gesture;
    _gesture = GESTURE_NONE;
    return g;
}

String TouchDriver::getChipModel() const {
    switch (_chipId) {
        case 0x20: return "CST716";
        case 0xB4: return "CST816S";
        case 0xB5: return "CST816T";
        case 0xB6: return "CST816D";
        case 0x11: return "CST826";
        case 0x12: return "CST830";
        case 0x13: return "CST836U";
        default:   return "Unknown";
    }
}

uint8_t TouchDriver::_readReg(uint8_t reg) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1, (uint8_t)true);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

void TouchDriver::_writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}
