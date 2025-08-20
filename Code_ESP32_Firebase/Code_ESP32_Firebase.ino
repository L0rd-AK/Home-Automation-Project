/*
  ESP8266 Smart Home - Corrected & Complete Version
  - Safe pin assignments for NodeMCU
  - Debounced manual switches
  - PIR + LDR (auto lights)
  - DHT11 temperature/humidity
  - Motor ON/OFF (relay-style) + dashboard control
  - Firebase RTDB sync + notifications with rate limit
  - No use of GPIO1 (TX) / GPIO3 (RX) / GPIO9&10 (flash) / pins that break boot mode
*/

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <DHT.h>
#include "secrets.h" // expected to define WIFI_SSID, WIFI_PASS, DATABASE_URL, API_KEY

// ---------------- DEBUG ----------------
#define DEBUG true

// ---------------- Pin Mapping (safe) ----------------
// Use pins that don't interfere with boot or flash
#define PIR_PIN            D5   // GPIO14
#define LDR_PIN            A0   // analog
#define DHT_PIN            D6   // GPIO12
#define DHT_TYPE           DHT11

#define LED1_PIN           D1   // GPIO5  (Main lights)
#define LED2_PIN           D2   // GPIO4  (Secondary light)
#define MOTION_LED_PIN     D8   // GPIO15 (status LED, OK as output - must be LOW at boot, we'll leave LOW)
#define MOTOR_RELAY_PIN    D4   // GPIO2  (relay to control motor power) - GPIO2 must be HIGH at boot; we'll initialize HIGH (relay OFF if active LOW)
#define SWITCH_MOTOR_PIN   D7   // GPIO13 (manual motor toggle)
#define SWITCH_LEDS_PIN    D3   // GPIO0  <--- WARNING: GPIO0 must be HIGH at boot. We'll avoid holding it LOW at boot. If you prefer, pick another pin. (See note)

// If you prefer to avoid D3 (GPIO0) altogether (recommended), change SWITCH_LEDS_PIN to a different pin such as D6/D7 depending on availability.

// ---------------- Settings ----------------
#define LDR_THRESHOLD           400   // calibrate in your environment
#define LDR_HYSTERESIS          50
#define TEMP_THRESHOLD          33.0
#define TEMP_HYSTERESIS         2.0
#define DEBOUNCE_DELAY_MS       80
#define SENSOR_READ_INTERVAL_MS 200
#define DHT_READ_INTERVAL_MS    10000L
#define FIREBASE_UPDATE_MS      5000L
#define MOTION_LED_TIMEOUT_MS   8000UL
#define NOTIFICATION_MINUTE_MS  60000UL
#define MAX_NOTIFICATIONS_PER_MIN 10
#define MOTION_DEBOUNCE_MS      150

// ---------------- Globals ----------------
DHT dht(DHT_PIN, DHT_TYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSensorRead = 0;
unsigned long lastDHTRead = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastNotificationTime = 0;
unsigned long notificationMinuteStart = 0;
unsigned int notificationCount = 0;

// sensor state
int ldrValue = 0;
float temperatureC = 0.0f;
float humidityPct = 0.0f;

// motion state
bool motionDetected = false;
bool lastRawMotion = false;
unsigned long lastMotionChange = 0;
bool pirStableState = false;
unsigned long motionLedOffAt = 0;

// control state
bool led1On = false;
bool led2On = false;
bool ledsManual = false;
bool autoLightEnabled = true;

bool motorOn = false;
bool motorsManual = false;
bool autoMotorEnabled = true;

// switch debouncing
bool lastMotorSwitchState = HIGH;
bool lastLedsSwitchState = HIGH;
unsigned long lastMotorSwitchTime = 0;
unsigned long lastLedsSwitchTime = 0;

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (DEBUG) {
    Serial.print("Connecting WiFi");
  }
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    if (DEBUG) Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (DEBUG) {
      Serial.println();
      Serial.print("WiFi connected, IP: ");
      Serial.println(WiFi.localIP());
    }
  } else {
    if (DEBUG) Serial.println("\nWiFi connect failed (continue, will retry later).");
  }
}

void initializeFirebaseState() {
  // update some defaults (with error checking)
  if (!Firebase.setBool(fbdo, "/controls/motor/on", false)) {
    if (DEBUG) Serial.println("FB set motor default failed: " + fbdo.errorReason());
  }
  if (!Firebase.setBool(fbdo, "/controls/led1/on", false)) {
    if (DEBUG) Serial.println("FB set led1 default failed: " + fbdo.errorReason());
  }
  if (!Firebase.setBool(fbdo, "/controls/led2/on", false)) {
    if (DEBUG) Serial.println("FB set led2 default failed: " + fbdo.errorReason());
  }
  Firebase.setTimestamp(fbdo, "/meta/last_seen");
  if (DEBUG) Serial.println("Firebase default state initialized");
}

void sendNotification(const String &type, const String &actor, const String &message, const String &details) {
  unsigned long now = millis();

  // reset per-minute counter
  if (now - notificationMinuteStart >= NOTIFICATION_MINUTE_MS) {
    notificationMinuteStart = now;
    notificationCount = 0;
  }

  if (notificationCount >= MAX_NOTIFICATIONS_PER_MIN) {
    if (DEBUG) Serial.println("Notification suppressed (rate limit)");
    return;
  }

  // motion debounce for notifications
  if (type == "motion" && (now - lastNotificationTime) < 2000) {
    if (DEBUG) Serial.println("Motion notification suppressed (short interval)");
    return;
  }

  String path = "/notifications/" + String(now);
  if (!Firebase.setTimestamp(fbdo, path + "/ts")) {
    if (DEBUG) Serial.println("FB notif ts failed: " + fbdo.errorReason());
  }
  Firebase.setString(fbdo, path + "/type", type);
  Firebase.setString(fbdo, path + "/actor", actor);
  Firebase.setString(fbdo, path + "/message", message);
  Firebase.setString(fbdo, path + "/details", details);

  notificationCount++;
  lastNotificationTime = now;

  if (DEBUG) Serial.println("Notification: " + message);
}

void updateFirebaseSensors() {
  // Write sensor values (non-blocking style; check errors)
  if (!Firebase.setFloat(fbdo, "/sensors/temperature", temperatureC) && DEBUG) {
    Serial.println("FB temp write error: " + fbdo.errorReason());
  }
  if (!Firebase.setFloat(fbdo, "/sensors/humidity", humidityPct) && DEBUG) {
    Serial.println("FB humidity write error: " + fbdo.errorReason());
  }
  if (!Firebase.setInt(fbdo, "/sensors/lux", ldrValue) && DEBUG) {
    Serial.println("FB LDR write error: " + fbdo.errorReason());
  }
  Firebase.setTimestamp(fbdo, "/meta/last_seen");
  if (DEBUG) Serial.printf("FB update done: T=%.1f H=%.1f LDR=%d\n", temperatureC, humidityPct, ldrValue);
}

void updateLEDOutputs() {
  digitalWrite(LED1_PIN, led1On ? HIGH : LOW);
  digitalWrite(LED2_PIN, led2On ? HIGH : LOW);
  if (DEBUG) Serial.printf("LEDs -> 1:%s 2:%s\n", led1On ? "ON" : "OFF", led2On ? "ON" : "OFF");
}

void updateMotorOutput() {
  // Relay control: assume active LOW relay module; set HIGH for OFF by default
  // If your relay is active HIGH, invert logic here.
  if (motorOn) digitalWrite(MOTOR_RELAY_PIN, LOW); // ON
  else digitalWrite(MOTOR_RELAY_PIN, HIGH); // OFF

  if (DEBUG) Serial.printf("Motor -> %s\n", motorOn ? "ON" : "OFF");
}

void readDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperatureC = t;
    humidityPct = h;
    if (DEBUG) Serial.printf("DHT read: %.1f C, %.1f %%\n", temperatureC, humidityPct);
  } else {
    if (DEBUG) Serial.println("DHT read failed");
  }
}

// PIR handling with simple stability time
void readPIR() {
  bool raw = digitalRead(PIR_PIN);
  unsigned long now = millis();

  if (raw != lastRawMotion) {
    lastRawMotion = raw;
    lastMotionChange = now;
    pirStableState = false;
  }

  if (!pirStableState && (now - lastMotionChange) >= MOTION_DEBOUNCE_MS) {
    pirStableState = true;
    if (raw && !motionDetected) {
      // motion started
      motionDetected = true;
      // write state + send notification
      Firebase.setBool(fbdo, "/state/motion", true);
      sendNotification("motion", "PIR", "Motion detected", "{\"lux\":" + String(ldrValue) + "}");
      // turn on motion LED and schedule off
      digitalWrite(MOTION_LED_PIN, HIGH);
      motionLedOffAt = now + MOTION_LED_TIMEOUT_MS;
      if (DEBUG) Serial.println("Motion detected (stable)");
    } else if (!raw && motionDetected) {
      // motion ended
      motionDetected = false;
      Firebase.setBool(fbdo, "/state/motion", false);
      if (DEBUG) Serial.println("Motion ended (stable)");
    }
  }
}

void checkSwitches() {
  unsigned long now = millis();
  // Motor switch: active LOW
  bool motorReading = (digitalRead(SWITCH_MOTOR_PIN) == LOW);
  if (motorReading != lastMotorSwitchState) {
    lastMotorSwitchTime = now;
  }
  if ((now - lastMotorSwitchTime) > DEBOUNCE_DELAY_MS) {
    if (motorReading && !motorsManual) {
      // button pressed (edge), toggle manual mode and motor state
      motorsManual = !motorsManual;
      motorOn = motorsManual;
      updateMotorOutput();
      Firebase.setBool(fbdo, "/controls/motor/manual", motorsManual);
      Firebase.setBool(fbdo, "/controls/motor/on", motorOn);
      sendNotification("manual", "switch_motor", motorsManual ? "Motor manually turned ON" : "Motor manually turned OFF", "{}");
      if (DEBUG) Serial.printf("Motor switch toggled -> manual:%s motor:%s\n", motorsManual ? "Y":"N", motorOn ? "ON":"OFF");
    }
  }
  lastMotorSwitchState = motorReading ? HIGH : LOW; // keep previous stable state (we track button presses as HIGH/LOW doesn't matter exact value)

  // LEDs switch: active LOW
  bool ledsReading = (digitalRead(SWITCH_LEDS_PIN) == LOW);
  if (ledsReading != lastLedsSwitchState) {
    lastLedsSwitchTime = now;
  }
  if ((now - lastLedsSwitchTime) > DEBOUNCE_DELAY_MS) {
    if (ledsReading && !ledsManual) {
      // toggle manual mode
      ledsManual = !ledsManual;
      led1On = ledsManual;
      led2On = ledsManual;
      updateLEDOutputs();
      Firebase.setBool(fbdo, "/controls/led1/manual", ledsManual);
      Firebase.setBool(fbdo, "/controls/led1/on", led1On);
      Firebase.setBool(fbdo, "/controls/led2/manual", ledsManual);
      Firebase.setBool(fbdo, "/controls/led2/on", led2On);
      sendNotification("manual", "switch_leds", ledsManual ? "LEDs manually turned ON" : "LEDs manually turned OFF", "{}");
      if (DEBUG) Serial.printf("LEDs switch toggled -> manual:%s\n", ledsManual ? "Y":"N");
    }
  }
  lastLedsSwitchState = ledsReading ? HIGH : LOW;
}

void checkFirebaseControls() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 1000) return; // check every second
  lastCheck = millis();

  // motor on/off from dashboard
  if (Firebase.getBool(fbdo, "/controls/motor/on")) {
    bool v = fbdo.boolData();
    if (v != motorOn && !motorsManual) {
      motorOn = v;
      updateMotorOutput();
      sendNotification("manual", "dashboard_user", motorOn ? "Motor turned ON via dashboard" : "Motor turned OFF via dashboard", "{}");
    }
  } else {
    if (DEBUG) Serial.println("FB get /controls/motor/on failed: " + fbdo.errorReason());
  }

  // led1
  if (Firebase.getBool(fbdo, "/controls/led1/on")) {
    bool v = fbdo.boolData();
    if (v != led1On && !ledsManual) {
      led1On = v;
      updateLEDOutputs();
      sendNotification("manual", "dashboard_user", led1On ? "LED1 ON via dashboard" : "LED1 OFF via dashboard", "{}");
    }
  } else {
    // optional debug
  }

  // led2
  if (Firebase.getBool(fbdo, "/controls/led2/on")) {
    bool v = fbdo.boolData();
    if (v != led2On && !ledsManual) {
      led2On = v;
      updateLEDOutputs();
      sendNotification("manual", "dashboard_user", led2On ? "LED2 ON via dashboard" : "LED2 OFF via dashboard", "{}");
    }
  }

  // auto toggles
  if (Firebase.getBool(fbdo, "/controls/led1/auto_enabled")) {
    autoLightEnabled = fbdo.boolData();
  }
  if (Firebase.getBool(fbdo, "/controls/motor/auto_enabled")) {
    autoMotorEnabled = fbdo.boolData();
  }
}

void processAutomaticControls() {
  // auto lights based on LDR
  if (autoLightEnabled && !ledsManual) {
    if (ldrValue < LDR_THRESHOLD && ! (led1On && led2On) ) {
      led1On = true; led2On = true;
      updateLEDOutputs();
      Firebase.setBool(fbdo, "/controls/led1/on", true);
      Firebase.setBool(fbdo, "/controls/led2/on", true);
      sendNotification("auto", "auto_light", "Lights turned ON (low lux)", "{\"lux\":" + String(ldrValue) + "}");
    } else if (ldrValue > (LDR_THRESHOLD + LDR_HYSTERESIS) && (led1On && led2On)) {
      led1On = false; led2On = false;
      updateLEDOutputs();
      Firebase.setBool(fbdo, "/controls/led1/on", false);
      Firebase.setBool(fbdo, "/controls/led2/on", false);
      sendNotification("auto", "auto_light_off", "Lights turned OFF (bright)", "{\"lux\":" + String(ldrValue) + "}");
    }
  }

  // auto motor control based on temperature
  if (autoMotorEnabled && !motorsManual) {
    if (temperatureC >= TEMP_THRESHOLD && !motorOn) {
      motorOn = true;
      updateMotorOutput();
      Firebase.setBool(fbdo, "/controls/motor/on", true);
      sendNotification("auto", "auto_temp", "Motor ON (high temp)", "{\"temp\":" + String(temperatureC) + "}");
    } else if (temperatureC < (TEMP_THRESHOLD - TEMP_HYSTERESIS) && motorOn) {
      motorOn = false;
      updateMotorOutput();
      Firebase.setBool(fbdo, "/controls/motor/on", false);
      sendNotification("auto", "auto_temp_off", "Motor OFF (temp normal)", "{\"temp\":" + String(temperatureC) + "}");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // Initialize pins
  pinMode(PIR_PIN, INPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(MOTION_LED_PIN, OUTPUT);
  pinMode(MOTOR_RELAY_PIN, OUTPUT);

  // Switches (wired to GND, so use INPUT_PULLUP)
  pinMode(SWITCH_MOTOR_PIN, INPUT_PULLUP);
  pinMode(SWITCH_LEDS_PIN, INPUT_PULLUP);

  // Ensure outputs default to safe states
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(MOTION_LED_PIN, LOW);
  // For relay: set HIGH for OFF if relay is active LOW. If your relay is active HIGH, invert logic here.
  digitalWrite(MOTOR_RELAY_PIN, HIGH);

  dht.begin();

  // WiFi
  connectWiFi();

  // Firebase config (if using auth tokens set them in secrets.h and uncomment)
  // Using simple host/auth pair:
  Firebase.begin(DATABASE_URL, API_KEY);
  Firebase.reconnectWiFi(true);

  // initialize defaults on DB (best-effort)
  initializeFirebaseState();

  notificationMinuteStart = millis();
  lastSensorRead = millis();
  lastDHTRead = millis();
  lastFirebaseUpdate = millis();
  if (DEBUG) Serial.println("Setup complete");
}

void loop() {
  unsigned long now = millis();

  // Regular sensor read
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = now;
    ldrValue = analogRead(LDR_PIN);
    readPIR();
  }

  // DHT slower
  if (now - lastDHTRead >= DHT_READ_INTERVAL_MS) {
    lastDHTRead = now;
    readDHT();
  }

  // Handle motion LED auto-off
  if (motionLedOffAt != 0 && now >= motionLedOffAt) {
    digitalWrite(MOTION_LED_PIN, LOW);
    motionLedOffAt = 0;
    if (DEBUG) Serial.println("Motion LED auto-off");
  }

  // Switch handling (debounced)
  checkSwitches();

  // Read firebase control values (throttled)
  checkFirebaseControls();

  // Automatic control logic
  processAutomaticControls();

  // Periodically update sensors to Firebase
  if (now - lastFirebaseUpdate >= FIREBASE_UPDATE_MS) {
    lastFirebaseUpdate = now;
    updateFirebaseSensors();
  }

  // maintain notification minute counter (it resets inside sendNotification)
  // small yield
  delay(10);
}
