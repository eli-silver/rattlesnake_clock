#ifndef COUNTDOWN_TIMER_H
#define COUNTDOWN_TIMER_H

#include <Arduino.h>
#include <TM1637Display.h>
#include <functional>

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
    
    void showTimePrivate(int seconds);
};

#endif