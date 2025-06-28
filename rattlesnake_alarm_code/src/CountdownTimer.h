#ifndef COUNTDOWN_TIMER_H
#define COUNTDOWN_TIMER_H

#include <Arduino.h>
#include <TM1637Display.h>
#include <functional>

enum BlinkMode { BLINK_NONE, BLINK_MINUTES, BLINK_SECONDS };

class CountdownTimer {
  public:
    CountdownTimer(TM1637Display& display);
    
    void start();
    void reset();
    void incrementTime(int sec);
    void setOnFinished(std::function<void()> callback);
    void update();
    void setTime(int seconds);
    void setLastUpdateTime();
    void triggerBlink(BlinkMode mode);
    
    bool isRunning();
    int getRemainingTime();
    void showTime(int seconds);

  private:
    TM1637Display& display;
    int default_seconds;
    int current_seconds;
    bool is_running;
    unsigned long last_update_time;
    std::function<void()> onFinished;
    
    // Blinking functionality
    BlinkMode current_blink_mode;
    unsigned long blink_start_time;
    unsigned long last_blink_toggle;
    bool blink_state;
    static const unsigned long BLINK_TIMEOUT = 3000;     // 3 seconds
    static const unsigned long BLINK_PERIOD = 500;       // 0.5 seconds
    static const unsigned long BLINK_ON_TIME = 400;      // 80% of 500ms = 400ms
    static const unsigned long BLINK_OFF_TIME = 100;     // 20% of 500ms = 100ms
    
    void showTimePrivate(int seconds);
    void updateBlinking();
    void showTimeWithBlink(int seconds);
    uint8_t getDigit(int value, int position);
};

#endif