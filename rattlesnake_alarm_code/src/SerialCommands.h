#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include <Arduino.h>
#include "CountdownTimer.h"

class SerialCommands {
  public:
    SerialCommands(CountdownTimer& timer);
    
    void update();
    void printWelcomeMessage();
    
    // Function pointers for external callbacks
    void setStopAlarmCallback(std::function<void()> callback);
    void setAlarmStatusCallback(std::function<bool()> callback);
    void setMotorStatusCallback(std::function<bool()> callback);

  private:
    CountdownTimer& timer;
    String serialBuffer;
    bool serialCommandReady;
    
    std::function<void()> stopAlarmCallback;
    std::function<bool()> getAlarmStatusCallback;
    std::function<bool()> getMotorStatusCallback;
    
    void readSerial();
    void processSerialCommand(String command);
};

#endif