#include "CountdownTimer.h"

CountdownTimer::CountdownTimer(TM1637Display& display) 
  : display(display), 
    default_seconds(10), 
    current_seconds(10), 
    is_running(false),
    last_update_time(0),
    onFinished([]() {}) {}

void CountdownTimer::start() {
  if (current_seconds > 0) {
    is_running = true;
    last_update_time = millis();
  }
}

void CountdownTimer::reset() {
  current_seconds = default_seconds;
  is_running = false;
  showTimePrivate(current_seconds);
}

void CountdownTimer::incrementTime(int sec) {
  if (!is_running) {
    default_seconds = max(0, default_seconds + sec);
    current_seconds = default_seconds;
    showTimePrivate(current_seconds);
  }
}

void CountdownTimer::setOnFinished(std::function<void()> callback) {
  onFinished = callback;
}

void CountdownTimer::update() {
  if (!is_running) return;

  unsigned long now = millis();
  if (now - last_update_time >= 1000) {
    last_update_time = now;

    if (current_seconds > 0) {
      current_seconds--;
      showTimePrivate(current_seconds);
    } else {
      is_running = false;
      onFinished();
    }
  }
}

void CountdownTimer::setTime(int seconds) {
  if (!is_running) {
    default_seconds = seconds;
    current_seconds = seconds;
    showTimePrivate(current_seconds);
  }
}

void CountdownTimer::setLastUpdateTime() {
  last_update_time = millis();
}

bool CountdownTimer::isRunning() {
  return is_running;
}

int CountdownTimer::getRemainingTime() {
  return current_seconds;
}

void CountdownTimer::showTime(int seconds) {
  showTimePrivate(seconds);
}

void CountdownTimer::showTimePrivate(int seconds) {
  int minutes = seconds / 60;
  int secs = seconds % 60;
  int display_value = minutes * 100 + secs;
  display.showNumberDecEx(display_value, 0b11100000, true);
}