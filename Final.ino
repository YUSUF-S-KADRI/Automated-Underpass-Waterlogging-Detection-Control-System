/********************************************************
 SMART UNDERPASS WATERLOGGING SYSTEM
 FIXED: RAIN-TRIGGERED DRAINAGE + AUTO LOGIC + BLYNK ALERTS
 ESP32 + BLYNK + LCD + MANUAL DRAINAGE
********************************************************/

#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= WIFI DETAILS =================
char ssid[] = "";
char pass[] = "";

// ================= PIN DEFINITIONS =================
#define rainPin 27
#define trigPin 5
#define echoPin 18
#define motorRelay 26  // Drainage Motor
#define pumpRelay 25   // Water Pump
#define greenLED 14
#define yellowLED 12
#define redLED 13
#define buzzer 33
#define servoPin 19

// ================= RELAY LOGIC =================
#define RELAY_ON  LOW  
#define RELAY_OFF HIGH 

// ================= OBJECTS & VARS =================
Servo barricade;
LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3F(0x3F, 16, 2);
LiquidCrystal_I2C* lcd = nullptr;

bool lcdReady = false;
BlynkTimer timer;
long duration = 0;
int distance = 0;
int waterLevel = 0;
int maxHeight = 20;
bool pumpState = false;
bool motorState = false; 
bool autoMode = true;
int currentServoAngle = 90;

// ================= I2C SCANNER & LCD =================
bool i2cDeviceExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void initLCD() {
  if (i2cDeviceExists(0x27)) { lcd = &lcd27; lcdReady = true; }
  else if (i2cDeviceExists(0x3F)) { lcd = &lcd3F; lcdReady = true; }
  
  if (lcdReady) {
    lcd->init();
    lcd->backlight();
    lcd->setCursor(0,0);
    lcd->print("System Online");
  }
}

// ================= BLYNK CONTROLS =================
BLYNK_WRITE(V4) { // Manual Pump
  if (!autoMode) {
    pumpState = param.asInt();
    digitalWrite(pumpRelay, pumpState ? RELAY_ON : RELAY_OFF);
  }
}

BLYNK_WRITE(V5) { // Manual Servo
  if (!autoMode) {
    currentServoAngle = constrain(param.asInt(), 0, 180);
    barricade.write(currentServoAngle);
  }
}

BLYNK_WRITE(V6) { // Manual Drainage Motor
  if (!autoMode) {
    motorState = param.asInt();
    digitalWrite(motorRelay, motorState ? RELAY_ON : RELAY_OFF);
  }
}

BLYNK_WRITE(V7) { // Auto Mode Toggle
  autoMode = param.asInt();
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

// ================= SENSOR & AUTO LOGIC =================
void sendSensorData() {
  int rainDetected = digitalRead(rainPin);

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH, 30000);

  if (duration > 0) {
    distance = duration * 0.034 / 2;
    waterLevel = constrain(maxHeight - distance, 0, maxHeight);
  }

  if (autoMode) {
    motorState = (rainDetected == LOW);
    digitalWrite(motorRelay, motorState ? RELAY_ON : RELAY_OFF);

    if (waterLevel >= 6) pumpState = true;
    else if (waterLevel <= 3) pumpState = false;
    digitalWrite(pumpRelay, pumpState ? RELAY_ON : RELAY_OFF);

    currentServoAngle = (waterLevel < 15) ? 90 : 0;
    barricade.write(currentServoAngle);
  }

  // LED & Buzzer
  if (waterLevel < 5) {
    digitalWrite(greenLED, HIGH); digitalWrite(yellowLED, LOW); digitalWrite(redLED, LOW); digitalWrite(buzzer, LOW);
  } else if (waterLevel < 15) {
    digitalWrite(greenLED, LOW); digitalWrite(yellowLED, HIGH); digitalWrite(redLED, LOW); digitalWrite(buzzer, LOW);
  } else {
    digitalWrite(greenLED, LOW); digitalWrite(yellowLED, LOW); digitalWrite(redLED, HIGH); digitalWrite(buzzer, HIGH);
  }

  if (lcdReady) {
    lcd->setCursor(0, 0);
    lcd->print("Lvl:"); lcd->print(waterLevel); lcd->print("cm Rain:");
    lcd->print(rainDetected == LOW ? "YES" : "NO ");
    lcd->setCursor(0, 1);
    if (waterLevel >= 15) lcd->print("DANGER: CLOSED ");
    else lcd->print("Underpass CLEAR");
  }

  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, waterLevel);
    Blynk.virtualWrite(V1, (rainDetected == LOW) ? 255 : 0);
    Blynk.virtualWrite(V2, pumpState ? 255 : 0);
    Blynk.virtualWrite(V4, pumpState);
    Blynk.virtualWrite(V5, currentServoAngle);
    Blynk.virtualWrite(V6, motorState);

    // --- UPDATED ALERT LOGIC FOR V3 ---
    if (waterLevel >= 15) {
      Blynk.virtualWrite(V3, "DANGER: FLOODED");
    } else if (waterLevel >= 5) {
      Blynk.virtualWrite(V3, "WARNING: HIGH");
    } else {
      Blynk.virtualWrite(V3, "SAFE: CLEAR");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(rainPin, INPUT_PULLUP);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(motorRelay, OUTPUT);
  pinMode(pumpRelay, OUTPUT);
  digitalWrite(motorRelay, RELAY_OFF);
  digitalWrite(pumpRelay, RELAY_OFF);
  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  barricade.attach(servoPin);
  barricade.write(90);
  Wire.begin(21, 22);
  initLCD();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(1000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}