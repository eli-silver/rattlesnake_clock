#include "SerialCommands.h"

SerialCommands::SerialCommands(CountdownTimer& timer)
  : timer(timer), 
    serialBuffer(""), 
    serialCommandReady(false),
    stopAlarmCallback([]() {}),
    getAlarmStatusCallback([]() { return false; }),
    getMotorStatusCallback([]() { return false; }) {}

void SerialCommands::update() {
  readSerial();
  
  if (serialCommandReady) {
    processSerialCommand(serialBuffer);
    serialBuffer = "";
    serialCommandReady = false;
  }
}

void SerialCommands::printWelcomeMessage() {
  Serial.println("Timer Controller Ready");
  Serial.println("Available commands:");
  Serial.println("  TIMER_START");
  Serial.println("  TIMER_STOP");
  Serial.println("  TIMER_RESET");
  Serial.println("  STATUS");
  Serial.println("  SET_TIME <seconds>");
}

void SerialCommands::setStopAlarmCallback(std::function<void()> callback) {
  stopAlarmCallback = callback;
}

void SerialCommands::setAlarmStatusCallback(std::function<bool()> callback) {
  getAlarmStatusCallback = callback;
}

void SerialCommands::setMotorStatusCallback(std::function<bool()> callback) {
  getMotorStatusCallback = callback;
}

void SerialCommands::readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        serialCommandReady = true;
      }
    } else {
      serialBuffer += c;
    }
  }
}

void SerialCommands::processSerialCommand(String command) {
  command.trim();
  command.toUpperCase();
  
  Serial.print("Received command: ");
  Serial.println(command);
  
  if (command == "TIMER_START") {
    bool alarmActive = getAlarmStatusCallback();
    if (alarmActive) {
      Serial.println("Cannot start timer - alarm is active");
    } else if (timer.isRunning()) {
      Serial.println("Timer is already running");
    } else {
      timer.start();
      Serial.println("Timer started via serial command");
    }
  }
  else if (command == "TIMER_STOP" || command == "TIMER_RESET") {
    bool alarmActive = getAlarmStatusCallback();
    if (alarmActive) {
      stopAlarmCallback();
      Serial.println("Alarm stopped via serial command");
    } else {
      timer.reset();
      Serial.println("Timer reset via serial command");
    }
  }
  else if (command == "STATUS") {
    Serial.print("Timer running: ");
    Serial.println(timer.isRunning());
    Serial.print("Remaining time: ");
    Serial.println(timer.getRemainingTime());
    Serial.print("Alarm active: ");
    Serial.println(getAlarmStatusCallback());
    Serial.print("Motor started: ");
    Serial.println(getMotorStatusCallback());
  }
  else if (command.startsWith("SET_TIME ")) {
    int seconds = command.substring(9).toInt();
    bool alarmActive = getAlarmStatusCallback();
    if (seconds > 0 && !timer.isRunning() && !alarmActive) {
      timer.setTime(seconds);
      Serial.print("Timer set to ");
      Serial.print(seconds);
      Serial.println(" seconds");
    } else {
      Serial.println("Cannot set time - timer is running or alarm is active");
    }
  }
  else {
    Serial.println("Unknown command. Available commands:");
    Serial.println("  TIMER_START");
    Serial.println("  TIMER_STOP");
    Serial.println("  TIMER_RESET");
    Serial.println("  STATUS");
    Serial.println("  SET_TIME <seconds>");
  }
}