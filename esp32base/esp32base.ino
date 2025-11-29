#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "addons/TokenHelper.h"   // For token status info
#include "addons/RTDBHelper.h"    // For debug helper

// Provide the token status callbacks when using Anonymous sign-in
void tokenStatusCallback(TokenInfo info);

// --- Configuration Constants ---
const char* AP_SSID = "HydroLink_Setup";
const char* AP_PASSWORD = "password123";
const int CONFIG_PORTAL_TIMEOUT_SECONDS = 180;

// Pin Definitions for ESP32
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 4
#define ULTRASONIC_TRIG_PIN 5 
#define ULTRASONIC_ECHO_PIN 18
#define SOLENOID_PIN 13
#define BATTERY_SENSE_PIN 1
#define FLOW_SENSOR_PIN 2


// Firebase configuration
#define FIREBASE_API_KEY "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA"
#define FIREBASE_DATABASE_URL "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app/"


// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800; // GMT+8 for Philippines
const int daylightOffset_sec = 0;

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// U8g2 Display Object for a 128x64 SH1106, using full buffer, hardware I2C.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Voltage Divider Config
const float R1 = 100000.0; // From battery+ to sense pin (100kΩ)
const float R2 = 20000.0;  // From sense pin to GND (20kΩ, made from two 10kΩ in series)

// Battery voltage limits
const float MAX_BATTERY_VOLTAGE = 4.2; // Full
const float MIN_BATTERY_VOLTAGE = 3.7; // Empty

bool solenoidState = false;  // Tracks if solenoid is ON

// --- Refill Tracking Variables ---
unsigned long refillStartTime = 0;
float currentRefillLiters = 0.0;
float currentRefillFlowRateSum = 0.0;
int flowRateSamples = 0;

// --- Firebase Status Variables to be updated ---
unsigned long lastRefillDurationMs = 0;
float lastRefillLiters = 0.0;
float lastRefillAvgFlowRateLPM = 0.0;
float stopWaterLevelCm = 0.0; // Water level distance (cm) when refilling stopped

// --- Refill History Variables ---
float startWaterDistanceCm = 0.0;
String refillEventType = "Automatic"; // Only "Automatic" remains
String refillStatus = "";
String actionLogDetails = "";

// --- No Water Retry Logic ---
unsigned long solenoidOpenStart = 0;
bool waitingForWater = false;
unsigned long lastWaterRetryAttempt = 0;
const unsigned long noFlowTimeoutMs = 5000;      // 5 seconds no flow
const unsigned long retryIntervalMs = 5000;    // 2 minutes
bool isNoWaterDetected = false;

// 
const unsigned char PROGMEM water_droplet_icon_30x54[] = {
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0e, 0x00,
 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01, 0xff, 0x00,
 0x00, 0x03, 0xff, 0x00, 0x00, 0x07, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x1f, 0xff, 0x00,
 0x00, 0x3f, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x00, 0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x00,
 0x07, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x00, 0x1f, 0xff, 0xff, 0x00, 0x3f, 0xff, 0xff, 0x00,
 0x7f, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
 0x7f, 0xff, 0xff, 0x00, 0x3f, 0xff, 0xff, 0x00, 0x1f, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x00,
 0x07, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x00, 0x01, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x00,
 0x00, 0x3f, 0xff, 0x00, 0x00, 0x1f, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x07, 0xff, 0x00,
 0x00, 0x03, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x3f, 0x00,
 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- Global Variables ---
WiFiManager wm;
Preferences preferences;

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// System variables
float waterDistanceCm = 0.0;
int waterPercentage = 0;
bool waterAvailable = true;
int refillThresholdPercentage = 25;
int maxFillLevelPercentage = 75;
int batteryPercentage = 0;
bool drumHeightInitialized = false;
float drumHeightCm = 0.0;

// Control flags
bool isDeviceFullyConfigured = false;
bool wifiConnected = false;
String deviceFirebaseId = "";
String deviceMacAddress = "";
bool isLinkedToUser = false;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastSettingsCheck = 0;
const unsigned long SENSOR_INTERVAL = 3000;
const unsigned long FIREBASE_INTERVAL = 3000;
const unsigned long SETTINGS_INTERVAL = 3000;

// --- Function Prototypes ---
void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
String getMacAddress();
float readUltrasonicCm();
void calculateWaterPercentage();
void updateFirebaseData();
void readFirebaseSettings();
void measureDrumHeight();
bool fetchDrumHeightFromFirebase();
void initializeDrumHeight();
int readBatteryPercentage();
void setSystemStatus(String status);
void checkManualDrumMeasurement();
void setupFirebase();
void mapMacToFirebaseUid();
bool ensureFirebaseConnection();
void handleWiFiReconnection();
void loadSettingsFromPreferences();
void saveSettingsToPreferences();
bool checkDeviceLinkStatus();
String getStoredFirebaseId();
void storeFirebaseId(String uid);
void handleFactoryReset();
void debugPreferences();
void resetFirebaseAuth();
void openSolenoid();
void closeSolenoid(String stopReason);

// --- OLED Display Function Prototypes ---
void displayHydroLinkAnimation();
void displayHomeScreen();
void displaySetupScreen();
void drawBatteryIcon(int percentage);
void drawWiFiIcon();
void logStatusToSerial();

bool checkDeviceExists(String deviceId) {
    String path = "/hydrolink/devices/" + deviceId;
    if (Firebase.RTDB.pathExisted(&fbdo, path)) {
        Serial.println("Device found in Firebase.");
        return true;
    } else {
        Serial.println("Device not found in Firebase. Resetting...");
        return false;
    }
}

void resetDevice() {
    preferences.begin("hydrolink", false);
    preferences.clear();
    preferences.end();
    ESP.restart(); // restart to go back into setup mode
}

void loadSettingsFromPreferences() {
  Serial.println("Loading settings from preferences...");
  
  debugPreferences();
  if (!preferences.begin("hydrolink", true)) {
    Serial.println("Failed to open preferences namespace! Forcing factory reset.");
    handleFactoryReset();
    return;
  }

  // ✅ Load Firebase UID here
  deviceFirebaseId = preferences.getString("firebase_uid", "");
  if (deviceFirebaseId.length() > 0) {
    Serial.println("Firebase UID loaded: " + deviceFirebaseId);
  } else {
    Serial.println("Firebase UID not found in preferences!");
  }
  
  float storedDrumHeight = preferences.getFloat("drumHeight", 0.0);
  if (storedDrumHeight < 0 || storedDrumHeight > 1000) {
    Serial.println("Invalid stored drum height, resetting to 0");
    storedDrumHeight = 0.0;
  }
  drumHeightCm = storedDrumHeight;
  
  refillThresholdPercentage = preferences.getInt("refillThreshold", 25);
  maxFillLevelPercentage = preferences.getInt("maxFillLevel", 75);
  waterDistanceCm = preferences.getFloat("lastWaterDistance", 0.0);
  waterPercentage = preferences.getInt("lastWaterPercent", 0);
  waterAvailable = preferences.getBool("lastWaterAvailable", true);
  batteryPercentage = preferences.getInt("lastBattery", 100);
  isLinkedToUser = preferences.getBool("isLinked", false);
  drumHeightInitialized = (drumHeightCm > 0);
  isDeviceFullyConfigured = drumHeightInitialized;
  
  preferences.end();

  Serial.println("=== Settings Loaded from Preferences ===");
  Serial.println("Firebase UID: " + (deviceFirebaseId.length() > 0 ? deviceFirebaseId : String("NOT_FOUND")));
  Serial.println("Drum height: " + String(drumHeightCm) + " cm");
  Serial.println("Refill threshold: " + String(refillThresholdPercentage) + "%");
  Serial.println("Max fill level: " + String(maxFillLevelPercentage) + "%");
  Serial.println("Device configured: " + String(isDeviceFullyConfigured ? "Yes" : "No"));
  Serial.println("Device linked: " + String(isLinkedToUser ? "Yes" : "No"));
}


void saveSettingsToPreferences() {
  preferences.begin("hydrolink", false);
  
  preferences.putFloat("drumHeight", drumHeightCm);
  preferences.putInt("refillThreshold", refillThresholdPercentage);
  preferences.putInt("maxFillLevel", maxFillLevelPercentage);
  preferences.putFloat("lastWaterDistance", waterDistanceCm);
  preferences.putInt("lastWaterPercent", waterPercentage);
  preferences.putBool("lastWaterAvailable", waterAvailable);
  preferences.putInt("lastBattery", batteryPercentage);
  preferences.putBool("isLinked", isLinkedToUser);
  
  preferences.end();
}

String getStoredFirebaseId() {
  preferences.begin("hydrolink", true);
  String storedId = preferences.getString("firebaseUid", "");
  preferences.end();
  return storedId;
}

void storeFirebaseId(String uid) {
  preferences.begin("hydrolink", false);
  preferences.putString("firebaseUid", uid);
  preferences.end();
}

bool checkDeviceLinkStatus() {
  if (!Firebase.ready() || deviceFirebaseId.length() == 0) {
    return false;
  }
  
  String linkPath = "hydrolink/devices/" + deviceFirebaseId + "/linkedUsers";
  bool linked = false;
  if (Firebase.RTDB.getJSON(&fbdo, linkPath.c_str())) {
    String jsonStr = fbdo.jsonString();
    if (jsonStr != "null" && jsonStr.length() > 4) {
      linked = true;
    }
  } else {
    Serial.print("Firebase get failed for link status: ");
    Serial.println(fbdo.errorReason());
  }

  if (linked != isLinkedToUser) {
    isLinkedToUser = linked;
    saveSettingsToPreferences();
    Serial.println("Device link status changed to: " + String(isLinkedToUser ? "LINKED" : "UNLINKED"));
  }
  
  return isLinkedToUser;
}

volatile int flowPulseCount = 0;
float totalLiters = 0.0;   // Track total water usage

// --- Flow Sensor ISR ---
void IRAM_ATTR flowPulseISR() {
  flowPulseCount++;
}

// --- Setup Flow Sensor ---
void setupFlowSensor() {
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseISR, RISING);
}

float getFlowRate() {
  static unsigned long lastTime = 0;
  static int lastPulseCount = 0;
  static float flowRate = 0.0;
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= 1000) {  // Every 1 second
    int pulseDiff = flowPulseCount - lastPulseCount;
    lastPulseCount = flowPulseCount;
    lastTime = currentTime;

    // For YF-S403: 450 pulses = 1 L/min
    flowRate = (pulseDiff / 450.0) * 60.0; // L/min

    // --- Track flow rate for average calculation ---
    if (solenoidState) { // Only track when solenoid is ON
        currentRefillFlowRateSum += flowRate;
        flowRateSamples++;
    }
    // ---------------------------------------------------

    totalLiters += (flowRate / 60.0); // Add liters per second

    // If there’s any pulse, water is available
    waterAvailable = (pulseDiff > 0);
  }

  return flowRate;
}


void handleFactoryReset() {
  Serial.println("=== FACTORY RESET DETECTED ===");
  
  preferences.begin("hydrolink", false);
  preferences.clear();
  preferences.end();
  
  resetFirebaseAuth();
  drumHeightCm = 0.0;
  refillThresholdPercentage = 25;
  maxFillLevelPercentage = 75;
  waterDistanceCm = 0.0;
  waterPercentage = 0;
  waterAvailable = true;
  batteryPercentage = 100;
  drumHeightInitialized = false;
  isDeviceFullyConfigured = false;
  deviceFirebaseId = "";
  isLinkedToUser = false;
  Serial.println("All settings reset to factory defaults");
}

void initializeDrumHeight() {
  if (drumHeightInitialized) {
    Serial.println("Drum height already initialized: " + String(drumHeightCm) + " cm");
    return;
  }
  
  Serial.println("Initializing drum height...");
  if (isLinkedToUser && Firebase.ready() && fetchDrumHeightFromFirebase()) {
    Serial.println("Drum height set from Firebase.");
    drumHeightInitialized = true;
    isDeviceFullyConfigured = true;
    saveSettingsToPreferences();
    return;
  }

  Serial.println("Drum height not found. Awaiting setup via web app.");
}

bool fetchDrumHeightFromFirebase() {
  if (!Firebase.ready() || deviceFirebaseId.length() == 0) {
    return false;
  }
  
  String heightPath = "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
  Serial.println("Reading drum height from: " + heightPath);
  if (Firebase.RTDB.getFloat(&fbdo, heightPath)) {
    if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_float || fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_integer) {
      float newDrumHeight = fbdo.floatData();
      if (newDrumHeight > 0) {
        drumHeightCm = newDrumHeight;
        drumHeightInitialized = true;
        isDeviceFullyConfigured = true;
        saveSettingsToPreferences();
        
        Serial.println("Drum height fetched from Firebase: " + String(newDrumHeight));
        return true;
      }
    }
  }
  return false;
}

void measureDrumHeight() {
  Serial.println("Measuring drum height...");
  const int numMeasurements = 5;
  float measurements[numMeasurements];
  int validCount = 0;
  for (int i = 0; i < numMeasurements; i++) {
    float reading = readUltrasonicCm();
    if (reading > 0) {
      measurements[validCount++] = reading;
      Serial.println("Measurement " + String(i + 1) + ": " + String(reading));
    }
    delay(200);
  }

  if (validCount == 0) {
    Serial.println("No valid measurements obtained!");
    return;
  }

  float sum = 0;
  for (int i = 0; i < validCount; i++) {
    sum += measurements[i];
  }
  drumHeightCm = sum / validCount;

  if (drumHeightCm <= 0) {
    Serial.println("Invalid calculated drum height!");
    return;
  }

  drumHeightInitialized = true;
  isDeviceFullyConfigured = true;
  saveSettingsToPreferences();
  
  Serial.println("Drum height measured and saved: " + String(drumHeightCm));
  if (isLinkedToUser && Firebase.ready() && deviceFirebaseId.length() > 0) {
    String heightPath = "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
    if (Firebase.RTDB.setFloat(&fbdo, heightPath, drumHeightCm)) {
      Serial.println("Drum height updated in Firebase");
    } else {
      Serial.println("Failed to update Firebase: " + fbdo.errorReason());
    }
  }
}

float readUltrasonicCm() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(25);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  unsigned long duration = pulseInLong(ULTRASONIC_ECHO_PIN, HIGH, 60000);
  if (duration == 0) {
    Serial.println("Ultrasonic sensor timeout");
    return -1.0;
  }

  return duration * 0.034 / 2.0;
}

void calculateWaterPercentage() {
  if (drumHeightCm <= 0) {
    waterPercentage = 0;
    waterAvailable = false;
    return;
  }

  float waterLevelCm = drumHeightCm - waterDistanceCm;
  waterLevelCm = constrain(waterLevelCm, 0, drumHeightCm);
  float effectiveHeight = drumHeightCm;
  if (maxFillLevelPercentage > 0 && maxFillLevelPercentage < 100) {
    effectiveHeight = drumHeightCm * (maxFillLevelPercentage / 100.0);
  }
  
  waterPercentage = (waterLevelCm / effectiveHeight) * 100;
  waterPercentage = constrain(waterPercentage, 0, 100);
  waterAvailable = (waterPercentage > refillThresholdPercentage);
  
  Serial.printf("Water: %d%% (%.1fcm / %.1fcm)\n", waterPercentage, waterLevelCm, drumHeightCm);
}

void logRefillHistory() {
  // Only log if Firebase is ready, device is identified, and a refill event actually occurred (duration > 0)
  if (!Firebase.ready() || deviceFirebaseId.length() == 0 || lastRefillDurationMs == 0) {
    return;
  }

  // --- 1. Calculate Starting Percentage (beforeLevelPct) ---
  float waterLevelCmStart = drumHeightCm - startWaterDistanceCm;
  waterLevelCmStart = constrain(waterLevelCmStart, 0, drumHeightCm);
  
  float effectiveHeight = drumHeightCm;
  // Account for the max fill level setting
  if (maxFillLevelPercentage > 0 && maxFillLevelPercentage < 100) {
    effectiveHeight = drumHeightCm * (maxFillLevelPercentage / 100.0);
  }
  int startWaterPercentageCalc = (waterLevelCmStart / effectiveHeight) * 100;
  startWaterPercentageCalc = constrain(startWaterPercentageCalc, 0, 100);
  
  // --- 2. Build the JSON object ---
  String basePath = "hydrolink/devices/" + deviceFirebaseId + "/refillHistory";

  FirebaseJson historyJson;
  // Refill Details
  historyJson.set("timestamp/.sv", "timestamp"); // Use Firebase Server Value for perfect time
  historyJson.set("type", refillEventType); 
  historyJson.set("beforeLevelPct", startWaterPercentageCalc);
  historyJson.set("afterLevelPct", waterPercentage); // waterPercentage is the final value from the loop()
  historyJson.set("amountLitersAdded", String(lastRefillLiters, 2));
  historyJson.set("durationMin", String(lastRefillDurationMs / (1000.0 * 60.0), 1)); // Store in minutes
  historyJson.set("status", refillStatus); 
  historyJson.set("actionsLog", actionLogDetails);

  // Settings Snapshot (Config)
  FirebaseJson configJson;
  configJson.set("drumHeightCm", String(drumHeightCm, 1));
  configJson.set("maxFillLevelPercentage", maxFillLevelPercentage);
  configJson.set("refillThresholdPercentage", refillThresholdPercentage);

  historyJson.set("config", configJson);

  // --- 3. PUSH the new log entry ---
  if (Firebase.RTDB.pushJSON(&fbdo, basePath, &historyJson)) {
    Serial.println("Refill history logged successfully!");
    // Reset the duration flag to zero so we don't log the same event twice
    lastRefillDurationMs = 0;
  } else {
    Serial.println("Refill history log FAILED: " + fbdo.errorReason());
  }
}

// Simplified: Removed String type argument and logic. Solenoid is only opened for "Automatic".
void openSolenoid() {
  // Solenoid OPEN (Refill): Active-LOW is LOW since it is Normally Closed.
  digitalWrite(SOLENOID_PIN, HIGH);
  solenoidState = true; 

  waterAvailable = true;

  // --- START REFILL RECORDING ---
  refillStartTime = millis();
  currentRefillLiters = 0.0;
  currentRefillFlowRateSum = 0.0;
  flowRateSamples = 0;
  
  // Capture STARTING STATE & TYPE
  startWaterDistanceCm = waterDistanceCm;
  refillEventType = "Automatic"; // Hardcoded to "Automatic"
  actionLogDetails = "NC Solenoid Open via Auto-Threshold";
  
  Serial.println("💧 Solenoid OPEN (Automatic) - Refilling...");
}

void closeSolenoid(String stopReason) {
  // Solenoid CLOSED (Full): Active-LOW is HIGH.
  digitalWrite(SOLENOID_PIN, LOW);
  solenoidState = false; // Update the global solenoid state flag
  
  // --- END REFILL RECORDING & CALCULATE STATS ---
  if (refillStartTime > 0) {
    // 1. Calculate Duration (ms)
    lastRefillDurationMs = millis() - refillStartTime;
    // 2. Calculate Average Flow Rate
    if (flowRateSamples > 0) {
      lastRefillAvgFlowRateLPM = currentRefillFlowRateSum / flowRateSamples;
    } else {
      lastRefillAvgFlowRateLPM = 0.0;
    }

    // 3. Calculate Total Liters Refilled
    // Liters = Avg_Flow_Rate (L/min) * Duration (min)
    float refillDurationMinutes = lastRefillDurationMs / (1000.0 * 60.0);
    lastRefillLiters = lastRefillAvgFlowRateLPM * refillDurationMinutes;

    // 4. Record final water distance (cm)
    stopWaterLevelCm = waterDistanceCm;
    
    // 5. Set final status and log details for history (CRITICAL FOR HISTORY LOG)
    // Append the final stop reason to the initial actionLogDetails captured in openSolenoid()
    actionLogDetails += " | Stop: " + stopReason;
    
    // Determine the final refill status based on the reason
    if (stopReason.indexOf("No Water Detected") != -1) {
      refillStatus = "Failed"; // Special case for water shortage
    } else if (stopReason.indexOf("Max Fill Level Reached") != -1) {
      refillStatus = "Completed";
    } else {
      refillStatus = "Interrupted"; // Should not happen in pure auto-mode unless error
    }
    
    // Reset start time to prevent re-calculation
    refillStartTime = 0;
  }
  
  // Reset flow tracking variables for the next event
  currentRefillFlowRateSum = 0.0;
  flowRateSamples = 0;
  Serial.printf("✅ Solenoid CLOSED - Stats: %.2f L @ %.2f L/min for %.1f sec\n", 
    lastRefillLiters, lastRefillAvgFlowRateLPM, lastRefillDurationMs / 1000.0);
}


void updateFirebaseData() {
 if (!Firebase.ready() || deviceFirebaseId.length() == 0) {
    return;
  }

  String basePath = "hydrolink/devices/" + deviceFirebaseId + "/status";
  
  FirebaseJson json;
  json.set("currentWaterLevelCm", String(waterDistanceCm, 1));
  json.set("waterPercentage", waterPercentage);
  json.set("waterAvailable", waterAvailable);
  json.set("batteryPercentage", batteryPercentage);
  json.set("lastUpdated/.sv", "timestamp");
  json.set("deviceId", deviceFirebaseId);
  json.set("macAddress", deviceMacAddress);
  json.set("drumHeightCm", drumHeightCm);
  json.set("isConfigured", isDeviceFullyConfigured);
  json.set("isLinked", isLinkedToUser);

  json.set("lastRefillDurationSec", lastRefillDurationMs / 1000.0); // Convert to seconds
  json.set("lastRefillLiters", String(lastRefillLiters, 2));
  json.set("lastRefillAvgFlowRateLPM", String(lastRefillAvgFlowRateLPM, 2));
  json.set("stopWaterDistanceCm", String(stopWaterLevelCm, 1));
  if (Firebase.RTDB.updateNode(&fbdo, basePath, &json)) {
    Serial.println("Firebase data updated successfully");
    saveSettingsToPreferences();
  } else {
    Serial.println("Firebase update failed: " + fbdo.errorReason());
  }
}

void readFirebaseSettings() {
  if (!isLinkedToUser || !Firebase.ready() || deviceFirebaseId.length() == 0) {
    return;
  }

  String settingsPath = "hydrolink/devices/" + deviceFirebaseId + "/settings";
  if (Firebase.RTDB.get(&fbdo, settingsPath)) {
    if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_json) {
      FirebaseJson *json = fbdo.to<FirebaseJson *>();
      FirebaseJsonData jsonData;
      
      bool settingsChanged = false;
      
      if (json->get(jsonData, "refillThresholdPercentage")) {
        if (refillThresholdPercentage != jsonData.to<int>()) {
          refillThresholdPercentage = jsonData.to<int>();
          settingsChanged = true;
        }
      }
      
      if (json->get(jsonData, "maxFillLevelPercentage")) {
        if (maxFillLevelPercentage != jsonData.to<int>()) {
          maxFillLevelPercentage = jsonData.to<int>();
          settingsChanged = true;
        }
      }
      
      if (json->get(jsonData, "drumHeightCm")) {
        float fbDrumHeight = jsonData.to<float>();
        if (fbDrumHeight > 0 && drumHeightCm != fbDrumHeight) {
          drumHeightCm = fbDrumHeight;
          drumHeightInitialized = true;
          isDeviceFullyConfigured = true;
          settingsChanged = true;
        }
      }
      
      if (settingsChanged) {
        Serial.println("Settings updated from Firebase:");
        Serial.println(" Refill threshold: " + String(refillThresholdPercentage));
        Serial.println(" Max fill level: " + String(maxFillLevelPercentage));
        Serial.println(" Drum height: " + String(drumHeightCm));
        saveSettingsToPreferences();
      }
      json->clear();
    }
  }
}

void setSystemStatus(String status) {
  if (!Firebase.ready() || deviceFirebaseId.length() == 0) return;
  String statusPath = "hydrolink/devices/" + deviceFirebaseId + "/status/systemStatus";
  if (Firebase.RTDB.setString(&fbdo, statusPath, status)) {
    Serial.println("System status set to: " + status);
  }
}

void checkManualDrumMeasurement() {
  if (!isLinkedToUser || !Firebase.ready() || deviceFirebaseId.length() == 0) return;
  String measurePath = "hydrolink/devices/" + deviceFirebaseId + "/settings/measureDrum";
  if (Firebase.RTDB.getBool(&fbdo, measurePath) && fbdo.boolData()) {
      Serial.println("Manual drum measurement requested");
      measureDrumHeight();
      Firebase.RTDB.setBool(&fbdo, measurePath, false);
  }
}

int readBatteryPercentage() {
  const int samples = 5;
  float totalVoltage = 0;
  for (int i = 0; i < samples; i++) {
    int adcValue = analogRead(BATTERY_SENSE_PIN);
    float voltageAtPin = (adcValue / 4095.0) * 3.3;
    float batteryVoltage = voltageAtPin * ((R1 + R2) / R2);
    totalVoltage += batteryVoltage;
    delay(10);
  }

  float avgVoltage = totalVoltage / samples;
  int percentage = (int)((avgVoltage - MIN_BATTERY_VOLTAGE) /
                         (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100);
  percentage = constrain(percentage, 0, 100);

  Serial.print("🔋 Battery Voltage: ");
  Serial.print(avgVoltage, 2);
  Serial.print(" V  |  ");
  Serial.print(percentage);
  Serial.println("%");
  return percentage;
}



bool ensureFirebaseConnection() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready, trying to reconnect...");
    Firebase.reconnectWiFi(true);
    return false;
  }
  return true;
}

void handleWiFiReconnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
        Serial.println("WiFi disconnected, attempting to reconnect...");
        wifiConnected = false;
    }
    wm.autoConnect(AP_SSID, AP_PASSWORD);
    if(WiFi.status() == WL_CONNECTED){
        wifiConnected = true;
        Serial.println("WiFi reconnected!");
        Firebase.reconnectWiFi(true);
    }
  }
}

void setupFirebase() {
  Serial.println("Setting up Firebase...");

  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // optional

  // 🔑 Assign device email & password here
  auth.user.email = "hydroLinkDevice_003@hydrolink.com";
  auth.user.password = "hydroLink_123";

  // Start Firebase with config + auth
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Wait until UID is ready
  unsigned long start = millis();
  while (auth.token.uid.length() == 0 && millis() - start < 10000) {
    delay(200);
  }

  if (auth.token.uid.length() > 0) {
    deviceFirebaseId = auth.token.uid.c_str();
    Serial.println("Device UID: " + deviceFirebaseId);
    // Save UID to preferences
    preferences.begin("hydrolink", false);
    preferences.putString("firebase_uid", deviceFirebaseId);
    preferences.end();
  } else {
    Serial.println("❌ Failed to retrieve UID from Firebase");
  }

  Serial.println("Firebase initialized!");
}


void resetFirebaseAuth() {
  // For this library version, creating a new auth instance is the way to reset.
  auth = FirebaseAuth(); 
  
  // Clear stored UID from preferences as well
  preferences.begin("hydrolink", false);
  preferences.remove("firebaseUid");
  preferences.end();
  
  deviceFirebaseId = "";
  isLinkedToUser = false;
  
  Serial.println("Firebase auth state has been reset.");
}

void debugPreferences() {
  preferences.begin("hydrolink", true);
  Serial.println("=== Preferences Debug Info ===");
  Serial.println("Namespace exists: " + String(preferences.isKey("drumHeight") ? "Yes" : "No"));
  Serial.println("Firebase UID: '" + preferences.getString("firebaseUid", "NOT_FOUND") + "'");
  Serial.println("Drum Height: " + String(preferences.getFloat("drumHeight", -999.0)));
  Serial.println("Refill Threshold: " + String(preferences.getInt("refillThreshold", -999)));
  Serial.println("Linked Status: " + String(preferences.getBool("isLinked", false) ? "Yes" : "No"));
  Serial.println("Free entries: " + String(preferences.freeEntries()));
  preferences.end();
}

void logStatusToSerial() {
    
    // Calculate actual water level in CM for printing
    float actualWaterLevelCm = drumHeightCm - waterDistanceCm;
    actualWaterLevelCm = constrain(actualWaterLevelCm, 0, drumHeightCm);
    
    // --- 1. Print Water Level and Status (COMBINED LINE) ---
    Serial.printf("Water: %d%% (%.1fcm / %.1fcm) | Water Status: ", 
                  waterPercentage, 
                  actualWaterLevelCm, 
                  drumHeightCm);
    // Print the availability and end the line
    if (waterAvailable) {
        Serial.println("Available");
    } else {
        Serial.println("Unavailable");
    }
    
    // --- 2. Print Battery Status ---
    // NOTE: Using the calculated batteryPercentage but printing 0.00V as the actual voltage isn't globally available here
    Serial.printf("🔋 Battery Voltage: %.2f V  |  %d%%\n", 0.00, batteryPercentage);
}

void mapMacToFirebaseUid() {
  if (!Firebase.ready() || deviceFirebaseId.isEmpty() || deviceMacAddress.isEmpty()) return;

  String path = "hydrolink/macToFirebaseUid/" + deviceMacAddress;
  // Use setString instead of updateNode for a simple key-value pair
  if (Firebase.RTDB.setString(&fbdo, path, deviceFirebaseId)) {
    Serial.println("MAC to UID mapping updated");
  } else {
    Serial.println("MAC mapping failed: " + fbdo.errorReason());
  }
}

// --- WiFiManager Callbacks ---
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered WiFi config mode");
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
}

void saveConfigCallback() {
  Serial.println("WiFi configuration saved!");
}

String getMacAddress() {
  char macStr[18];
  // Create a version without colons for the Firebase path
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", 
      WiFi.macAddress()[0], WiFi.macAddress()[1], WiFi.macAddress()[2], 
      WiFi.macAddress()[3], WiFi.macAddress()[4], WiFi.macAddress()[5]);
  return String(macStr);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== HydroLink ESP32 Starting ===");
  // --- OLED Initialization ---
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();
  Serial.println("U8g2 OLED initialized.");

  // --- Display Startup Animation ---
  displayHydroLinkAnimation();
  delay(1500);

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  // Solenoid is Normally Closed (NC). Set to HIGH to keep it closed.
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);

  setupFlowSensor(); // 🔹 Initialize water flow sensor interrupt
  Serial.println("Flow sensor initialized.");

  bool factoryResetRequested = false;
  if (factoryResetRequested) {
    handleFactoryReset();
  } else {
    loadSettingsFromPreferences();
  }

  WiFi.mode(WIFI_STA);
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SECONDS);
  Serial.println("Connecting to WiFi...");
  wifiConnected = wm.autoConnect(AP_SSID, AP_PASSWORD);

if (wifiConnected) {
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    deviceMacAddress = WiFi.macAddress();
    Serial.println("Device MAC: " + deviceMacAddress);


    // 🔥 Initialize Firebase right after WiFi is connected
    setupFirebase();
    // Wait for Firebase to get ready and assign a UID
    unsigned long firebase_start_time = millis();
    while(!Firebase.ready() && millis() - firebase_start_time < 15000) {
        Serial.print(".");
        delay(500);
    }

    // Attempt to load the UID from preferences if the callback hasn't fired yet
    if (deviceFirebaseId.length() == 0) {
        deviceFirebaseId = getStoredFirebaseId();
    }

    if (Firebase.ready() && deviceFirebaseId.length() > 0) {
        Serial.println("\nFirebase ready. UID: " + deviceFirebaseId);
        String devicePath = "/hydrolink/devices/" + deviceFirebaseId;
        if (!Firebase.RTDB.pathExisted(&fbdo, devicePath)) {
            Serial.println("Device not found in Firebase. It might be a new device. This is normal.");
        }

        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        fbdo.setBSSLBufferSize(4096, 1024);
        fbdo.setResponseSize(2048);
        setSystemStatus("online");

        deviceMacAddress = WiFi.macAddress(); // formatted MAC
        Serial.println("Device MAC: " + deviceMacAddress);

        // After Firebase.begin(&config, &auth);
        // and after you confirm auth.token.uid is valid:
        if (auth.token.uid.length() > 0) {
            deviceFirebaseId = auth.token.uid.c_str();
            Serial.println("Device UID: " + deviceFirebaseId);

            // Now it's safe to map
            mapMacToFirebaseUid();
        } else {
            Serial.println("No UID from Firebase yet.");
        }

        
        checkDeviceLinkStatus();
        if (!isLinkedToUser) {
            Serial.println("Waiting for device to be linked by user...");
            unsigned long linkStartTime = millis();
            while (millis() - linkStartTime < 300000) { 
                displaySetupScreen();
                if (checkDeviceLinkStatus()) {
                    Serial.println("Device linked to user account!");
                    break;
                }
                delay(5000);
            }
        }

        if(isLinkedToUser) {
            initializeDrumHeight();
            readFirebaseSettings();
        }

        Serial.println("Setup complete!");
        Serial.println("Device ID: " + deviceFirebaseId);
        Serial.println("Device Linked: " + String(isLinkedToUser ? "Yes" : "No"));
        Serial.println("Device Configured: " + String(isDeviceFullyConfigured ? "Yes" : "No"));
    } else {
        Serial.println("Firebase failed to initialize or get a UID. Please check credentials and network.");
    }
} else {
    Serial.println("Failed to connect to WiFi.");
}


  config.token_status_callback = tokenStatusCallback;


  Serial.println("System ready!");
}

// --- Main Loop ---
void loop() {
  unsigned long currentTime = millis();
  handleWiFiReconnection();
  
  // --- Connection Check ---
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB08_tf);
    u8g2.drawStr(10, 35, "Connecting...");
    u8g2.sendBuffer();
    delay(1000);
    return;
  }

  // --- Link Status ---
  if (!isLinkedToUser) {
    displaySetupScreen();
    checkDeviceLinkStatus();
    delay(5000);
    return;
  }

// --- Sensor Readings ---
if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
  float newReading = readUltrasonicCm();
  if (newReading > 0) {
    waterDistanceCm = newReading;
    calculateWaterPercentage();
    
    // --- Read flow sensor data ---
    float flowRate = getFlowRate();
    Serial.printf("Flow rate: %.2f L/min | Total: %.2f L\n", flowRate, totalLiters);
    
    // ------------------------------------------------------------------
    // --- COMPREHENSIVE REFILL LOGIC (Simplified Block) ---
    // ------------------------------------------------------------------
    
    if (!solenoidState && isDeviceFullyConfigured) {
        // --- AUTOMATIC REFILL LOGIC WITH RETRY ---
        if (!waitingForWater) {
            if (waterPercentage < refillThresholdPercentage) {
                Serial.println("💧 Below threshold and water available — opening solenoid!");
                openSolenoid(); // Simplified call
                solenoidOpenStart = millis();
                isNoWaterDetected = false;
            }
        } else {
            // --- Retry every 2 minutes if water was unavailable ---
            if (millis() - lastWaterRetryAttempt >= retryIntervalMs) {
                Serial.println("⏳ Retrying solenoid open to check if water is back...");
                openSolenoid(); // Simplified call
                solenoidOpenStart = millis();
                waitingForWater = false; // Try again
            }
        }
    }

    else if (solenoidState) {
        // Solenoid is ON — monitor flow and stop conditions
        String stopReason = "";
        bool shouldStop = false;

        float flowRate = getFlowRate();

        // --- Detect No Flow for 5 seconds after opening ---
        if (!isNoWaterDetected && flowRate <= 0.1 && (millis() - solenoidOpenStart >= noFlowTimeoutMs)) {
            Serial.println("🚱 No water flow detected for 5 seconds — closing solenoid.");
            stopReason = "No Water Detected";
            shouldStop = true;
            isNoWaterDetected = true;
            waterAvailable = false;
            waitingForWater = true;
            lastWaterRetryAttempt = millis();
        }

        // --- Normal Stop Conditions (Only Automatic) ---
        else if (!shouldStop) {
            if (waterPercentage >= maxFillLevelPercentage) { // Simplified to just check max fill level
                stopReason = "Max Fill Level Reached";
                shouldStop = true;
            } 
        }

        // --- Execute Stop if triggered ---
        if (shouldStop) {
            closeSolenoid(stopReason);
            
            if (lastRefillDurationMs > 0) {
                logRefillHistory();
            }

            updateFirebaseData();
        }
    }

    // ------------------------------------------------------------------

  }

  // --- Battery Update ---
  batteryPercentage = readBatteryPercentage();
  lastSensorRead = currentTime;
}


  // --- Firebase Sync ---
  if (currentTime - lastFirebaseUpdate >= FIREBASE_INTERVAL) {
    updateFirebaseData();
    logStatusToSerial();
    lastFirebaseUpdate = currentTime;
  }

  // --- Firebase Settings ---
  if (currentTime - lastSettingsCheck >= SETTINGS_INTERVAL) {
    if (isLinkedToUser) {
      readFirebaseSettings();
      checkManualDrumMeasurement();
    }
    lastSettingsCheck = currentTime;
  }

  displayHomeScreen();
  delay(100);
} 


// --- OLED Display Functions ---

void displayHydroLinkAnimation() {
  const String appName = "HydroLink";
  int icon_width = 30;
  int icon_height = 54;
  
  u8g2.clearBuffer();
  int droplet_x = (SCREEN_WIDTH - icon_width) / 2;
  int droplet_y = (SCREEN_HEIGHT - icon_height) / 2;
  u8g2.drawXBMP(droplet_x, droplet_y, icon_width, icon_height, water_droplet_icon_30x54);
  u8g2.sendBuffer();
  delay(1000);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB18_tf);
  u8g2.setFontMode(1);
  int text_width = u8g2.getUTF8Width(appName.c_str());
  int text_centered_x = (SCREEN_WIDTH - text_width) / 2;
  int text_height = u8g2.getFontAscent() - u8g2.getFontDescent();
  int text_centered_y = (SCREEN_HEIGHT - text_height) / 2 + u8g2.getFontAscent();
  for (int i = 0; i <= appName.length(); i++) {
    u8g2.clearBuffer();
    u8g2.drawStr(text_centered_x, text_centered_y, appName.substring(0, i).c_str());
    u8g2.sendBuffer();
    delay(150);
  }
}

void displaySetupScreen() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB08_tf);

    u8g2.drawStr(0, 12, "Device Setup");
    u8g2.drawStr(0, 28, "Enter MAC on website:");
    // Format MAC with colons for better readability on screen
    String formattedMac = "";
    for(int i=0; i < deviceMacAddress.length(); i+=2) {
      formattedMac += deviceMacAddress.substring(i, i+2);
      if (i < deviceMacAddress.length() - 2) {
        formattedMac += ":";
      }
    }
    u8g2.drawStr(0, 44, formattedMac.c_str());
    
    u8g2.drawStr(0, 60, "Waiting for link...");
    
    u8g2.sendBuffer();
}


void displayHomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tf);

  drawBatteryIcon(batteryPercentage);
  drawWiFiIcon();

  u8g2.drawStr(0, 28, "Water Level:");
  char buffer[10];
  sprintf(buffer, "%d%%", waterPercentage);
  u8g2.drawStr(u8g2.getUTF8Width("Water Level: ") + 2, 28, buffer);

  u8g2.drawStr(0, 43, "Status: ");
  if (waterAvailable) {
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 2, 43, "Available");
  } else {
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 2, 43, "Unavailable");
  }

  u8g2.drawStr(0, 58, "Threshold: ");
  sprintf(buffer, "%d%%", refillThresholdPercentage);
  u8g2.drawStr(u8g2.getUTF8Width("Threshold: ") + 2, 58, buffer);

  u8g2.sendBuffer();
}

void drawBatteryIcon(int percentage) {
  int x = 95;
  int y = 0;
  int width = 30;
  int height = 15;
  int notchWidth = 4;
  int notchHeight = 5;
  u8g2.drawFrame(x, y, width, height);
  u8g2.drawBox(x + width, y + (height - notchHeight) / 2, notchWidth, notchHeight);
  int fillWidth = map(percentage, 0, 100, 0, width - 4);
  if (fillWidth < 0) fillWidth = 0;
  if (fillWidth > width - 4) fillWidth = width - 4;
  u8g2.drawBox(x + 2, y + 2, fillWidth, height - 4);

  u8g2.setFont(u8g2_font_7x13_mf);
  u8g2.setFontMode(1);
  char percBuffer[5];
  sprintf(percBuffer, "%d%%", percentage);
  u8g2.drawStr(x - u8g2.getUTF8Width(percBuffer) - 2, y + height / 2 + u8g2.getFontAscent() / 2 - 1, percBuffer);
}

void drawWiFiIcon() {
  int x_start = 5;
  int y_bottom = 15;
  int bar_width = 3;
  int bar_spacing = 2;
  if (wifiConnected) {
    u8g2.drawBox(x_start, y_bottom - 4, bar_width, 4);
    u8g2.drawBox(x_start + bar_width + bar_spacing, y_bottom - 7, bar_width, 7);
    u8g2.drawBox(x_start + 2 * (bar_width + bar_spacing), y_bottom - 10, bar_width, 10);
    u8g2.drawBox(x_start + 3 * (bar_width + bar_spacing), y_bottom - 13, bar_width, 13);
  }
}