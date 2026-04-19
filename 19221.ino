/************************************************************
   Chameleon Greenhouse - ESP32 + Blynk + Firebase + Calibration System
   Multi-Plant Smart Greenhouse with Complete Sensor Calibration
************************************************************/

/* Blynk */
#define BLYNK_TEMPLATE_ID "TMPL2PKGyCUw5"
#define BLYNK_TEMPLATE_NAME "greenhouse"
#define BLYNK_AUTH_TOKEN "pdNfdI5Jilv4Rq69WIQdKPYO2pWULxR8"

/* Firebase Configuration */
#define FIREBASE_HOST "greenhouse-27fd6-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "01GUpPKIcN7pPmJqw9JsSUOdXuWp7yASSiUKdUzz"

/* Libraries */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

/* WiFi */
char ssid[] = "kotory10";
char pass[] = "salma123456789";

/* Firebase Objects */
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/* DHT22 */
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

/* Soil Moisture */
#define SOIL_PIN 34

/* Relay Pins */
#define FAN_RELAY     16
#define MIST_RELAY    17
#define PUMP_RELAY    18
#define HEATER_RELAY  21

/* Servo Motor */
#define SERVO_PIN 19
Servo shadeServo;

/* Plant Database */
struct Plant {
  String name;
  float maxTemp;
  float minTemp;
  float minHumidity;
  float maxHumidity;
  int minSoilMoisture;
  bool needsShade;
};

Plant plants[3] = {
  {"Coffee", 24.0, 18.0, 60.0, 80.0, 40, true},
  {"Papaya", 30.0, 22.0, 60.0, 85.0, 50, false},
  {"Jackfruit", 32.0, 24.0, 70.0, 90.0, 60, false}
};

int currentPlantIndex = 0;

/* CALIBRATION SYSTEM */
enum CalibrationMode {
  NORMAL_MODE,
  SOIL_CALIBRATION,
  DHT_TEMP_CALIBRATION,
  DHT_HUMIDITY_CALIBRATION
};

CalibrationMode calMode = NORMAL_MODE;

// Soil Moisture Calibration Variables
int soilDryADC = 3200;    // تربة جافة (معروفة)
int soilWetADC = 1500;    // تربة مبللة (معروفة)
float soilDryPercent = 0; // نسبة التربة الجافة المعروفة
float soilWetPercent = 100; // نسبة التربة المبللة المعروفة

// DHT22 Calibration Variables
float dhtTempOffset = 0.0;    // تصحيح درجة الحرارة
float dhtHumidityOffset = 0.0; // تصحيح الرطوبة

// Calibration Status
bool calibrationComplete = false;
unsigned long calTimer = 0;

/* System Variables */
bool manualMode = false;
unsigned long sendDataPrevMillis = 0;
unsigned long lastFirebaseUpdate = 0;

/************************************************************
   Soil Moisture Calibration Functions
************************************************************/
void enterSoilCalibration() {
  calMode = SOIL_CALIBRATION;
  Serial.println("🌱 === SOIL MOISTURE CALIBRATION ===");
  Serial.println("1. Put sensor in DRY soil");
  Serial.println("2. Press V30 in Blynk to set DRY point");
  Serial.println("3. Put sensor in WET soil");
  Serial.println("4. Press V31 in Blynk to set WET point");
  Serial.println("5. Press V32 to EXIT calibration");
}

void calibrateSoilDry() {
  soilDryADC = analogRead(SOIL_PIN);
  Serial.print("✅ Dry Soil ADC: "); Serial.println(soilDryADC);
  Blynk.virtualWrite(V40, soilDryADC);
  
  // حفظ في Firebase
  Firebase.RTDB.setInt(&fbdo, "calibration/soilDryADC", soilDryADC);
}

void calibrateSoilWet() {
  soilWetADC = analogRead(SOIL_PIN);
  Serial.print("✅ Wet Soil ADC: "); Serial.println(soilWetADC);
  Blynk.virtualWrite(V41, soilWetADC);
  
  // حفظ في Firebase
  Firebase.RTDB.setInt(&fbdo, "calibration/soilWetADC", soilWetADC);
  
  calibrationComplete = true;
  Serial.println("🎉 Soil Calibration COMPLETE!");
}

/************************************************************
   DHT22 Calibration Functions
************************************************************/
void enterDHTCalibration() {
  calMode = DHT_TEMP_CALIBRATION;
  Serial.println("🌡️ === DHT22 CALIBRATION ===");
  Serial.println("Compare with THERMOMETER:");
  Serial.println("1. HOT temp → V33");
  Serial.println("2. COLD temp → V34");
  Serial.println("3. NORMAL temp → V35");
  Serial.println("4. Press V36 to calculate offsets");
}

void calibrateDHTTemp(int knownTemp) {
  float measuredTemp = dht.readTemperature();
  if (!isnan(measuredTemp)) {
    dhtTempOffset = knownTemp - measuredTemp;
    Serial.print("✅ Temp Offset: "); Serial.print(dhtTempOffset); Serial.println("°C");
    Blynk.virtualWrite(V42, dhtTempOffset);
  }
}

void calibrateDHTHumidity(int knownHumidity) {
  float measuredHumidity = dht.readHumidity();
  if (!isnan(measuredHumidity)) {
    dhtHumidityOffset = knownHumidity - measuredHumidity;
    Serial.print("✅ Humidity Offset: "); Serial.print(dhtHumidityOffset); Serial.println("%");
    Blynk.virtualWrite(V43, dhtHumidityOffset);
  }
}

/************************************************************
   Blynk Calibration Controls
************************************************************/
// Soil Calibration Controls
BLYNK_WRITE(V30) { if (calMode == SOIL_CALIBRATION) calibrateSoilDry(); }
BLYNK_WRITE(V31) { if (calMode == SOIL_CALIBRATION) calibrateSoilWet(); }
BLYNK_WRITE(V32) { 
  calMode = NORMAL_MODE; 
  Serial.println("✅ Calibration Mode OFF - Normal Operation");
}

// DHT Calibration Controls
BLYNK_WRITE(V33) { if (calMode == DHT_TEMP_CALIBRATION) calibrateDHTTemp(35); }  // Hot
BLYNK_WRITE(V34) { if (calMode == DHT_TEMP_CALIBRATION) calibrateDHTTemp(15); }  // Cold  
BLYNK_WRITE(V35) { if (calMode == DHT_TEMP_CALIBRATION) calibrateDHTTemp(25); }  // Normal
BLYNK_WRITE(V36) { 
  calMode = NORMAL_MODE; 
  calibrationComplete = true;
  Serial.println("✅ DHT Calibration COMPLETE!");
}

// Enter Calibration Modes
BLYNK_WRITE(V37) { enterSoilCalibration(); }
BLYNK_WRITE(V38) { enterDHTCalibration(); }

/************************************************************
   Plant & Manual Controls (سابقة)
************************************************************/
BLYNK_WRITE(V10) { 
  currentPlantIndex = param.asInt() % 3;
  Firebase.RTDB.setString(&fbdo, "greenhouse/currentPlant", plants[currentPlantIndex].name);
}

BLYNK_WRITE(V11) { manualMode = param.asInt(); }

/************************************************************
   Calibrated Sensor Reading Function
************************************************************/
void readCalibratedSensors(float &temp, float &humidity, int &soilPercent) {
  // DHT22 مع التصحيح
  temp = dht.readTemperature() + dhtTempOffset;
  humidity = dht.readHumidity() + dhtHumidityOffset;
  
  // Soil Moisture مع التصحيح
  int soilRaw = analogRead(SOIL_PIN);
  soilPercent = map(soilRaw, soilDryADC, soilWetADC, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
}

/************************************************************
   Main Sensor Data + Control Function
************************************************************/
void sendSensorData() {
  float temperature, humidity;
  int soilPercent;
  
  readCalibratedSensors(temperature, humidity, soilPercent);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("❌ Sensor reading failed!");
    return;
  }

  Plant currentPlant = plants[currentPlantIndex];

  /**************** Display Data ****************/
  Serial.println("=== CALIBRATED DATA ===");
  Serial.print("Plant: "); Serial.println(currentPlant.name);
  Serial.print("🌡️ Temp: "); Serial.print(temperature); Serial.print("°C ("); 
  Serial.print(temperature - dhtTempOffset); Serial.print(" +"); Serial.print(dhtTempOffset,1); Serial.println("°C)");
  Serial.print("💧 Humidity: "); Serial.print(humidity); Serial.print("% ("); 
  Serial.print(humidity - dhtHumidityOffset); Serial.print(" +"); Serial.print(dhtHumidityOffset,1); Serial.println("%)");
  Serial.print("🌱 Soil: "); Serial.print(soilPercent); Serial.print("% (ADC: "); 
  Serial.print(analogRead(SOIL_PIN)); Serial.print(", Dry:"); Serial.print(soilDryADC); 
  Serial.print(", Wet:"); Serial.println(soilWetADC); Serial.println(")");

  /**************** Blynk ****************/
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, soilPercent);
  Blynk.virtualWrite(V3, currentPlantIndex);
  
  // Calibration Status
  Blynk.virtualWrite(V44, calibrationComplete ? 1 : 0);
  Blynk.virtualWrite(V45, soilDryADC);
  Blynk.virtualWrite(V46, soilWetADC);

  /**************** Firebase ****************/
  if (millis() - lastFirebaseUpdate > 30000) {
    Firebase.RTDB.setFloat(&fbdo, "greenhouse/temperature", temperature);
    Firebase.RTDB.setFloat(&fbdo, "greenhouse/humidity", humidity);
    Firebase.RTDB.setInt(&fbdo, "greenhouse/soilMoisture", soilPercent);
    Firebase.RTDB.setString(&fbdo, "calibration/status", calibrationComplete ? "Complete" : "Pending");
    lastFirebaseUpdate = millis();
  }

  if (manualMode || !calibrationComplete) return;

  /**************** Automatic Control ****************/
  // نفس التحكم السابق مع البيانات المُصححة...
  // Temperature Control
  if (temperature > currentPlant.maxTemp) {
    digitalWrite(FAN_RELAY, LOW);
    digitalWrite(HEATER_RELAY, HIGH);
  } else if (temperature < currentPlant.minTemp) {
    digitalWrite(FAN_RELAY, HIGH);
    digitalWrite(HEATER_RELAY, LOW);
  } else {
    digitalWrite(FAN_RELAY, HIGH);
    digitalWrite(HEATER_RELAY, HIGH);
  }

  // Humidity Control
  if (humidity < currentPlant.minHumidity) {
    digitalWrite(MIST_RELAY, LOW);
  } else {
    digitalWrite(MIST_RELAY, HIGH);
  }

  // Soil Control
  if (soilPercent < currentPlant.minSoilMoisture) {
    digitalWrite(PUMP_RELAY, LOW);
  } else {
    digitalWrite(PUMP_RELAY, HIGH);
  }

  // Shade Control
  if (currentPlant.needsShade && temperature > 26.0) {
    shadeServo.write(90);
  } else {
    shadeServo.write(0);
  }
}

/************************************************************
   Setup
************************************************************/
void setup() {
  Serial.begin(115200);
  
  // Initialize hardware
  dht.begin();
  pinMode(FAN_RELAY, OUTPUT);
  pinMode(MIST_RELAY, OUTPUT);
  pinMode(PUMP_RELAY, OUTPUT);
  pinMode(HEATER_RELAY, OUTPUT);
  
  digitalWrite(FAN_RELAY, HIGH);
  digitalWrite(MIST_RELAY, HIGH);
  digitalWrite(PUMP_RELAY, HIGH);
  digitalWrite(HEATER_RELAY, HIGH);
  
  shadeServo.attach(SERVO_PIN);
  shadeServo.write(0);
  
  // Connect services
Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  
  Serial.println("🚀 Chameleon Greenhouse with CALIBRATION Started!");
  Serial.println("📱 Use Blynk V37 for Soil Calib, V38 for DHT Calib");
}

/************************************************************
   Loop
************************************************************/
void loop() {
  Blynk.run();
  if (millis() - sendDataPrevMillis > 2000) {
    sendSensorData();
    sendDataPrevMillis = millis();
  }
} 
