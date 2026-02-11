#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct Screen {
    int id;
    String text;
};

class ScreenManager {
public:
    void setScreens(const JsonArray& screensArray);
    void addScreen(int id, const String& text);
    void clearScreens();

    const Screen& current() const;
    int currentIndex() const { return _currentIndex; }
    int totalScreens() const { return _screens.size(); }
    bool hasScreens() const { return !_screens.empty(); }

    bool next();
    bool prev();
    void goTo(int index);

private:
    std::vector<Screen> _screens;
    int _currentIndex = 0;
};
