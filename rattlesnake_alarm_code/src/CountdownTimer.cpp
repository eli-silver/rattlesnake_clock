#include "CountdownTimer.h"

CountdownTimer::CountdownTimer(TM1637Display& display) 
  : display(display), 
    default_seconds(10), 
    current_seconds(10), 
    is_running(false),
    last_update_time(0),
    onFinished([]() {}),
    current_blink_mode(BLINK_NONE),
    blink_start_time(0),
    last_blink_toggle(0),
    blink_state(true) {}

void CountdownTimer::start() {
  if (current_seconds > 0) {
    is_running = true;
    last_update_time = millis();
    current_blink_mode = BLINK_NONE; // Stop blinking when timer starts
  }
}

void CountdownTimer::reset() {
  current_seconds = default_seconds;
  is_running = false;
  current_blink_mode = BLINK_NONE; // Stop blinking when reset
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
  if (is_running) {
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
  } else {
    // Update blinking when not running
    updateBlinking();
  }
}

void CountdownTimer::setTime(int seconds) {
  if (!is_running) {
    default_seconds = seconds;
    current_seconds = seconds;
    current_blink_mode = BLINK_NONE; // Stop blinking when time is set via command
    showTimePrivate(current_seconds);
  }
}

void CountdownTimer::setLastUpdateTime() {
  last_update_time = millis();
}

void CountdownTimer::triggerBlink(BlinkMode mode) {
  if (!is_running) {
    current_blink_mode = mode;
    blink_start_time = millis();
    last_blink_toggle = millis();
    blink_state = true;
  }
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
  if (current_blink_mode != BLINK_NONE && !is_running) {
    showTimeWithBlink(seconds);
  } else {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    int display_value = minutes * 100 + secs;
    display.showNumberDecEx(display_value, 0b11100000, true);
  }
}

void CountdownTimer::updateBlinking() {
  unsigned long now = millis();
  
  // Check if blink timeout has elapsed
  if (current_blink_mode != BLINK_NONE && (now - blink_start_time >= BLINK_TIMEOUT)) {
    current_blink_mode = BLINK_NONE;
    showTimePrivate(current_seconds); // Show solid display
    return;
  }
  
  // Handle blink timing
  if (current_blink_mode != BLINK_NONE) {
    unsigned long time_in_cycle = (now - last_blink_toggle);
    
    if (blink_state && time_in_cycle >= BLINK_ON_TIME) {
      // Switch to off state
      blink_state = false;
      last_blink_toggle = now;
      showTimeWithBlink(current_seconds);
    } else if (!blink_state && time_in_cycle >= BLINK_OFF_TIME) {
      // Switch to on state
      blink_state = true;
      last_blink_toggle = now;
      showTimeWithBlink(current_seconds);
    }
  }
}

void CountdownTimer::showTimeWithBlink(int seconds) {
  int minutes = seconds / 60;
  int secs = seconds % 60;
  
  uint8_t segments[4];
  
  // Get individual digits
  segments[0] = getDigit(minutes, 1);  // Minutes tens
  segments[1] = getDigit(minutes, 0);  // Minutes ones
  segments[2] = getDigit(secs, 1);     // Seconds tens
  segments[3] = getDigit(secs, 0);     // Seconds ones
  
  // Apply blinking logic
  if (!blink_state) {
    if (current_blink_mode == BLINK_MINUTES) {
      segments[0] = 0x00; // Turn off minutes tens
      segments[1] = 0x00; // Turn off minutes ones
    } else if (current_blink_mode == BLINK_SECONDS) {
      segments[2] = 0x00; // Turn off seconds tens
      segments[3] = 0x00; // Turn off seconds ones
    }
  }
  
  // Always show colon
  display.setSegments(segments, 4, 0);
  display.showNumberDecEx(0, 0b11100000, false); // Just to set the colon
  
  // Set segments manually to preserve colon
  for (int i = 0; i < 4; i++) {
    display.setSegments(&segments[i], 1, i);
  }
}

uint8_t CountdownTimer::getDigit(int value, int position) {
  // Position 0 = ones digit, position 1 = tens digit
  int digit;
  if (position == 0) {
    digit = value % 10;
  } else {
    digit = (value / 10) % 10;
  }
  
  // 7-segment encoding for digits 0-9
  const uint8_t digitToSegment[] = {
    0x3f, // 0
    0x06, // 1
    0x5b, // 2
    0x4f, // 3
    0x66, // 4
    0x6d, // 5
    0x7d, // 6
    0x07, // 7
    0x7f, // 8
    0x6f  // 9
  };
  
  return digitToSegment[digit];
}