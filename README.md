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

### Wiring Diagram
```
ESP8266 NodeMCU          Components
================         ==========
3V3 ------------------- VCC (DHT11, PIR)
GND ------------------- GND (All components)
D5 (GPIO14) ----------- PIR Signal
A0 -------------------- LDR Signal (with pull-down resistor)
D6 (GPIO12) ----------- DHT11 Data
D1 (GPIO5) ------------ L298N ENA
D2 (GPIO4) ------------ L298N IN1
D3 (GPIO0) ------------ L298N IN2
D7 (GPIO13) ----------- LED1 Anode (+)
D8 (GPIO15) ----------- LED2 Anode (+)
D4 (GPIO2) ------------ Switch 1 (Motor)
D0 (GPIO16) ----------- Switch 2 (LEDs)

L298N Motor Driver:
OUT1, OUT2 ------------ DC Motor
+12V ------------------- External Power Supply
GND -------------------- Common Ground
```

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
