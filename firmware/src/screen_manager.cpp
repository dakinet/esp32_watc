#include "screen_manager.h"

static Screen _emptyScreen = {0, "No screens"};

void ScreenManager::setScreens(const JsonArray& screensArray) {
    _screens.clear();
    for (JsonObject s : screensArray) {
        Screen screen;
        screen.id = s["id"] | 0;
        screen.text = s["text"].as<String>();
        _screens.push_back(screen);
    }
    _currentIndex = 0;
}

void ScreenManager::addScreen(int id, const String& text) {
    Screen s;
    s.id = id;
    s.text = text;
    _screens.push_back(s);
}

void ScreenManager::clearScreens() {
    _screens.clear();
    _currentIndex = 0;
}

const Screen& ScreenManager::current() const {
    if (_screens.empty()) return _emptyScreen;
    return _screens[_currentIndex];
}

bool ScreenManager::next() {
    if (_currentIndex < (int)_screens.size() - 1) {
        _currentIndex++;
        return true;
    }
    return false;
}

bool ScreenManager::prev() {
    if (_currentIndex > 0) {
        _currentIndex--;
        return true;
    }
    return false;
}

void ScreenManager::goTo(int index) {
    if (index >= 0 && index < (int)_screens.size()) {
        _currentIndex = index;
    }
}
