#include <Arduino.h>
#include <TM1637Display.h>

const uint8_t snakeFrames[] = {
  0b00000001, // Segment A
  0b00000010, // Segment B
  0b00000100, // Segment C
  0b00001000, // Segment D
  0b00010000, // Segment E
  0b00100000, // Segment F
  0b01000000, // Segment G
};
const int numSnakeFrames = sizeof(snakeFrames) / sizeof(snakeFrames[0]);
int snakeIndex = 0;

class CountdownTimer {
  public:
    CountdownTimer(TM1637Display& display) 
      : display(display), 
        default_seconds(10), 
        current_seconds(10), 
        is_running(false),
        last_update_time(0) {}

    void start() {
      if (current_seconds > 0) {
        is_running = true;
        last_update_time = millis();
      }
    }

    void reset() {
      current_seconds = default_seconds;
      is_running = false;
      showTime(current_seconds);
    }

  void incrementTime(int sec) {
    if (!is_running) {
      default_seconds = max(0, default_seconds + sec);
      current_seconds = default_seconds;
      showTime(current_seconds);
    }
  }

void setOnFinished(std::function<void()> callback) {
  onFinished = callback;
}
    void update() {
      if (!is_running) return;

      unsigned long now = millis();
      if (now - last_update_time >= 1000) {
        last_update_time = now;

        if (current_seconds > 0) {
          current_seconds--;
          showTime(current_seconds);
          } else {
            is_running = false;
            onFinished();  // 🔔 fire alarm callback
          }

      }
    }

    bool isRunning() {
      return is_running;
    }

    int getRemainingTime() {
      return current_seconds;
    }

  private:
    TM1637Display& display;
    int default_seconds;
    int current_seconds;
    bool is_running;
    unsigned long last_update_time;

    std::function<void()> onFinished = []() {};  // default empty function

    void showTime(int seconds) {
      int minutes = seconds / 60;
      int secs = seconds % 60;
      int display_value = minutes * 100 + secs;
      display.showNumberDecEx(display_value, 0b11100000, true);
    }
};

class Switch {
  public:
    Switch(uint8_t pin) : pin(pin), state(false), last_state(false),
                          last_debounce_time(0), press_start(0), long_press_reported(false) {
      pinMode(pin, INPUT_PULLUP);
    }

    void update() {
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

    void setHandlers(std::function<void()> shortPressFunc, std::function<void()> longPressFunc) {
      onShortPress = shortPressFunc;
      onLongPress = longPressFunc;
    }

  private:
    uint8_t pin;
    bool state;
    bool last_state;
    unsigned long last_debounce_time;
    unsigned long press_start;
    bool long_press_reported;
    const unsigned long debounce_delay = 30;
    const unsigned long long_press_time = 600;

    std::function<void()> onShortPress = []() {};
    std::function<void()> onLongPress = []() {};
};

#define LED_PIN 25
#define MOT_IN1 10
#define MOT_IN2 11

#define ENC_A 20
#define ENC_B 19
#define SWITCH 18

#define CLK 13  
#define DIO 12

volatile int encoderPosition = 0;
volatile bool encoderMoved = false;
bool lastEncA = HIGH;
bool motorStarted = false;

bool alarmActive = false;
unsigned long alarmStartTime = 0;
const int alarmDuration = 1000; // 1 seconds
bool displayVisible = true;
unsigned long lastFlashTime = 0;

unsigned long motorRunStartTime = 0;
const unsigned long MAX_MOTOR_RUN_TIME = 5000; // 5 seconds max safety limit

void toggleMode();
void readEncoder();
void stopAlarm();
void handleAlarm();

TM1637Display display(CLK, DIO);
uint8_t data[] = {0xff, 0xff, 0xff, 0xff};

CountdownTimer timer(display);
enum IncrementMode { INCREMENT_MIN, INCREMENT_SEC };
IncrementMode currentMode = INCREMENT_MIN;
Switch modeSwitch(SWITCH);


void setup() {
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(MOT_IN1, OUTPUT);
  pinMode(MOT_IN2, OUTPUT);
  analogWriteResolution(10);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  display.setBrightness(0x0f); 

  timer.setOnFinished([]() {
    alarmActive = true;
    alarmStartTime = millis();
  });

  modeSwitch.setHandlers(
    []() {  // short press
      if (alarmActive) {
        stopAlarm();
      } else if (timer.isRunning()) {
        timer.reset();
      } else {
        toggleMode();
      }
    },
    []() {  // long press
      if (alarmActive) {
        stopAlarm();
      } else if (timer.isRunning()) {
        timer.reset();
      } else {
        timer.start();
      }
    }
  );
  timer.reset();
  //timer.start(); // Uncomment to make timer countdown start on boot
}

void loop() {
  timer.update();
  modeSwitch.update();
  readEncoder();

  if (alarmActive) {
    if (motorRunStartTime == 0) {
      motorRunStartTime = millis(); // Record when motor started
    }
    
    // Safety watchdog: force stop motor after max time
    if (millis() - motorRunStartTime > MAX_MOTOR_RUN_TIME) {
      Serial.println("SAFETY: Motor ran too long, forcing stop");
      stopAlarm();
      motorRunStartTime = 0;
    }

    handleAlarm();
  }else {
    digitalWrite(MOT_IN1, LOW);
    digitalWrite(MOT_IN2, LOW);
  }
  delay(5);
}

void readEncoder() {
  bool encA = digitalRead(ENC_A);
  bool encB = digitalRead(ENC_B);

  if (encA != lastEncA && encA == LOW) {
    if (encB == HIGH) {
      encoderPosition++;  // clockwise
    } else {
      encoderPosition--;  // counter-clockwise
    }
    encoderMoved = true;
  }
  lastEncA = encA;

  if (encoderMoved && !timer.isRunning()) {
    int step = (currentMode == INCREMENT_MIN) ? 60 : 5;

    if (encoderPosition > 0) {
      timer.incrementTime(step);
      encoderPosition = 0;
    } else if (encoderPosition < 0) {
      timer.incrementTime(-step);
      encoderPosition = 0;
    }
    encoderMoved = false;
  }
}

void handleAlarm() {
  unsigned long now = millis();

  if (!motorStarted) {
    float ASHER_MOTOR_PERCENT = 0.55;
    analogWrite(MOT_IN1, (int)(1024 * ASHER_MOTOR_PERCENT)); 
    digitalWrite(MOT_IN2, LOW);
    motorStarted = true;
    Serial.println("Motor started"); // Debug message
  }

  // Snake animation code...
  if (now - lastFlashTime >= 250) {
    lastFlashTime = now;
    uint8_t frame = snakeFrames[snakeIndex];
    uint8_t segments[] = {frame, frame, frame, frame};
    display.setSegments(segments);
    snakeIndex = (snakeIndex + 1) % numSnakeFrames;
  }

  // Check if alarm duration has elapsed
  unsigned long elapsedTime = now - alarmStartTime;
  if (elapsedTime >= alarmDuration) {
    Serial.print("Alarm duration elapsed: ");
    Serial.print(elapsedTime);
    Serial.println("ms");
    stopAlarm();
  }
}

void stopAlarm() {
  Serial.println("in StopAlarm");
  alarmActive = false;
  motorStarted = false; 
  motorRunStartTime = 0;

  
  // Explicitly turn off motor with multiple methods
  digitalWrite(MOT_IN1, LOW);
  digitalWrite(MOT_IN2, LOW);
  analogWrite(MOT_IN1, 0);  // Ensure PWM is also set to 0
  
  display.clear();
  timer.reset();
  
  Serial.println("Motor should be OFF now"); 
}

void toggleMode() {
  currentMode = (currentMode == INCREMENT_MIN) ? INCREMENT_SEC : INCREMENT_MIN;
  Serial.print("Mode: ");
  Serial.println(currentMode == INCREMENT_MIN ? "Minutes" : "Seconds");
}
