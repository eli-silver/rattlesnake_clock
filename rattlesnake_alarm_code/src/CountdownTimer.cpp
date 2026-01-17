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
    blink_state(true),
    last_hhmm_flash_time(0) {}

void CountdownTimer::start() {
  if (current_seconds > 0) {
    is_running = true;
    last_update_time = millis();
    last_hhmm_flash_time = millis(); // Initialize flash timing
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
    const int MAX_SECONDS = 24 * 3600; // 24 hours = 86400 seconds
    default_seconds = max(0, min(MAX_SECONDS, default_seconds + sec));
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

    // Update countdown every second
    if (now - last_update_time >= 1000) {
      last_update_time = now;

      if (current_seconds > 0) {
        current_seconds--;
      } else {
        is_running = false;
        onFinished();
        return;
      }
    }

    // Always refresh display when running (for HH:MM flashing)
    showTimePrivate(current_seconds);
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

    // Switch to HH:MM format when >= 60 minutes
    if (minutes >= 60) {
      int hours = minutes / 60;
      int mins = minutes % 60;

      // Flash minutes with duty cycle when running
      if (is_running) {
        unsigned long now = millis();
        unsigned long time_in_period = (now - last_hhmm_flash_time) % HHMM_FLASH_PERIOD;
        unsigned long on_time = HHMM_FLASH_PERIOD * HHMM_FLASH_DUTY_CYCLE;

        if (time_in_period < on_time) {
          // Show minutes (on state)
          int display_value = hours * 100 + mins;
          display.showNumberDecEx(display_value, 0b11100000, true);
        } else {
          // Hide minutes (off state) - show only hours with colon
          int display_value = hours * 100;
          display.showNumberDecEx(display_value, 0b11100000, true);
        }
      } else {
        // Not running - show solid display
        int display_value = hours * 100 + mins;
        display.showNumberDecEx(display_value, 0b11100000, true);
        last_hhmm_flash_time = millis(); // Reset timing for when it starts running
      }
    } else {
      // Standard MM:SS format
      int display_value = minutes * 100 + secs;
      display.showNumberDecEx(display_value, 0b11100000, true);
    }
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

void CountdownTimer::showModeIndicator(char mode_char) {
  // 7-segment encodings for mode characters
  uint8_t char_segment;

  switch(mode_char) {
    case 'S':  // Seconds - display "S"
      char_segment = 0x6d; // Same as digit 5
      break;
    case 'P':  // Minutes (P for Period) - display "P"
      char_segment = 0x73; // P shape
      break;
    case 'H':  // Hours - display "H"
      char_segment = 0x76; // H shape
      break;
    default:
      char_segment = 0x00;
      break;
  }

  // Display pattern: blank, (left bracket), character, (right bracket)
  uint8_t segments[4] = {
    0x00,        // Position 0: blank
    0x30,        // Position 1: left bracket ( = top and bottom segments
    char_segment,// Position 2: S, P, or H
    0x06         // Position 3: right bracket ) = top-right and bottom-right segments
  };

  display.setSegments(segments, 4, 0);
}