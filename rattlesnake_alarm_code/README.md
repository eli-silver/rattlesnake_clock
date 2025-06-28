# Rattlesnake Timer - Serial Commands Documentation

## Overview
The Rattlesnake Timer supports serial communication for remote control and monitoring. Commands are sent via USB serial connection at **115200 baud**.

## Setup
1. Connect the Raspberry Pi Pico via USB
2. Open a serial terminal (Arduino IDE Serial Monitor, PuTTY, or terminal)
3. Set baud rate to **115200**
4. Ensure line ending is set to **Newline** or **Carriage Return + Newline**

## Available Commands

### `TIMER_START`
**Description:** Starts the countdown timer from its current value  
**Usage:** `TIMER_START`  
**Response:** 
- Success: `"Timer started via serial command"`
- If alarm active: `"Cannot start timer - alarm is active"`
- If already running: `"Timer is already running"`

**Example:**
```
> TIMER_START
Timer started via serial command
```

---

### `TIMER_STOP`
**Description:** Stops and resets the timer, or stops an active alarm  
**Usage:** `TIMER_STOP`  
**Alias:** `TIMER_RESET` (same functionality)  
**Response:**
- If alarm active: `"Alarm stopped via serial command"`
- If timer running: `"Timer reset via serial command"`

**Example:**
```
> TIMER_STOP
Timer reset via serial command
```

---

### `TIMER_RESET`
**Description:** Same as `TIMER_STOP` - stops and resets the timer  
**Usage:** `TIMER_RESET`  
**Response:** Same as `TIMER_STOP`

---

### `STATUS`
**Description:** Shows current system status  
**Usage:** `STATUS`  
**Response:** Multi-line status report showing:
- Timer running state
- Remaining time (in seconds)
- Alarm active state
- Motor running state

**Example:**
```
> STATUS
Timer running: 1
Remaining time: 23
Alarm active: 0
Motor started: 0
```

---

### `SET_TIME <seconds>`
**Description:** Sets the timer to a specific number of seconds  
**Usage:** `SET_TIME 60` (sets timer to 60 seconds)  
**Parameters:** 
- `<seconds>`: Integer value (must be > 0)
**Restrictions:** Can only be used when timer is stopped and alarm is inactive  
**Response:**
- Success: `"Timer set to X seconds"`
- If invalid: `"Cannot set time - timer is running or alarm is active"`

**Examples:**
```
> SET_TIME 30
Timer set to 30 seconds

> SET_TIME 120
Timer set to 120 seconds

> SET_TIME 5
Timer set to 5 seconds
```

---

## Command Behavior

### Case Insensitive
All commands are case-insensitive:
- `TIMER_START`, `timer_start`, `Timer_Start` all work

### Error Handling
Unknown commands return:
```
Unknown command. Available commands:
  TIMER_START
  TIMER_STOP
  TIMER_RESET
  STATUS
  SET_TIME <seconds>
```

### State-Dependent Behavior
Commands behave differently based on system state:

| State | TIMER_START | TIMER_STOP | SET_TIME |
|-------|-------------|------------|----------|
| Idle (timer stopped) | ✅ Starts timer | ❌ No effect | ✅ Sets time |
| Timer running | ❌ "Already running" | ✅ Stops timer | ❌ "Timer is running" |
| Alarm active | ❌ "Alarm is active" | ✅ Stops alarm | ❌ "Alarm is active" |

## System States

### 1. Idle State
- Timer is stopped and showing set time
- Motor is off
- Display shows time (e.g., `00:10`)

### 2. Timer Running
- Countdown in progress
- Display shows decreasing time
- Motor is off

### 3. Alarm Active
- Timer reached zero
- Motor is running
- Display shows snake animation
- Lasts for 3 seconds, then auto-stops

## Integration Examples

### Python Script Example
```python
import serial
import time

# Connect to Pico
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(2)  # Wait for connection

# Set timer to 30 seconds
ser.write(b'SET_TIME 30\n')
print(ser.readline().decode().strip())

# Start timer
ser.write(b'TIMER_START\n')
print(ser.readline().decode().strip())

# Check status periodically
for i in range(5):
    time.sleep(5)
    ser.write(b'STATUS\n')
    # Read all status lines
    for j in range(4):
        print(ser.readline().decode().strip())

ser.close()
```

### Terminal/Command Line
```bash
# Using screen (macOS/Linux)
screen /dev/ttyACM0 115200

# Or using minicom (Linux)
minicom -D /dev/ttyACM0 -b 115200

# Then type commands directly:
SET_TIME 45
TIMER_START
STATUS
```

## Troubleshooting

### No Response
- Check baud rate is 115200
- Verify correct port/device
- Ensure line endings are enabled
- Try `STATUS` command first

### Commands Not Working
- Ensure commands are typed correctly
- Check for extra spaces
- Try uppercase versions
- Use `STATUS` to check current state

### Connection Issues
- Disconnect/reconnect USB
- Check if device shows up in system
- Try different USB port
- Restart serial terminal

## Debug Output
The system also outputs automatic debug messages:
```
Timer Controller Ready
Available commands:
  TIMER_START
  TIMER_STOP
  TIMER_RESET
  STATUS
  SET_TIME <seconds>

Timer finished - alarm activated!
Motor started at: 45230ms, alarm will stop at: 48230ms
Alarm elapsed: 500/3000
Alarm elapsed: 750/3000
...
Alarm duration elapsed: 3000ms - STOPPING ALARM
in StopAlarm
Motor should be OFF now
```

These messages help monitor system behavior and debug timing issues.