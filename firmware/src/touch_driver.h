#pragma once
#include <Arduino.h>
#include <Wire.h>

enum TouchGesture {
    GESTURE_NONE = 0,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_DOUBLE_TAP,
    GESTURE_SINGLE_TAP,
    GESTURE_LONG_PRESS
};

struct TouchPoint {
    int16_t x;
    int16_t y;
    bool pressed;
};

class TouchDriver {
public:
    bool begin(int sda = 8, int scl = 9, int rst = 0, uint8_t addr = 0x15);
    void update();

    TouchGesture getGesture();
    TouchPoint getPoint() const { return _point; }
    bool isTouched() const { return _point.pressed; }
    bool justTapped() const { return _tapped; }
    String getChipModel() const;

private:
    uint8_t _addr = 0x15;
    int _rstPin = 0;
    uint8_t _chipId = 0;
    TouchPoint _point = {0, 0, false};
    TouchGesture _gesture = GESTURE_NONE;

    // For software gesture detection
    bool _wasTouched = false;
    bool _tapped = false;
    int16_t _startX = 0, _startY = 0;
    unsigned long _touchStart = 0;
    unsigned long _lastTapTime = 0;
    bool _waitingDoubleTap = false;

    uint8_t _readReg(uint8_t reg);
    void _writeReg(uint8_t reg, uint8_t val);
};
