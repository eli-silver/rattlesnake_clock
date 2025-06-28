#include "Switch.h"

Switch::Switch(uint8_t pin) 
  : pin(pin), 
    state(false), 
    last_state(false),
    last_debounce_time(0), 
    press_start(0), 
    long_press_reported(false),
    onShortPress([]() {}),
    onLongPress([]() {}) {
  pinMode(pin, INPUT_PULLUP);
}

void Switch::update() {
  bool reading = digitalRead(pin) == LOW;
  unsigned long now = millis();

  if (reading != last_state) {
    last_debounce_time = now;
  }

  if ((now - last_debounce_time) > debounce_delay) {
    if (reading != state) {
      state = reading;

      if (state) {
        press_start = now;
        long_press_reported = false;
      } else {
        unsigned long press_duration = now - press_start;
        if (press_duration >= long_press_time) {
          onLongPress();
        } else {
          onShortPress();
        }
      }
    }
  }

  last_state = reading;
}

void Switch::setHandlers(std::function<void()> shortPressFunc, std::function<void()> longPressFunc) {
  onShortPress = shortPressFunc;
  onLongPress = longPressFunc;
}