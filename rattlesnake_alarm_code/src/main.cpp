#include <Arduino.h>
#include <TM1637Display.h>
#include "CountdownTimer.h"
#include "Switch.h"
#include "SerialCommands.h"


// Snake animation frames
// Figure-8 snake animation - 4 segments tracing a figure-8 pattern
// Path: Top→Right→Middle-Left→Down→Bottom-Right→Up→Middle-Left→repeat
// 24 positions for smooth figure-8 motion
const uint8_t figure8Frames[][4] = {
  // Upper loop - going right across top
  {0b00000001, 0b00000000, 0b00000000, 0b00000000}, // Position 0: A,-,-,-
  {0b00000001, 0b00000001, 0b00000000, 0b00000000}, // Position 1: A,A,-,-
  {0b00000001, 0b00000001, 0b00000001, 0b00000000}, // Position 2: A,A,A,-
  {0b00000001, 0b00000001, 0b00000001, 0b00000001}, // Position 3: A,A,A,A
  
  // Going down right side
  {0b00000000, 0b00000001, 0b00000001, 0b00000011}, // Position 4: -,A,A,A+B
  {0b00000000, 0b00000000, 0b00000001, 0b01000110}, // Position 5: -,-,A,A+B+G
  {0b00000000, 0b00000000, 0b00000000, 0b01000110}, // Position 6: -,-,-,B+G
  
  // Crossing middle going left
  {0b00000000, 0b00000000, 0b01000000, 0b01000110}, // Position 7: -,-,G,B+G
  {0b00000000, 0b01000000, 0b01000000, 0b01000000}, // Position 8: -,G,G,G
  {0b01000000, 0b01000000, 0b01000000, 0b00000000}, // Position 9: G,G,G,-
  
  // Going down left side
  {0b01010000, 0b01000000, 0b00000000, 0b00000000}, // Position 10: G+E,G,-,-
  {0b00011000, 0b00000000, 0b00000000, 0b00000000}, // Position 11: D+E,-,-,-
  
  // Lower loop - going right across bottom
  {0b00011000, 0b00001000, 0b00000000, 0b00000000}, // Position 12: D+E,D,-,-
  {0b00001000, 0b00001000, 0b00001000, 0b00000000}, // Position 13: D,D,D,-
  {0b00000000, 0b00001000, 0b00001000, 0b00001000}, // Position 14: -,D,D,D
  {0b00000000, 0b00000000, 0b00001000, 0b00001100}, // Position 15: -,-,D,D+C
  
  // Going up right side
  {0b00000000, 0b00000000, 0b00000000, 0b01001100}, // Position 16: -,-,-,C+D+G
  {0b00000000, 0b00000000, 0b01000000, 0b01000100}, // Position 17: -,-,G,G+C
  {0b00000000, 0b01000000, 0b01000000, 0b01000000}, // Position 18: -,G,G,G
  
  // Crossing middle going left (repeat of middle cross)
  {0b01000000, 0b01000000, 0b01000000, 0b00000000}, // Position 19: G,G,G,-
  {0b01000000, 0b01000000, 0b00000000, 0b00000000}, // Position 20: G,G,-,-
  {0b01000000, 0b00000000, 0b00000000, 0b00000000}, // Position 21: G,-,-,-
  
  // Going up left side to complete figure-8
  {0b01100000, 0b00000000, 0b00000000, 0b00000000}, // Position 22: F+G,-,-,-
  {0b00100001, 0b00000000, 0b00000000, 0b00000000}  // Position 23: F+A,-,-,- (leads back to pos 0)
};
const int numFigure8Frames = sizeof(figure8Frames) / sizeof(figure8Frames[0]);
int figure8Index = 0;

// Pin definitions
#define LED_PIN 25
#define MOT_IN1 10
#define MOT_IN2 11
#define ENC_A 20
#define ENC_B 19
#define SWITCH 18
#define CLK 13  
#define DIO 12

// Global variables
volatile int encoderPosition = 0;
volatile bool encoderMoved = false;
bool lastEncA = HIGH;
bool motorStarted = false;

bool alarmActive = false;
unsigned long alarmStartTime = 0;
const int alarmDuration = 5000; // 5 seconds
unsigned long lastFlashTime = 0;

unsigned long motorRunStartTime = 0;
const unsigned long MAX_MOTOR_RUN_TIME = 15000; // 15 seconds max safety limit

enum IncrementMode { INCREMENT_MIN, INCREMENT_SEC };
IncrementMode currentMode = INCREMENT_MIN;

// Objects
TM1637Display display(CLK, DIO);
CountdownTimer timer(display);
Switch modeSwitch(SWITCH);
SerialCommands serialCommands(timer);

// Function declarations
void toggleMode();
void readEncoder();
void stopAlarm();
void handleAlarm();

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(MOT_IN1, OUTPUT);
  pinMode(MOT_IN2, OUTPUT);
  analogWriteResolution(10);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  display.setBrightness(0x0f); 
  timer.reset();

  // Set up timer callback
  timer.setOnFinished([]() {
    alarmActive = true;
    alarmStartTime = millis();
    Serial.println("Timer finished - alarm activated!");
  });

  // Set up switch handlers
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

  // Set up serial command callbacks
  serialCommands.setStopAlarmCallback([]() {
    stopAlarm();
  });
  
  serialCommands.setAlarmStatusCallback([]() {
    return alarmActive;
  });
  
  serialCommands.setMotorStatusCallback([]() {
    return motorStarted;
  });

  serialCommands.printWelcomeMessage();
}

void loop() {
  timer.update();
  modeSwitch.update();
  readEncoder();
  serialCommands.update();

  if (alarmActive) {
    if (motorRunStartTime == 0) {
      motorRunStartTime = millis();
    }
    
    unsigned long motorRunTime = millis() - motorRunStartTime;
    if (motorRunTime > MAX_MOTOR_RUN_TIME) {
      Serial.print("SAFETY: Motor ran for ");
      Serial.print(motorRunTime);
      Serial.println("ms - forcing stop");
      stopAlarm();
    } else {
      handleAlarm();
    }
  } else {
    digitalWrite(MOT_IN1, LOW);
    digitalWrite(MOT_IN2, LOW);
    analogWrite(MOT_IN1, 0);
  }
  
  delay(5);
}

void readEncoder() {
  bool encA = digitalRead(ENC_A);
  bool encB = digitalRead(ENC_B);

  if (encA != lastEncA && encA == LOW) {
    if (encB == HIGH) {
      encoderPosition++;
    } else {
      encoderPosition--;
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

/*         // Trigger appropriate blinking mode
    if (currentMode == INCREMENT_MIN) {
      timer.triggerBlink(BLINK_MINUTES);
      Serial.println("Blinking minutes");
    } else {
      timer.triggerBlink(BLINK_SECONDS);
      Serial.println("Blinking seconds");
    } */

    encoderMoved = false;
  }
}

void handleAlarm() {
  unsigned long now = millis();
  unsigned long elapsedTime = now - alarmStartTime;

  if (!motorStarted) {
<<<<<<< Updated upstream
    float ASHER_MOTOR_PERCENT = 0.30;
    analogWrite(MOT_IN1, (int)(1023 * ASHER_MOTOR_PERCENT)); 
    digitalWrite(MOT_IN2, LOW);
    motorStarted = true;
    Serial.print("Motor started at: ");
    Serial.print(now);
    Serial.print("ms, alarm will stop at: ");
    Serial.println(alarmStartTime + alarmDuration);
  }

  // Figure-8 animation - 4 segments tracing figure-8 pattern
  if (now - lastFlashTime >= 140) {  // Smooth animation timing
    lastFlashTime = now;
    
    // Set all 4 digits with the current frame
    display.setSegments(figure8Frames[figure8Index], 4, 0);
    
    figure8Index = (figure8Index + 1) % numFigure8Frames;
    
    Serial.print("Alarm elapsed: ");
    Serial.print(elapsedTime);
    Serial.print("/");
    Serial.println(alarmDuration);
  }

  // Check if alarm duration has elapsed
  if (elapsedTime >= alarmDuration) {
    Serial.print("Alarm duration elapsed: ");
    Serial.print(elapsedTime);
    Serial.println("ms - STOPPING ALARM");
    stopAlarm();
  }
}
void stopAlarm() {
  Serial.println("in StopAlarm");
  alarmActive = false;
  motorStarted = false; 
  motorRunStartTime = 0;
  
  digitalWrite(MOT_IN1, LOW);
  digitalWrite(MOT_IN2, LOW);
  analogWrite(MOT_IN1, 0);
  
  display.clear();
  timer.reset();
  
  Serial.println("Motor should be OFF now"); 
}

void toggleMode() {
  currentMode = (currentMode == INCREMENT_MIN) ? INCREMENT_SEC : INCREMENT_MIN;
  Serial.print("Mode: ");
  Serial.println(currentMode == INCREMENT_MIN ? "Minutes" : "Seconds");
}