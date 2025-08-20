// ...existing code...
/*
  ESP8266 Smart Home System with Firebase Realtime Database
  (Updated: improved PIR filtering for large/steady motion + dedicated motion LED)
*/

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <DHT.h>
#include "secrets.h"

// Debug control
#define DEBUG true

// Pin definitions (modify as needed for your board)
#define PIR_PIN           14  // D5
#define LDR_PIN           A0  // A0
#define DHT_PIN           12  // D6
#define MOTOR_PWM_PIN     5   // D1 (ENA)
#define MOTOR_IN1_PIN     4   // D2
#define MOTOR_IN2_PIN     0   // D3
#define LED1_PIN          13  // D7
#define LED2_PIN          15  // D8
#define MOTION_LED_PIN    1   // D9/TX (new dedicated motion LED)
#define SWITCH_MOTOR_PIN  2   // D4
#define SWITCH_LEDS_PIN   16  // D0

// Sensor thresholds and settings
#define LDR_THRESHOLD     400   // Below this = dark, turn on lights
#define TEMP_THRESHOLD    33.0  // Above this = hot, turn on motor
#define TEMP_HYSTERESIS   2.0   // Temperature hysteresis
#define DEBOUNCE_DELAY    100   // Switch debounce in ms
#define MAX_MOTOR_PWM     204   // 80% of 255 for motor protection
#define MAX_NOTIFICATIONS_PER_MINUTE 10
#define SENSOR_READ_INTERVAL  200   // PIR/LDR read interval in ms (faster for responsive motion)
#define MOTION_LED_TIMEOUT    10000 // Motion LED auto-off timeout in ms (10 seconds)

// PIR sensor settings (simplified for reliability)
#define PIR_DEBOUNCE_MS       300   // Minimum time between motion triggers
#define PIR_STABLE_TIME_MS    150   // Time PIR must be stable before reading

// DHT sensor setup
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// WiFi and Firebase
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Timing variables (non-blocking)
unsigned long lastSensorRead = 0;
unsigned long lastDHTRead = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastNotificationTime = 0;
unsigned long notificationCount = 0;
unsigned long notificationMinuteStart = 0;

// Sensor states
bool motionDetected = false;
bool lastMotionState = false;
unsigned long lastMotionChangeTime = 0;
bool pirStableState = false;
float temperature = 0;
float humidity = 0;
int ldrValue = 0;
bool autoLightTriggered = false;
bool autoMotorTriggered = false;

// Motion LED state
bool motionLedOn = false;
unsigned long motionLedOffTime = 0;

// Control states
bool motorOn = false;
bool motorsManual = false;
bool led1On = false;
bool led2On = false;
bool ledsManual = false;
bool autoLightEnabled = true;
bool autoMotorEnabled = true;
int motorSpeed = 50; // 0-100%
String motorDirection = "forward";

// Switch states for debouncing
bool lastMotorSwitchState = HIGH;
bool lastLedsSwitchState = HIGH;
unsigned long lastMotorSwitchTime = 0;
unsigned long lastLedsSwitchTime = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(PIR_PIN, INPUT);
  pinMode(DHT_PIN, INPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(MOTION_LED_PIN, OUTPUT);
  pinMode(SWITCH_MOTOR_PIN, INPUT_PULLUP);
  pinMode(SWITCH_LEDS_PIN, INPUT_PULLUP);
  
  // Initialize outputs to OFF
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(MOTION_LED_PIN, LOW);
  
  // Initialize DHT sensor
  dht.begin();
  
  if (DEBUG) Serial.println("ESP8266 Smart Home System Started");
  
  // Connect to WiFi
  connectWiFi();
  
  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  if (DEBUG) Serial.println("ESP8266 Smart Home System Started");
  
  // Initialize Firebase state
  initializeFirebaseState();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  // Reset notification counter every minute
  if (currentTime - notificationMinuteStart > 60000) {
    notificationCount = 0;
    notificationMinuteStart = currentTime;
  }
  
  // Read sensors periodically
  if (currentTime - lastSensorRead > SENSOR_READ_INTERVAL) {
    readSensors();
    lastSensorRead = currentTime;
  }
  
  // Read DHT sensor less frequently
  if (currentTime - lastDHTRead > 10000) { // Every 10 seconds
    readDHTSensor();
    lastDHTRead = currentTime;
  }
  
  // Handle motion LED auto-off timeout
  if (motionLedOn && currentTime >= motionLedOffTime) {
    motionLedOn = false;
    digitalWrite(MOTION_LED_PIN, LOW);
    if (DEBUG) Serial.println("Motion LED auto-turned off");
  }
  
  // Check manual switches with debouncing
  checkManualSwitches();
  
  // Check Firebase for control updates
  checkFirebaseControls();
  
  // Update Firebase with sensor data
  if (currentTime - lastFirebaseUpdate > 5000) { // Every 5 seconds
    updateFirebaseSensors();
    lastFirebaseUpdate = currentTime;
  }
  
  // Process automatic controls
  processAutomaticControls();
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  if (DEBUG) {
    Serial.print("Connecting to WiFi");
  }
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    if (DEBUG) Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    if (DEBUG) {
      Serial.println();
      Serial.println("WiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  } else {
    if (DEBUG) Serial.println("\nWiFi connection failed!");
  }
}

void initializeFirebaseState() {
  // Initialize default control states in Firebase
  Firebase.setBool(firebaseData, "/controls/motor/on", false);
  Firebase.setInt(firebaseData, "/controls/motor/speed", 50);
  Firebase.setString(firebaseData, "/controls/motor/direction", "forward");
  Firebase.setBool(firebaseData, "/controls/motor/manual", false);
  Firebase.setBool(firebaseData, "/controls/motor/auto_enabled", true);
  
  Firebase.setBool(firebaseData, "/controls/led1/on", false);
  Firebase.setBool(firebaseData, "/controls/led1/manual", false);
  Firebase.setBool(firebaseData, "/controls/led1/auto_enabled", true);
  
  Firebase.setBool(firebaseData, "/controls/led2/on", false);
  Firebase.setBool(firebaseData, "/controls/led2/manual", false);
  Firebase.setBool(firebaseData, "/controls/led2/auto_enabled", true);
  
  // Set last seen timestamp
  Firebase.setTimestamp(firebaseData, "/meta/last_seen");
  
  if (DEBUG) Serial.println("Firebase state initialized");
}

void readSensors() {
  // Simple and reliable PIR motion detection
  bool currentMotion = digitalRead(PIR_PIN);
  unsigned long currentTime = millis();
  
  // Check if PIR state changed
  if (currentMotion != lastMotionState) {
    lastMotionState = currentMotion;
    lastMotionChangeTime = currentTime;
    pirStableState = false;
  }
  
  // Wait for stability before acting on the reading
  if (!pirStableState && (currentTime - lastMotionChangeTime) > PIR_STABLE_TIME_MS) {
    pirStableState = true;
    
    if (currentMotion && !motionDetected) {
      // Motion started
      motionDetected = true;
      
      sendNotification("motion", "PIR", "Motion detected", 
                      "{\"lux\":" + String(ldrValue) + "}");
      Firebase.setBool(firebaseData, "/state/motion", true);
      
      // Turn on motion LED
      motionLedOn = true;
      motionLedOffTime = currentTime + MOTION_LED_TIMEOUT;
      digitalWrite(MOTION_LED_PIN, HIGH);
      
      if (DEBUG) Serial.println("Motion detected");
    }
    else if (!currentMotion && motionDetected) {
      // Motion ended
      motionDetected = false;
      Firebase.setBool(firebaseData, "/state/motion", false);
      if (DEBUG) Serial.println("Motion ended");
    }
  }
  
  // Read LDR sensor
  ldrValue = analogRead(LDR_PIN);
}

void readDHTSensor() {
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  
  if (!isnan(newTemp) && !isnan(newHum)) {
    temperature = newTemp;
    humidity = newHum;
    
    if (DEBUG) {
      Serial.printf("DHT - Temp: %.1f°C, Humidity: %.1f%%, LDR: %d\n", 
                   temperature, humidity, ldrValue);
    }
  } else {
    if (DEBUG) Serial.println("DHT read error");
  }
}

void checkManualSwitches() {
  unsigned long currentTime = millis();
  
  // Check motor switch
  bool motorSwitchState = digitalRead(SWITCH_MOTOR_PIN);
  if (motorSwitchState != lastMotorSwitchState && 
      (currentTime - lastMotorSwitchTime) > DEBOUNCE_DELAY) {
    
    if (motorSwitchState == LOW) { // Switch pressed (active low)
      motorsManual = !motorsManual;
      motorOn = motorsManual;
      
      updateMotorControl();
      Firebase.setBool(firebaseData, "/controls/motor/manual", motorsManual);
      Firebase.setBool(firebaseData, "/controls/motor/on", motorOn);
      
      sendNotification("manual", "switch_motor", 
                      motorsManual ? "Motor manually turned ON" : "Motor manually turned OFF",
                      "{}");
    }
    
    lastMotorSwitchState = motorSwitchState;
    lastMotorSwitchTime = currentTime;
  }
  
  // Check LEDs switch
  bool ledsSwitchState = digitalRead(SWITCH_LEDS_PIN);
  if (ledsSwitchState != lastLedsSwitchState && 
      (currentTime - lastLedsSwitchTime) > DEBOUNCE_DELAY) {
    
    if (ledsSwitchState == LOW) { // Switch pressed (active low)
      ledsManual = !ledsManual;
      led1On = ledsManual;
      led2On = ledsManual;
      
      updateLEDControl();
      Firebase.setBool(firebaseData, "/controls/led1/manual", ledsManual);
      Firebase.setBool(firebaseData, "/controls/led1/on", led1On);
      Firebase.setBool(firebaseData, "/controls/led2/manual", ledsManual);
      Firebase.setBool(firebaseData, "/controls/led2/on", led2On);
      
      sendNotification("manual", "switch_leds", 
                      ledsManual ? "LEDs manually turned ON" : "LEDs manually turned OFF",
                      "{}");
    }
    
    lastLedsSwitchState = ledsSwitchState;
    lastLedsSwitchTime = currentTime;
  }
}

void checkFirebaseControls() {
  // Avoid heavy Firebase operations in fast loops - only check occasionally
  static unsigned long lastFirebaseControlCheck = 0;
  if (millis() - lastFirebaseControlCheck < 1000) return; // Check only every 1 second
  lastFirebaseControlCheck = millis();
  
  // Check motor controls
  if (Firebase.getBool(firebaseData, "/controls/motor/on")) {
    bool newMotorOn = firebaseData.boolData();
    if (newMotorOn != motorOn && !motorsManual) {
      motorOn = newMotorOn;
      updateMotorControl();
      sendNotification("manual", "dashboard_user", 
                      motorOn ? "Motor turned ON via dashboard" : "Motor turned OFF via dashboard",
                      "{}");
    }
  }
  
  if (Firebase.getInt(firebaseData, "/controls/motor/speed")) {
    int newSpeed = firebaseData.intData();
    if (newSpeed != motorSpeed && newSpeed >= 0 && newSpeed <= 100) {
      motorSpeed = newSpeed;
      if (motorOn) updateMotorControl();
    }
  }
  
  if (Firebase.getString(firebaseData, "/controls/motor/direction")) {
    String newDirection = firebaseData.stringData();
    if (newDirection != motorDirection && (newDirection == "forward" || newDirection == "reverse")) {
      motorDirection = newDirection;
      if (motorOn) updateMotorControl();
    }
  }
  
  if (Firebase.getBool(firebaseData, "/controls/motor/auto_enabled")) {
    autoMotorEnabled = firebaseData.boolData();
  }
  
  // Check LED1 controls
  if (Firebase.getBool(firebaseData, "/controls/led1/on")) {
    bool newLed1On = firebaseData.boolData();
    if (newLed1On != led1On && !ledsManual) {
      led1On = newLed1On;
      updateLEDControl();
      sendNotification("manual", "dashboard_user", 
                      led1On ? "LED1 turned ON via dashboard" : "LED1 turned OFF via dashboard",
                      "{}");
    }
  }
  
  // Check LED2 controls
  if (Firebase.getBool(firebaseData, "/controls/led2/on")) {
    bool newLed2On = firebaseData.boolData();
    if (newLed2On != led2On && !ledsManual) {
      led2On = newLed2On;
      updateLEDControl();
      sendNotification("manual", "dashboard_user", 
                      led2On ? "LED2 turned ON via dashboard" : "LED2 turned OFF via dashboard",
                      "{}");
    }
  }
  
  if (Firebase.getBool(firebaseData, "/controls/led1/auto_enabled")) {
    autoLightEnabled = firebaseData.boolData();
  }
}

void updateFirebaseSensors() {
  Firebase.setFloat(firebaseData, "/sensors/temperature", temperature);
  Firebase.setFloat(firebaseData, "/sensors/humidity", humidity);
  Firebase.setInt(firebaseData, "/sensors/lux", ldrValue);
  Firebase.setTimestamp(firebaseData, "/meta/last_seen");
  
  if (DEBUG) {
    Serial.printf("Firebase update - Temp: %.1f°C, Humidity: %.1f%%, LDR: %d\n", 
                  temperature, humidity, ldrValue);
  }
}

void processAutomaticControls() {
  // Automatic lighting based on LDR
  if (autoLightEnabled && !ledsManual) {
    if (ldrValue < LDR_THRESHOLD && !autoLightTriggered) {
      led1On = true;
      led2On = true;
      autoLightTriggered = true;
      updateLEDControl();
      Firebase.setBool(firebaseData, "/controls/led1/on", true);
      Firebase.setBool(firebaseData, "/controls/led2/on", true);
      sendNotification("auto", "auto_light", "Lights automatically turned ON (low light detected)",
                      "{\"lux\":" + String(ldrValue) + "}");
    }
    else if (ldrValue > (LDR_THRESHOLD + 50) && autoLightTriggered) { // Hysteresis
      led1On = false;
      led2On = false;
      autoLightTriggered = false;
      updateLEDControl();
      Firebase.setBool(firebaseData, "/controls/led1/on", false);
      Firebase.setBool(firebaseData, "/controls/led2/on", false);
      sendNotification("auto", "auto_light_off", "Lights automatically turned OFF (bright environment)",
                      "{\"lux\":" + String(ldrValue) + "}");
    }
  }
  
  // Automatic motor control based on temperature
  if (autoMotorEnabled && !motorsManual) {
    if (temperature >= TEMP_THRESHOLD && !autoMotorTriggered) {
      motorOn = true;
      autoMotorTriggered = true;
      updateMotorControl();
      Firebase.setBool(firebaseData, "/controls/motor/on", true);
      sendNotification("auto", "auto_temp", "Motor automatically turned ON (high temperature)",
                      "{\"temp\":" + String(temperature) + "}");
    }
    else if (temperature < (TEMP_THRESHOLD - TEMP_HYSTERESIS) && autoMotorTriggered) {
      motorOn = false;
      autoMotorTriggered = false;
      updateMotorControl();
      Firebase.setBool(firebaseData, "/controls/motor/on", false);
      sendNotification("auto", "auto_temp_off", "Motor automatically turned OFF (temperature normal)",
                      "{\"temp\":" + String(temperature) + "}");
    }
  }
}

void updateMotorControl() {
  if (motorOn) {
    // Set direction
    if (motorDirection == "forward") {
      digitalWrite(MOTOR_IN1_PIN, HIGH);
      digitalWrite(MOTOR_IN2_PIN, LOW);
    } else {
      digitalWrite(MOTOR_IN1_PIN, LOW);
      digitalWrite(MOTOR_IN2_PIN, HIGH);
    }
    
    // Set speed (convert 0-100% to 0-MAX_MOTOR_PWM)
    int pwmValue = map(motorSpeed, 0, 100, 0, MAX_MOTOR_PWM);
    analogWrite(MOTOR_PWM_PIN, pwmValue);
    
    if (DEBUG) Serial.printf("Motor ON: %s, Speed: %d%% (PWM: %d)\n", 
                            motorDirection.c_str(), motorSpeed, pwmValue);
  } else {
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    analogWrite(MOTOR_PWM_PIN, 0);
    
    if (DEBUG) Serial.println("Motor OFF");
  }
}

void updateLEDControl() {
  digitalWrite(LED1_PIN, led1On ? HIGH : LOW);
  digitalWrite(LED2_PIN, led2On ? HIGH : LOW);
  
  if (DEBUG) Serial.printf("LED1: %s, LED2: %s\n", led1On ? "ON" : "OFF", led2On ? "ON" : "OFF");
}

void sendNotification(String type, String actor, String message, String details) {
  // Rate limiting
  if (notificationCount >= MAX_NOTIFICATIONS_PER_MINUTE) {
    return;
  }
  
  // Simple debouncing for motion notifications
  unsigned long currentTime = millis();
  if (currentTime - lastNotificationTime < 2000 && type == "motion") {
    return;
  }
  
  String notificationPath = "/notifications/" + String(currentTime);
  
  Firebase.setTimestamp(firebaseData, notificationPath + "/ts");
  Firebase.setString(firebaseData, notificationPath + "/type", type);
  Firebase.setString(firebaseData, notificationPath + "/actor", actor);
  Firebase.setString(firebaseData, notificationPath + "/message", message);
  Firebase.setString(firebaseData, notificationPath + "/details", details);
  
  notificationCount++;
  lastNotificationTime = currentTime;
  
  if (DEBUG) {
    Serial.printf("Notification: %s\n", message.c_str());
  }
}