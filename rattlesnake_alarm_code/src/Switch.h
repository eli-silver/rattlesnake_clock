#ifndef SWITCH_H
#define SWITCH_H

#include <Arduino.h>
#include <functional>

class Switch {
  public:
    Switch(uint8_t pin);
    
    void update();
    void setHandlers(std::function<void()> shortPressFunc, std::function<void()> longPressFunc);

  private:
    uint8_t pin;
    bool state;
    bool last_state;
    unsigned long last_debounce_time;
    unsigned long press_start;
    bool long_press_reported;
    static const unsigned long debounce_delay = 30;
    static const unsigned long long_press_time = 600;

    std::function<void()> onShortPress;
    std::function<void()> onLongPress;
};

#endif