#include <Arduino.h>
#include <TM1637Display.h>
#include "CountdownTimer.h"
#include "Switch.h"
#include "SerialCommands.h"

// Snake animation frames
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
const int alarmDuration = 5000; // 3 seconds
unsigned long lastFlashTime = 0;

unsigned long motorRunStartTime = 0;
const unsigned long MAX_MOTOR_RUN_TIME = 15000; // 5 seconds max safety limit

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
  Serial.begin(9600);

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

        // Trigger appropriate blinking mode
    if (currentMode == INCREMENT_MIN) {
      timer.triggerBlink(BLINK_MINUTES);
      Serial.println("Blinking minutes");
    } else {
      timer.triggerBlink(BLINK_SECONDS);
      Serial.println("Blinking seconds");
    }
    
    encoderMoved = false;
  }
}

void handleAlarm() {
  unsigned long now = millis();
  unsigned long elapsedTime = now - alarmStartTime;

  if (!motorStarted) {
    float ASHER_MOTOR_PERCENT = 0.55;
    analogWrite(MOT_IN1, (int)(1024 * ASHER_MOTOR_PERCENT)); 
    digitalWrite(MOT_IN2, LOW);
    motorStarted = true;
    Serial.print("Motor started at: ");
    Serial.print(now);
    Serial.print("ms, alarm will stop at: ");
    Serial.println(alarmStartTime + alarmDuration);
  }

  // Snake animation
  if (now - lastFlashTime >= 250) {
    lastFlashTime = now;
    uint8_t frame = snakeFrames[snakeIndex];
    uint8_t segments[] = {frame, frame, frame, frame};
    display.setSegments(segments);
    snakeIndex = (snakeIndex + 1) % numSnakeFrames;
    
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