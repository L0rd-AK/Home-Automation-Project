# ESP8266 Smart Home System

A comprehensive IoT smart home system using ESP8266, multiple sensors, actuators, and Firebase Realtime Database for remote monitoring and control.

## 🚀 Features

### Hardware Components
- **ESP8266** (NodeMCU/WeMos D1 Mini)
- **PIR Motion Sensor** - Automatic motion detection and notifications
- **LDR Light Sensor** - Automatic lighting control based on ambient light
- **DHT11** - Temperature and humidity monitoring with automatic fan control
- **L298N Motor Driver + DC Motor** - PWM speed control and direction control
- **2 LEDs** - Manual and automatic lighting control
- **2 Manual Switches** - Physical override controls
- **Firebase Realtime Database** - Cloud data storage and real-time synchronization

### Software Features
- **Real-time sensor monitoring** with automatic data logging
- **Automatic control systems**:
  - Auto-lighting when ambient light is low
  - Auto-motor activation when temperature exceeds threshold
  - Motion detection with instant notifications
- **Manual override controls** via physical switches and web dashboard
- **Rate-limited notifications** to prevent spam
- **Hysteresis control** to prevent frequent switching
- **WiFi reconnection** with exponential backoff
- **Non-blocking code** using millis() timers
- **Comprehensive web dashboard** for remote control

## 📋 Hardware Setup

### Pin Configuration (ESP8266)
```cpp
// Sensors
PIR Motion Sensor    -> D5 (GPIO14)
LDR Light Sensor     -> A0 (Analog)
DHT11 Temp/Humidity  -> D6 (GPIO12)

// Motor Control (L298N)
ENA (PWM Enable)     -> D1 (GPIO5)
IN1 (Direction)      -> D2 (GPIO4)
IN2 (Direction)      -> D3 (GPIO0)

// LEDs
LED1                 -> D7 (GPIO13)
LED2                 -> D8 (GPIO15)

// Manual Switches
Motor Switch         -> D4 (GPIO2) + GND
LEDs Switch          -> D0 (GPIO16) + GND
```

### 🔌 Detailed Wiring Connections

#### Power Connections
```
ESP8266 NodeMCU          Components
================         ==========
3V3 ------------------- VCC (DHT11, PIR Sensor)
GND ------------------- GND (All components)
```

#### Sensor Connections
```
ESP8266 Pin             Sensor Pin          Details
=============           ===========         =======
D5 (GPIO14) ----------- PIR Signal         Digital input, 3.3V logic
A0 -------------------- LDR Signal         Analog input (0-1023)
                        (with 10kΩ pull-down resistor)
D6 (GPIO12) ----------- DHT11 Data         Digital input, 4.7kΩ pull-up resistor
```

#### Motor Driver (L298N) Connections
```
ESP8266 Pin             L298N Pin          Details
=============           ==========         =======
D1 (GPIO5) ------------ ENA               PWM control for motor speed
D2 (GPIO4) ------------ IN1               Motor direction control
D3 (GPIO0) ------------ IN2               Motor direction control
                        OUT1, OUT2 ------- DC Motor terminals
                        +12V ------------- External 12V power supply
                        GND -------------- Common ground with ESP8266
```

#### LED Connections
```
ESP8266 Pin             LED Pin            Details
=============           ========           =======
D7 (GPIO13) ----------- LED1 Anode (+)    220Ω current limiting resistor
D8 (GPIO15) ----------- LED2 Anode (+)    220Ω current limiting resistor
                        LED Cathode (-) --- GND
```

#### Manual Switch Connections
```
ESP8266 Pin             Switch Pin         Details
=============           ===========        =======
D4 (GPIO2) ------------ Switch 1 (Motor)  One terminal to GPIO, other to GND
D0 (GPIO16) ----------- Switch 2 (LEDs)   One terminal to GPIO, other to GND
```

### 📐 Wiring Diagram

```
                    ESP8266 NodeMCU
                ┌─────────────────────┐
                │                     │
                │ 3V3 ────────────────┼─── VCC (DHT11, PIR)
                │                     │
                │ GND ────────────────┼─── GND (All Components)
                │                     │
                │ D5 (GPIO14) ────────┼─── PIR Signal
                │                     │
                │ A0 ─────────────────┼─── LDR + 10kΩ to GND
                │                     │
                │ D6 (GPIO12) ────────┼─── DHT11 Data + 4.7kΩ to 3V3
                │                     │
                │ D1 (GPIO5) ─────────┼─── L298N ENA (PWM)
                │                     │
                │ D2 (GPIO4) ─────────┼─── L298N IN1
                │                     │
                │ D3 (GPIO0) ─────────┼─── L298N IN2
                │                     │
                │ D7 (GPIO13) ────────┼─── LED1 + 220Ω
                │                     │
                │ D8 (GPIO15) ────────┼─── LED2 + 220Ω
                │                     │
                │ D4 (GPIO2) ─────────┼─── Motor Switch ── GND
                │                     │
                │ D0 (GPIO16) ────────┼─── LEDs Switch ── GND
                └─────────────────────┘

                    L298N Motor Driver
                ┌─────────────────────┐
                │                     │
                │ ENA ←── D1 (GPIO5)  │
                │ IN1 ←── D2 (GPIO4)  │
                │ IN2 ←── D3 (GPIO0)  │
                │                     │
                │ OUT1 ───────────────┼─── DC Motor Terminal 1
                │ OUT2 ───────────────┼─── DC Motor Terminal 2
                │                     │
                │ +12V ←── External Power Supply
                │ GND ←── Common Ground
                └─────────────────────┘

                    Component Details
                ┌─────────────────────┐
                │                     │
                │ PIR Sensor:         │
                │ • VCC: 3.3V         │
                │ • GND: Common       │
                │ • Signal: D5        │
                │                     │
                │ DHT11:              │
                │ • VCC: 3.3V         │
                │ • GND: Common       │
                │ • Data: D6 + 4.7kΩ │
                │                     │
                │ LDR:                │
                │ • Signal: A0        │
                │ • 10kΩ to GND      │
                │                     │
                │ LEDs:               │
                │ • Anode: D7/D8      │
                │ • 220Ω resistor     │
                │ • Cathode: GND      │
                │                     │
                │ Switches:           │
                │ • One terminal: D4/D0
                │ • Other terminal: GND
                └─────────────────────┘
```

### ⚠️ Important Wiring Notes

1. **Power Supply**: Use a stable 3.3V supply for ESP8266 and sensors
2. **Ground Connection**: Ensure all components share the same ground
3. **Pull-up Resistors**: DHT11 requires 4.7kΩ pull-up resistor to 3V3
4. **Pull-down Resistor**: LDR needs 10kΩ pull-down resistor to GND
5. **Current Limiting**: LEDs require 220Ω resistors to limit current
6. **Motor Power**: L298N needs separate 12V supply for motor operation
7. **Wire Length**: Keep sensor wires as short as possible to reduce noise
8. **Shielding**: Consider using shielded cables for analog sensors in noisy environments

### 🔧 Component Specifications

- **ESP8266**: 3.3V operation, 80MHz clock, 4MB flash
- **PIR Sensor**: 3.3V-5V, 110° detection angle, 3-7m range
- **DHT11**: 3.3V-5V, -40°C to +80°C, ±2°C accuracy
- **LDR**: 5mm diameter, 10kΩ at 10 lux, 1kΩ at 1000 lux
- **L298N**: 5V logic, 12V motor supply, 2A per channel
- **DC Motor**: 12V, 100-200mA typical current draw
- **LEDs**: 3.3V forward voltage, 20mA typical current

## 🛠️ Software Setup

### 1. Arduino IDE Setup
1. Install ESP8266 board package:
   - File → Preferences → Additional Board Manager URLs
   - Add: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Tools → Board → Boards Manager → Search "ESP8266" → Install

2. Install required libraries via Library Manager:
   ```
   - FirebaseESP8266 by Mobizt
   - DHT sensor library by Adafruit
   - ArduinoJson (dependency)
   ```

### 2. Firebase Setup
1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Create new project or select existing
3. Enable **Realtime Database**:
   - Database → Create Database → Start in test mode
   - Set rules (for development):
     ```json
     {
       "rules": {
         ".read": true,
         ".write": true
       }
     }
     ```
4. Get configuration:
   - Project Settings → General → Your Apps → Web App
   - Copy `apiKey` and `databaseURL`
5. Enable **Authentication**:
   - Authentication → Sign-in method → Email/Password → Enable
   - Users → Add user (email/password for ESP8266)

### 3. Code Configuration
1. Open `Code_ESP32_Firebase/secrets.h`
2. Replace placeholders with your actual values:
   ```cpp
   #define WIFI_SSID     "Your_WiFi_Name"
   #define WIFI_PASS     "Your_WiFi_Password"
   #define API_KEY       "Your_Firebase_API_Key"
   #define DATABASE_URL  "https://your-project-default-rtdb.region.firebasedatabase.app/"
   #define USER_EMAIL    "your-email@example.com"
   #define USER_PASSWORD "your-password"
   ```
3. Upload code to ESP8266

### 4. Dashboard Setup
1. Open `dashboard.html`
2. Update Firebase configuration in the JavaScript section:
   ```javascript
   const firebaseConfig = {
       apiKey: "your-api-key",
       authDomain: "your-project.firebaseapp.com",
       databaseURL: "https://your-project-default-rtdb.region.firebasedatabase.app/",
       projectId: "your-project-id",
       // ... other config values
   };
   ```
3. Open `dashboard.html` in a web browser

## 📊 Database Structure

The system uses the following Firebase Realtime Database structure:

```json
{
  "sensors": {
    "temperature": 25.6,
    "humidity": 60.2,
    "lux": 450,
    "motion": false
  },
  "controls": {
    "motor": {
      "on": false,
      "speed": 50,
      "direction": "forward",
      "manual": false,
      "auto_enabled": true
    },
    "leds": {
      "on": false,
      "manual": false,
      "auto_enabled": true
    }
  },
  "notifications": {
    "timestamp_id": {
      "ts": 1692345600000,
      "type": "motion",
      "actor": "PIR",
      "message": "Motion detected in living room",
      "details": "{\"lux\":450,\"temp\":25.6}"
    }
  },
  "meta": {
    "last_seen": 1692345600000
  }
}
```

## 🎯 System Behavior

### Automatic Controls
1. **Motion Detection**: PIR sensor triggers notification and updates `/state/motion`
2. **Auto-Lighting**: When LDR < 400, automatically turn on LEDs
3. **Auto-Motor**: When temperature ≥ 30°C, automatically start motor
4. **Hysteresis**: Prevents rapid switching with temperature and light thresholds

### Manual Overrides
1. **Physical Switches**: Toggle motor and LEDs with debounced switches
2. **Dashboard Controls**: Real-time web interface for all controls
3. **Manual Priority**: Manual controls override automatic systems

### Safety Features
- **Rate Limiting**: Max 10 notifications per minute
- **Motor Protection**: PWM limited to 80% (204/255)
- **Debouncing**: 100ms debounce for switches
- **WiFi Reconnection**: Automatic reconnection with backoff
- **Error Logging**: Serial debug output for troubleshooting

## 🧪 Testing

### Test Cases
1. **Motion Test**: Wave hand in front of PIR → Check dashboard notifications
2. **Light Test**: Cover LDR → LEDs should auto-turn on → Dashboard updates
3. **Temperature Test**: Heat DHT11 → Motor should auto-start → Check PWM
4. **Manual Test**: Toggle physical switches → Check dashboard reflects changes
5. **Rate Limit Test**: Rapid PIR triggers → Notifications should be limited
6. **Dashboard Test**: Control motor/LEDs via web → ESP8266 should respond

### Troubleshooting
- **WiFi Issues**: Check credentials, signal strength, router settings
- **Firebase Issues**: Verify API key, database URL, authentication
- **Sensor Issues**: Check wiring, power supply, pull-up/pull-down resistors
- **Motor Issues**: Verify L298N connections, power supply voltage
- **Dashboard Issues**: Check browser console, Firebase config, database rules

## 📝 Customization

### Modifying Thresholds
```cpp
#define LDR_THRESHOLD     400   // Light level threshold
#define TEMP_THRESHOLD    30.0  // Temperature threshold (°C)
#define TEMP_HYSTERESIS   2.0   // Temperature hysteresis
#define MAX_MOTOR_PWM     204   // Motor PWM limit (80%)
```

### Changing Pin Assignments
Update the pin definitions at the top of the `.ino` file:
```cpp
#define PIR_PIN           14  // Change to your preferred pin
#define DHT_PIN           12  // Change to your preferred pin
// ... etc
```

### Adding New Sensors
1. Define new pins in the pin definitions section
2. Add sensor reading code in `readSensors()` function
3. Update Firebase paths in `updateFirebaseSensors()`
4. Add dashboard elements and JavaScript listeners

## 📄 License

This project is open-source and available under the MIT License.

## 🤝 Contributing

Feel free to submit issues, fork the repository, and create pull requests for any improvements.

## 🔗 Dependencies

- ESP8266 Arduino Core
- FirebaseESP8266 Library
- DHT Sensor Library
- ArduinoJson Library
- Firebase Web SDK (CDN)

---

**Created by**: Smart Home IoT Project  
**Last Updated**: August 2025  
**Version**: 1.0.0
