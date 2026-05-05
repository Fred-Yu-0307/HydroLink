#include <Firebase_ESP_Client.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <addons/RTDBHelper.h>
#include <addons/TokenHelper.h>
#include <esp_wifi.h>
#include <time.h>
// FOR MY WIRELESS UPLOAD
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

// Provide the token status callbacks when using Anonymous sign-in
void tokenStatusCallback(TokenInfo info);

//  Configuration Constants
const char *AP_SSID = "HydroLink_Setup";
const char *AP_PASSWORD = "password123";
const int CONFIG_PORTAL_TIMEOUT_SECONDS = 60;

// Pin Definitions for ESP32-S3
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 8
#define ULTRASONIC_TRIG_PIN 16
#define ULTRASONIC_ECHO_PIN 18
#define SOLENOID_PIN 13
#define BATTERY_SENSE_PIN 1 // Safe Analog Input (ADC1_CH3)
#define FLOW_SENSOR_PIN 4  // Requires 5V-to-3.3V voltage divider

// Firebase configuration
#define FIREBASE_API_KEY "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA"
#define FIREBASE_DATABASE_URL                                                  \
  "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app/"

const float CALIBRATION_FACTOR = 450.0;

volatile unsigned long flowPulseCount = 0; // Total pulses since boot
unsigned long startRefillPulseCount = 0;   // Pulses at start of refill

// Tracking refill
float totalLiters = 0.0;
float currentRefillFlowRateSum = 0.0;
int flowRateSamples = 0;

bool persistentRefillEnabled =
    false; // Setting from Firebase for continuous refill if interrupted
bool isRefillPending =
    false; // Tracks if a refill was interrupted and needs to resume

unsigned long lastVerifiedPulseCount = 0;
unsigned long pulseVerificationTime = 0;
float expectedLiters = 0;

void IRAM_ATTR flowPulseISR() {
  static unsigned long lastInterruptTime = 0;
  unsigned long now = micros();

  // Reduced debounce from 2000us to 200us.
  if (now - lastInterruptTime > 200) {
    flowPulseCount++;
    lastInterruptTime = now;
  }
}

// NTP configuration
const long gmtOffset_sec = 28800; // GMT+8 for Philippines
const int daylightOffset_sec = 0;

//  OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// U8g2 Display Object for a 128x64 SH1106, using full buffer, hardware I2C.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Voltage Divider Config for battery sense
const float Rtop = 20000.0; // 20kΩ (from Battery+ to Pin 1)
const float Rbot = 10000.0; // 10kΩ (from Pin 1 to GND)

// Battery voltage limits for 2S Li-ion pack
const float MIN_BATTERY_VOLTAGE = 6.4; // 0%
const float MAX_BATTERY_VOLTAGE = 7.7; // 100%

//  Refill State Variables
bool solenoidState = false; // Tracks if solenoid is ON
bool isRefilling = false;
unsigned long refillStartTime = 0;
float currentRefillLiters = 0.0;

//  Firebase Status Variables to be updated
unsigned long lastRefillDurationMs = 0;
float lastRefillLiters = 0.0;
float lastRefillAvgFlowRateLPM = 0.0;
float stopWaterLevelCm = 0.0;

//  Refill History Variables
float startWaterDistanceCm = 0.0;
String refillEventType = "Automatic";
String refillStatus = "";
String actionLogDetails = "";

//  Failed Refill Tracking
bool inFailedRefillState = false;
String currentFailedHistoryKey = "";
int failedAttemptNumber = 0;

//  Failed Notification Tracking
bool hasSentFailedNotification = false;

unsigned long lastFlowDetectedTime =
    0; // Tracks last real flow; reset in openSolenoid()
bool noWaterWarningActive = false;

// Icon Data
const unsigned char PROGMEM water_droplet_icon_30x54[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x1f, 0x00,
    0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01, 0xff, 0x00,
    0x00, 0x03, 0xff, 0x00, 0x00, 0x07, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00,
    0x00, 0x1f, 0xff, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x00,
    0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x00, 0x07, 0xff, 0xff, 0x00,
    0x0f, 0xff, 0xff, 0x00, 0x1f, 0xff, 0xff, 0x00, 0x3f, 0xff, 0xff, 0x00,
    0x7f, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
    0xff, 0xff, 0xff, 0x00, 0x7f, 0xff, 0xff, 0x00, 0x3f, 0xff, 0xff, 0x00,
    0x1f, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x00, 0x07, 0xff, 0xff, 0x00,
    0x03, 0xff, 0xff, 0x00, 0x01, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x00,
    0x00, 0x3f, 0xff, 0x00, 0x00, 0x1f, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00,
    0x00, 0x07, 0xff, 0x00, 0x00, 0x03, 0xff, 0x00, 0x00, 0x01, 0xff, 0x00,
    0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x1f, 0x00,
    0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//  Global Variables
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
int refillRetrySeconds = 20;

unsigned long lastNoWaterErrorTime =
    0; // Variable to track when the last error occurred

unsigned long lastFlowCalculationTime = 0;
float currentFlowRateLPM = 0.0; // Separate from the ISR counting variable
unsigned long lastPulseCountForFlow = 0;

//  MANUAL REFILL VARIABLES
int manualTargetLevel = 100;  
bool isManualRefilling = false;  
unsigned long lastManualCheck = 0; // Timer to prevent spamming database

int startWaterPercentage = 0;

int batteryPercentage = 0;
bool drumHeightInitialized = false;
float drumHeightCm = 0.0;
float systemEstimatedVolume = 50.0;

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
unsigned long lastWifiCheck = 0;
const unsigned long SENSOR_INTERVAL = 3000;
const unsigned long FIREBASE_INTERVAL = 3000;
const unsigned long SETTINGS_INTERVAL = 3000;

//  Function Prototypes
void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
float readUltrasonicCm();
void calculateWaterPercentage();
void setupFirebase();
void mapMacToFirebaseUid();
bool ensureFirebaseConnection();
void handleWiFiReconnection();
void loadSettingsFromPreferences();
void saveSettingsToPreferences();
bool checkDeviceLinkStatus();
void handleFactoryReset();
void debugPreferences();
void resetFirebaseAuth();
void openSolenoid();
void closeSolenoid(String stopReason);
float getFlowRate();
float calculateFlowRate();

//  OLED Display Function Prototypes
void displayHydroLinkAnimation();
void displayHomeScreen();
void displaySetupScreen();
void drawBatteryIcon(int percentage);
void drawWiFiIcon();
void logStatusToSerial();

void loadSettingsFromPreferences() {
  Serial.println("Loading settings from preferences...");

  debugPreferences();

  if (!preferences.begin("hydrolink", true)) {
    Serial.println(
        "Failed to open preferences namespace! Forcing factory reset.");
    handleFactoryReset();
    return;
  }

  //  Load Firebase UID
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
  refillRetrySeconds = preferences.getInt("refillRetry", 20);
  waterDistanceCm = preferences.getFloat("lastWaterDistance", 0.0);
  waterPercentage = preferences.getInt("lastWaterPercent", 0);
  waterAvailable = preferences.getBool("lastWaterAvailable", true);
  batteryPercentage = preferences.getInt("lastBattery", 100);
  isLinkedToUser = preferences.getBool("isLinked", false);
  persistentRefillEnabled = preferences.getBool("persistRefill", false);
  isRefillPending = preferences.getBool("refillPending", false);

  drumHeightInitialized = (drumHeightCm > 0);
  isDeviceFullyConfigured = drumHeightInitialized;

  preferences.end();

  Serial.println("=== Settings Loaded from Preferences ===");
  Serial.println("Firebase UID: " + (deviceFirebaseId.length() > 0
                                         ? deviceFirebaseId
                                         : String("NOT_FOUND")));
  Serial.println("Drum height: " + String(drumHeightCm) + " cm");
  Serial.println("Refill threshold: " + String(refillThresholdPercentage) +
                 "%");
  Serial.println("Max fill level: " + String(maxFillLevelPercentage) + "%");
  Serial.println("Device configured: " +
                 String(isDeviceFullyConfigured ? "Yes" : "No"));
  Serial.println("Device linked: " + String(isLinkedToUser ? "Yes" : "No"));
}

void saveSettingsToPreferences() {
  preferences.begin("hydrolink", false);

  preferences.putFloat("drumHeight", drumHeightCm);
  preferences.putInt("refillThreshold", refillThresholdPercentage);
  preferences.putInt("maxFillLevel", maxFillLevelPercentage);
  preferences.putInt("refillRetry", refillRetrySeconds);
  preferences.putFloat("lastWaterDistance", waterDistanceCm);
  preferences.putInt("lastWaterPercent", waterPercentage);
  preferences.putBool("lastWaterAvailable", waterAvailable);
  preferences.putInt("lastBattery", batteryPercentage);
  preferences.putBool("isLinked", isLinkedToUser);
  preferences.putBool("persistRefill", persistentRefillEnabled);
  preferences.putBool("refillPending", isRefillPending);

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
    Serial.println("Device link status changed to: " +
                   String(isLinkedToUser ? "LINKED" : "UNLINKED"));
  }

  return isLinkedToUser;
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
  refillRetrySeconds = 20;
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
    Serial.println("Drum height already initialized: " + String(drumHeightCm) +
                   " cm");
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

  String heightPath =
      "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
  Serial.println("Reading drum height from: " + heightPath);

  if (Firebase.RTDB.getFloat(&fbdo, heightPath)) {
    if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_float ||
        fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_integer) {
      float newDrumHeight = fbdo.floatData();
      if (newDrumHeight > 0) {
        drumHeightCm = newDrumHeight;
        drumHeightInitialized = true;
        isDeviceFullyConfigured = true;
        saveSettingsToPreferences();

        Serial.println("Drum height fetched from Firebase: " +
                       String(newDrumHeight));
        return true;
      }
    }
  }
  return false;
}

float readUltrasonicCm() {
  // Take 5 readings and return the median to filter out noise
  const int NUM_SAMPLES = 5;
  float samples[NUM_SAMPLES];
  int validCount = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(25);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    unsigned long duration = pulseInLong(ULTRASONIC_ECHO_PIN, HIGH, 60000);
    if (duration > 0) {
      float rawDistanceCm = duration * 0.034 / 2.0;
      float adjustedDistanceCm = rawDistanceCm - 7.5;
      if (adjustedDistanceCm < 0)
        adjustedDistanceCm = 0.0;
      samples[validCount++] = adjustedDistanceCm;
    }
    delay(30); // Short delay between readings
  }

  if (validCount == 0) {
    Serial.println("WARNING: Ultrasonic sensor timeout (0 valid out of 5)");
    return -1.0;
  }

  // Sort valid samples (bubble sort on small array)
  for (int i = 0; i < validCount - 1; i++) {
    for (int j = 0; j < validCount - i - 1; j++) {
      if (samples[j] > samples[j + 1]) {
        float tmp = samples[j];
        samples[j] = samples[j + 1];
        samples[j + 1] = tmp;
      }
    }
  }

  float median = samples[validCount / 2];
  Serial.printf("Ultrasonic: %d/%d valid, median=%.1fcm\n", validCount,
                NUM_SAMPLES, median);

  // Return median
  return median;
}

float calculateFlowRate() {
  unsigned long now = millis();

  // Calculate every second
  if (now - lastFlowCalculationTime >= 1000) {
    // Disable interrupts while reading the volatile variable
    noInterrupts();
    unsigned long currentPulses = flowPulseCount;
    interrupts();

    // Guard against unsigned underflow
    unsigned long pulseDiff = (currentPulses >= lastPulseCountForFlow)
                                  ? (currentPulses - lastPulseCountForFlow)
                                  : 0;

    // Accumulate lifetime liters (was in getFlowRate before, now centralised heree
    totalLiters += (float)pulseDiff / CALIBRATION_FACTOR;

    // DETECT PULSE LOSS
    if (solenoidState) {
      // Every 10 seconds, verify pulse count
      if (now - pulseVerificationTime > 10000) {
        pulseVerificationTime = now;
        unsigned long pulsesThisPeriod = currentPulses - lastVerifiedPulseCount;
        float litersThisPeriod = (float)pulsesThisPeriod / CALIBRATION_FACTOR;

        Serial.printf("VERIFY: %lu pulses in 10s = %.2f L\n", pulsesThisPeriod,
                      litersThisPeriod);

        // If we're getting fewer pulses than expected, adjust calibration
        // dynamically
        if (expectedLiters > 0) {
          float actualLiters = litersThisPeriod;
          float ratio = actualLiters / expectedLiters;
          if (ratio < 0.8) { // If we're missing more than 20% of pulses
            Serial.printf(" PULSE LOSS DETECTED! Ratio: %.2f\n", ratio);
            // You could dynamically adjust calibration factor here
          }
        }

        lastVerifiedPulseCount = currentPulses;
        expectedLiters = 0; // Reset expected
      }
    }

    // Calculate flow rate in L/min
    if (pulseDiff > 0) {
      currentFlowRateLPM = ((float)pulseDiff * 60.0f) / CALIBRATION_FACTOR;

      // Update expected liters for next verification
      expectedLiters += (float)pulseDiff / CALIBRATION_FACTOR;

      if (solenoidState) {
        Serial.printf("Flow: %lu pulses/sec = %.2f L/min, Total: %lu\n",
                      pulseDiff, currentFlowRateLPM, currentPulses);
      }
    } else {
      currentFlowRateLPM = 0.0;
    }

    // Accumulate flow rate average ONLY here (getFlowRate is now a wrapper)
    if (solenoidState) {
      currentRefillFlowRateSum += currentFlowRateLPM;
      flowRateSamples++;
    }

    lastPulseCountForFlow = currentPulses;
    lastFlowCalculationTime = now;
  }

  return currentFlowRateLPM;
}

void calculateWaterPercentage() {
  if (drumHeightCm <= 0) {
    waterPercentage = 0;
    waterAvailable = false;
    return;
  }

  // Calculate actual water height in the drum
  float waterHeightInDrum = drumHeightCm - waterDistanceCm;

  // Add deadzone: If water is less than 1.5 cm from the bottom, consider it
  // empty
  if (waterHeightInDrum < 1.5) {
    waterHeightInDrum = 0.0;
  }

  // Ensure we don't go negative or over drum height
  waterHeightInDrum = constrain(waterHeightInDrum, 0, drumHeightCm);

  // The percentage must be relative to the TOTAL drum volume, not
  // maxFillLevelPercentage!
  waterPercentage = (waterHeightInDrum / drumHeightCm) * 100;
  waterPercentage = constrain(waterPercentage, 0, 100);

  // Decide if water is available based on threshold
  waterAvailable = (waterPercentage > refillThresholdPercentage);

  Serial.printf("Water: %d%% (Distance from sensor: %.1fcm, Height in drum: "
                "%.1fcm / %.1fcm)\n",
                waterPercentage, waterDistanceCm, waterHeightInDrum,
                drumHeightCm);
}

void logRefillHistory() {
  // Only log if Firebase is ready, device is identified, and a refill event actually occurred (duration > 0)
  if (!Firebase.ready() || deviceFirebaseId.length() == 0 ||
      lastRefillDurationMs == 0) {
    return;
  }

  // Calculate Starting and Ending Levels (cm and %)

  // Calculate BEFORE level in cm
  float waterLevelCmStart = drumHeightCm - startWaterDistanceCm;
  waterLevelCmStart = constrain(waterLevelCmStart, 0, drumHeightCm);

  // Calculate AFTER level in cm
  float waterLevelCmEnd = drumHeightCm - stopWaterLevelCm;
  waterLevelCmEnd = constrain(waterLevelCmEnd, 0, drumHeightCm);

  // Use the same formula as calculateWaterPercentage() — no maxFillLevelPercentage scaling Also use the captured startWaterPercentage directly for accuracy
  int startWaterPercentageCalc =
      startWaterPercentage; // Captured at valve open time
  startWaterPercentageCalc = constrain(startWaterPercentageCalc, 0, 100);

  //  2. Build the JSON object for refillHistory
  String basePath = "hydrolink/devices/" + deviceFirebaseId + "/refillHistory";

  FirebaseJson historyJson;

  // Refill Details
  historyJson.set("type", refillEventType);
  historyJson.set("beforeLevelPct", startWaterPercentageCalc);
  historyJson.set("afterLevelPct", waterPercentage);

  // amountLitersAdded is strictly based on flow sensor pulse delta (startRefillPulseCount → flowPulseCount), calculated in closeSolenoid()
  float litersToLog = lastRefillLiters;

  historyJson.set("amountLitersAdded", String(litersToLog, 2));
  historyJson.set("durationMin", String(lastRefillDurationMs / (1000.0 * 60.0),
                                        1)); // Store in minutes
  historyJson.set("status", refillStatus);
  historyJson.set("actionsLog", actionLogDetails);

  // Detailed CM fields merged into the main history
  historyJson.set("beforeLevelCm", String(waterLevelCmStart, 1));
  historyJson.set("afterLevelCm", String(waterLevelCmEnd, 1));

  // Settings Snapshot (Config)
  FirebaseJson configJson;
  configJson.set("drumHeightCm", String(drumHeightCm, 1));
  configJson.set("maxFillLevelPercentage", maxFillLevelPercentage);
  configJson.set("refillThresholdPercentage", refillThresholdPercentage);

  historyJson.set("config", configJson);

  // PUSH or UPDATE the log entry to Firebase
  if (refillStatus == "Failed") {
    if (!inFailedRefillState) {
      inFailedRefillState = true;
      failedAttemptNumber = 1;

      historyJson.set("firstFailedRefillTimestamp/.sv", "timestamp");
      historyJson.set("lastFailedRefillTimestamp/.sv", "timestamp");
      historyJson.set("attemptNumber", failedAttemptNumber);
      historyJson.set(
          "timestamp/.sv",
          "timestamp"); // Retain primary timestamp for general sorting/UI

      if (Firebase.RTDB.pushJSON(&fbdo, basePath, &historyJson)) {
        Serial.println("First Failed refill history logged successfully!");
        currentFailedHistoryKey = fbdo.pushName();
        lastRefillDurationMs = 0;
      } else {
        Serial.println("First Failed refill history log FAILED: " +
                       fbdo.errorReason());
        lastRefillDurationMs = 0;
      }
    } else {
      failedAttemptNumber++;

      String updatePath = basePath + "/" + currentFailedHistoryKey;
      FirebaseJson updateJson;
      updateJson.set("lastFailedRefillTimestamp/.sv", "timestamp");
      updateJson.set("attemptNumber", failedAttemptNumber);
      updateJson.set("afterLevelPct", waterPercentage);
      updateJson.set("afterLevelCm", String(waterLevelCmEnd, 1));

      if (Firebase.RTDB.updateNode(&fbdo, updatePath, &updateJson)) {
        Serial.println("Failed refill history updated (Attempt " +
                       String(failedAttemptNumber) + ")");
        lastRefillDurationMs = 0;
      } else {
        Serial.println("Failed refill history update FAILED: " +
                       fbdo.errorReason());
        lastRefillDurationMs = 0;
      }
    }
  } else {
    // Competed or Interrupted
    inFailedRefillState = false;
    currentFailedHistoryKey = "";
    failedAttemptNumber = 0;

    historyJson.set("timestamp/.sv", "timestamp");

    bool pushStandard = Firebase.RTDB.pushJSON(&fbdo, basePath, &historyJson);
    if (pushStandard) {
      Serial.println("Refill history logged successfully!");
      lastRefillDurationMs = 0;
    } else {
      Serial.println("Refill history log FAILED: " + fbdo.errorReason());
      lastRefillDurationMs = 0;
    }
  }
}

void openSolenoid() {
  esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
  digitalWrite(SOLENOID_PIN, HIGH);
  solenoidState = true;
  delay(150);

  waterAvailable = true;

  // START REFILL RECORDING
  refillStartTime = millis();
  currentRefillLiters = 0.0;
  currentRefillFlowRateSum = 0.0;
  flowRateSamples = 0;

  // Capture current pulse count with interrupts disabled
  noInterrupts();
  startRefillPulseCount = flowPulseCount;
  interrupts();

  Serial.printf("Refill started - Initial pulse count: %lu\n",
                startRefillPulseCount);

  lastFlowDetectedTime = 0;
  noWaterWarningActive = false;

  // Capture STARTING STATE & TYPE
  startWaterDistanceCm = waterDistanceCm;
  startWaterPercentage = waterPercentage;

  //  CHECK IF MANUAL OR AUTO
  if (isManualRefilling) {
    refillEventType = "Manual";
    actionLogDetails = "Manual Command";
    Serial.println("Solenoid OPEN (Manual) - Refilling...");
  } else {
    refillEventType = "Automatic";
    actionLogDetails = "NC Solenoid Open via Auto-Threshold";
    Serial.println("Solenoid OPEN (Automatic) - Refilling...");
  }
}

void closeSolenoid(String stopReason) {
  digitalWrite(SOLENOID_PIN, LOW);
  solenoidState = false;
  esp_wifi_set_ps(WIFI_PS_NONE);

  if (refillStartTime > 0) {
    // Calculate Duration
    lastRefillDurationMs = millis() - refillStartTime;

    // Calculate Average Flow Rate
    if (flowRateSamples > 0) {
      lastRefillAvgFlowRateLPM = currentRefillFlowRateSum / flowRateSamples;
    } else {
      lastRefillAvgFlowRateLPM = 0.0;
    }

    // Calculate total liters using pulse count difference
    noInterrupts();
    unsigned long currentTotalPulses = flowPulseCount;
    interrupts();

    // Guard against wrap-around (unsigned subtraction)
    unsigned long pulsesDuringRefill =
        (currentTotalPulses >= startRefillPulseCount)
            ? (currentTotalPulses - startRefillPulseCount)
            : 0;
    lastRefillLiters = (float)pulsesDuringRefill / CALIBRATION_FACTOR;

    // Debug output with more details
    Serial.println("\n=== REFILL SUMMARY ===");
    Serial.printf("Start pulses: %lu\n", startRefillPulseCount);
    Serial.printf("End pulses: %lu\n", currentTotalPulses);
    Serial.printf("Pulses during refill: %lu\n", pulsesDuringRefill);
    Serial.printf("Calibration factor: %.1f pulses/L\n", CALIBRATION_FACTOR);
    Serial.printf("Calculated liters: %.2f L\n", lastRefillLiters);
    Serial.printf("Duration: %.1f seconds\n", lastRefillDurationMs / 1000.0);
    Serial.printf("Avg flow rate: %.2f L/min\n", lastRefillAvgFlowRateLPM);
    Serial.printf("Stop reason: %s\n", stopReason.c_str());
    Serial.println("=====================\n");

    // COMPUTE ESTIMATED DRUM VOLUME
    // Calculate how much the percentage changed during this specific refill
    int percentageRefilled = waterPercentage - startWaterPercentage;

    // Only calculate if at least 30% changed and 0.5L flowed
    // (large refills give much more accurate volume estimation)
    if (percentageRefilled >= 30 && lastRefillLiters > 0.5) {
      // Formula: (Liters Refilled / Percentage Change) * 100
      float estimatedTotalVolume =
          (lastRefillLiters / (float)percentageRefilled) * 100.0;

      // Save the new calculated total volume to Firebase under the device
      // status Ensure deviceFirebaseId and fbdo are correctly scoped here
      String volumePath = "hydrolink/devices/" + deviceFirebaseId +
                          "/status/estimatedTotalVolume";
      if (Firebase.RTDB.setFloat(&fbdo, volumePath.c_str(),
                                 estimatedTotalVolume)) {
        systemEstimatedVolume = estimatedTotalVolume; // Update local fallback
        Serial.printf(
            " New Estimated Drum Volume: %.2f L pushed to Firebase.\n",
            estimatedTotalVolume);
      } else {
        Serial.printf(" Failed to push volume to Firebase: %s\n",
                      fbdo.errorReason().c_str());
      }
    } else {
      Serial.println(" Volume calculation skipped (refill too small to "
                     "accurately measure).");
    }
    // ==========================================

    stopWaterLevelCm = waterDistanceCm;
    actionLogDetails = stopReason;

    // Set status based on stop reason
    if (stopReason == "No Source Water") {
      refillStatus = "Failed";
    } else if (stopReason == "Water Stopped") {
      refillStatus = "Interrupted";
    } else if (stopReason == "Target Reached") {
      refillStatus = "Completed";
    } else {
      refillStatus = "Interrupted";
    }

    // Save to history
    logRefillHistory();

    refillStartTime = 0;
  }

  currentRefillFlowRateSum = 0.0;
  flowRateSamples = 0;
}

void updateFirebaseData() {
  if (!Firebase.ready() || deviceFirebaseId.length() == 0) {
    return;
  }

  // Use deviceFirebaseId as the identifier
  String basePath = "hydrolink/devices/" + deviceFirebaseId + "/status";

  FirebaseJson json;

  // for wifi name in the  web settings
  json.set("wifiSSID", WiFi.SSID());
  //

  // Calculate actual water height in the drum
  float actualWaterLevelCm = drumHeightCm - waterDistanceCm;
  actualWaterLevelCm = constrain(actualWaterLevelCm, 0, drumHeightCm);
  json.set("currentWaterLevelCm", String(actualWaterLevelCm, 1));
  json.set("waterPercentage", waterPercentage);
  json.set("waterAvailable", waterAvailable);
  json.set("batteryPercentage", batteryPercentage);
  json.set("lastUpdated/.sv", "timestamp");
  json.set("deviceId", deviceFirebaseId);
  json.set("macAddress", deviceMacAddress);
  json.set("drumHeightCm", drumHeightCm);
  json.set("isConfigured", isDeviceFullyConfigured);
  json.set("isLinked", isLinkedToUser);

  json.set("lastRefillDurationSec", lastRefillDurationMs / 1000.0);
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

      if (json->get(jsonData, "persistentRefillEnabled")) {
        if (persistentRefillEnabled != jsonData.to<bool>()) {
          persistentRefillEnabled = jsonData.to<bool>();
          settingsChanged = true;
        }
      }

      if (json->get(jsonData, "refillRetrySeconds")) {
        if (refillRetrySeconds != jsonData.to<int>()) {
          refillRetrySeconds = jsonData.to<int>();
          settingsChanged = true;
        }
      }

      if (settingsChanged) {
        Serial.println("Settings updated from Firebase:");
        Serial.println(" Refill threshold: " +
                       String(refillThresholdPercentage));
        Serial.println(" Max fill level: " + String(maxFillLevelPercentage));
        Serial.println(" Refill retry sec: " + String(refillRetrySeconds));
        Serial.println(" Drum height: " + String(drumHeightCm));
        saveSettingsToPreferences();
      }
      json->clear();
    }
  }
}

void setSystemStatus(String status) {
  if (!Firebase.ready() || deviceFirebaseId.length() == 0)
    return;
  String statusPath =
      "hydrolink/devices/" + deviceFirebaseId + "/status/systemStatus";
  if (Firebase.RTDB.setString(&fbdo, statusPath, status)) {
    Serial.println("System status set to: " + status);
  }
}

// Check if Web App requested a Drum Measurement
void checkManualDrumMeasurement() {
  if (!isLinkedToUser || !Firebase.ready() || deviceFirebaseId.length() == 0) {
    return;
  }

  String path =
      "hydrolink/devices/" + deviceFirebaseId + "/settings/measureDrum";

  // Check if the measureDrum flag is set to true
  if (Firebase.RTDB.getBool(&fbdo, path)) {
    if (fbdo.dataType() == "boolean" && fbdo.boolData() == true) {
      Serial.println("Command received: Scanning Drum Height...");

      // Perform the measurement
      measureDrumHeight();

      // Reset the trigger flag to false so it doesn't loop
      Firebase.RTDB.setBool(&fbdo, path, false);
    }
  }
}

// Execute the Measurement Logic
void measureDrumHeight() {
  // Display status on OLED (Optional)
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tf);
  u8g2.drawStr(10, 30, "Scanning...");
  u8g2.sendBuffer();

  float totalHeight = 0;
  int validReadings = 0;
  int attempts = 10;

  // Take average of 10 readings for stability
  for (int i = 0; i < attempts; i++) {
    float reading = readUltrasonicCm();
    if (reading > 0 && reading < 300) { // Filter invalid readings
      totalHeight += reading;
      validReadings++;
    }
    delay(100); // Short delay between pings
  }

  if (validReadings > 0) {
    float avgHeight = totalHeight / validReadings;

    // Apply Offset if necessary (Sensor to Drum Top distance) avgHeight += 2.0;

    drumHeightCm = avgHeight;
    drumHeightInitialized = true;
    isDeviceFullyConfigured = true;

    Serial.print("Measured Drum Height: ");
    Serial.print(drumHeightCm);
    Serial.println(" cm");

    // 1. Save to Local Preferences
    saveSettingsToPreferences();

    // 2. Upload Result to Firebase (This updates the Web App UI)
    String path =
        "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
    if (Firebase.RTDB.setFloat(&fbdo, path, drumHeightCm)) {
      Serial.println("Drum Height updated in Firebase!");
    } else {
      Serial.println("Failed to upload height: " + fbdo.errorReason());
    }

    // 3. Force an immediate update of system status
    calculateWaterPercentage();
    updateFirebaseData();

  } else {
    Serial.println(
        "Error: Could not measure drum height (Sensor Timeout/Invalid)");
  }

  // Return to home screen
  displayHomeScreen();
}

// Global variable to remember the last stable level
int lastStableBatteryLevel = 100;

int readBatteryPercentage() {
  // 1. CHECK: Is the Solenoid ON?
  // If the solenoid is active, voltage drops temporarily. Return last good
  // value.
  if (digitalRead(SOLENOID_PIN) == HIGH) {
    return lastStableBatteryLevel;
  }

  // 2. Take Average Readings
  const int samples = 10;
  float totalVoltageSum = 0;

  for (int i = 0; i < samples; i++) {
    int adcValue = analogRead(BATTERY_SENSE_PIN);

    // Convert ADC to Pin Voltage (0-3.3V)
    float voltageAtPin = (adcValue / 4095.0) * 3.3;

    // Calculate Actual Battery Voltage
    float currentSampleVoltage = voltageAtPin * ((Rtop + Rbot) / Rbot);

    totalVoltageSum += currentSampleVoltage;
    delay(10);
  }

  // 3. Calculate Average Voltage
  float avgVoltage = totalVoltageSum / samples;

  // 4. Convert to Percentage (Where 8.0V is 100%)
  int percentage = (int)((avgVoltage - MIN_BATTERY_VOLTAGE) /
                         (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100);

  // Ensure it stays between 0-100%
  percentage = constrain(percentage, 0, 100);

  // 5. Update the "Last Stable" variable
  lastStableBatteryLevel = percentage;

  // 6. Debug Print
  Serial.print("Battery Voltage: ");
  Serial.print(avgVoltage, 2);
  Serial.print(" V | ");
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
    if (WiFi.status() == WL_CONNECTED) {
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
  auth.user.email = "hydroLinkDevice_004@hydrolink.com";
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
    Serial.println(" Failed to retrieve UID from Firebase");
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
  Serial.println("Namespace exists: " +
                 String(preferences.isKey("drumHeight") ? "Yes" : "No"));
  Serial.println("Firebase UID: '" +
                 preferences.getString("firebaseUid", "NOT_FOUND") + "'");
  Serial.println("Drum Height: " +
                 String(preferences.getFloat("drumHeight", -999.0)));
  Serial.println("Refill Threshold: " +
                 String(preferences.getInt("refillThreshold", -999)));
  Serial.println("Refill Retry: " +
                 String(preferences.getInt("refillRetry", -999)));
  Serial.println("Linked Status: " +
                 String(preferences.getBool("isLinked", false) ? "Yes" : "No"));
  Serial.println("Free entries: " + String(preferences.freeEntries()));
  preferences.end();
}

void logStatusToSerial() {

  // Calculate actual water level in CM for printing
  float actualWaterLevelCm = drumHeightCm - waterDistanceCm;
  actualWaterLevelCm = constrain(actualWaterLevelCm, 0, drumHeightCm);

  // Print Water Level and Status (COMBINED LINE)
  Serial.printf("Water: %d%% (%.1fcm / %.1fcm) | Water Status: ",
                waterPercentage, actualWaterLevelCm, drumHeightCm);
  // Print the availability and end the line
  if (waterAvailable) {
    Serial.println("Available");
  } else {
    Serial.println("Unavailable");
  }

  // Print Battery Status
  Serial.printf("Battery Voltage: %.2f V  |  %d%%\n", 0.00,
                batteryPercentage);
}

void mapMacToFirebaseUid() {
  if (!Firebase.ready() || deviceFirebaseId.isEmpty() ||
      deviceMacAddress.isEmpty())
    return;

  String path = "hydrolink/macToFirebaseUid/" + deviceMacAddress;
  // Use setString instead of updateNode for a simple key-value pair
  if (Firebase.RTDB.setString(&fbdo, path, deviceFirebaseId)) {
    Serial.println("MAC to UID mapping updated");
  } else {
    Serial.println("MAC mapping failed: " + fbdo.errorReason());
  }
}

//  WiFiManager Callbacks
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered WiFi config mode");
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
}

void saveConfigCallback() { Serial.println("WiFi configuration saved!"); }

void playSoftStartSequence() {
  Serial.println("Initiating Soft Start...");

  // 1. Play your Logo Animation first (Preserving original call)
  displayHydroLinkAnimation();

  // 2. Play Loading Bar Sequence
  for (int i = 0; i <= 100; i += 5) {
    u8g2.clearBuffer();

    //  DRAW TITLE (Bigger & Centered)
    u8g2.setFont(u8g2_font_helvB14_tf); // Big, bold font
    const char *title = "HYDROLINK";
    int titleWidth = u8g2.getStrWidth(title);
    int titleX =
        (128 - titleWidth) / 2; // (Screen Width - Text Width) / 2 = Centered X
    u8g2.drawStr(titleX, 25, title);

    //  DRAW LOADING BAR
    u8g2.drawFrame(10, 40, 108, 12); // The outer frame
    int w = map(i, 0, 100, 0, 104);  // Calculate fill width
    u8g2.drawBox(12, 42, w, 8);      // The inner fill

    //  DRAW STATUS TEXT (Small & Centered)
    u8g2.setFont(u8g2_font_5x7_tf); // Tiny font for status
    const char *status = "";

    if (i < 35)
      status = "Stabilizing Power...";
    else if (i < 75)
      status = "Calibrating Sensors...";
    else
      status = "Starting System...";

    int statusWidth = u8g2.getStrWidth(status);
    int statusX = (128 - statusWidth) / 2;
    u8g2.drawStr(statusX, 62, status);

    u8g2.sendBuffer();

    // DELAY: Keeps power draw low while capacitors charge
    delay(80);
  }
}

//  UPDATED OTA SETUP (High Stability Mode)
void setupOTA() {
  ArduinoOTA.setHostname("HydroLink-Device");

  ArduinoOTA
      .onStart([]() {
        // Stop the solenoid immediately
        digitalWrite(SOLENOID_PIN, LOW);
        isRefilling = false;
        isManualRefilling = false;

        // Clear Serial buffer
        Serial.println("OTA Update Starting...");

        // Prepare Display
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(15, 30, "SYSTEM UPDATE");
        u8g2.drawStr(10, 45, "Starting Transfer...");
        u8g2.sendBuffer();

        // Firebase is NOT stopped manually, but we stop calling it in the loop via a flag if needed.
      })
      .onEnd([]() {
        u8g2.clearBuffer();
        u8g2.drawStr(15, 35, "SUCCESS!");
        u8g2.sendBuffer();
        delay(1000);
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        int p = (progress / (total / 100));

        // Only update screen every 5% to prevent i2c crashes
        if (p % 5 == 0) {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(30, 20, "Updating...");
          u8g2.drawFrame(10, 35, 108, 12);
          int w = map(p, 0, 100, 0, 104);
          u8g2.drawBox(12, 37, w, 8);
          u8g2.sendBuffer();
        }
      })
      .onError([](ota_error_t error) {
        u8g2.clearBuffer();
        u8g2.drawStr(10, 30, "Update Error!");
        u8g2.sendBuffer();
        delay(2000);
      });

  ArduinoOTA.begin();
}

void setup() {
  //  1. SERIAL & PINS (Low Power Initialization)
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== HydroLink S3 (N16R8) Starting ===");

  // Force Solenoid OFF immediately for safety
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  pinMode(FLOW_SENSOR_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseISR,
                  FALLING);

  // verify flow sensor pin state at boot
  Serial.printf("Flow sensor initialized on GPIO %d (state: %s)\n",
                FLOW_SENSOR_PIN, digitalRead(FLOW_SENSOR_PIN) ? "HIGH" : "LOW");
  Serial.printf("Pulse count at boot: %lu\n", flowPulseCount);

  //  The display for the capacitor to charge up yet so that the it will stabilize
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();

  // Charge capacitors while showing animation
  playSoftStartSequence();

  // SETTINGS
  loadSettingsFromPreferences();

  //WIFI
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SECONDS);

  wm.setRestorePersistent(true);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(20, 30, "Connecting to");
  u8g2.drawStr(35, 45, "WiFi...");
  u8g2.sendBuffer();

  Serial.println("Connecting to WiFi...");
  wifiConnected = wm.autoConnect(AP_SSID, AP_PASSWORD);

  //NETWORK SERVICES
  if (wifiConnected) {
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    deviceMacAddress = WiFi.macAddress();

    // ACTIVATE MDNS (Fixes Wireless Port disappearance)
    if (MDNS.begin("HydroLink-Device")) {
      MDNS.addService("arduino", "tcp", 3232);
      Serial.println("mDNS responder started: HydroLink-Device.local");
    }

    configTime(28800, 0, "ph.pool.ntp.org", "asia.pool.ntp.org",
               "time.google.com");

    Serial.print("Waiting for PH Time Sync");
    time_t now = time(nullptr);
    int retry = 0;
    while (now < 1000000 && retry < 40) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
      retry++;
    }
    Serial.println("");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time. Using internal clock.");
    } else {
      Serial.println("Time synchronized successfully!");
    }

    // OTA & FIREBASE
    setupOTA();
    setupFirebase();

    // Firebase Handshake
    unsigned long firebase_start_time = millis();
    while (!Firebase.ready() && millis() - firebase_start_time < 15000) {
      Serial.print(".");
      delay(500);
    }

    if (deviceFirebaseId.length() == 0) {
      // Fallback: try loading from preferences
      preferences.begin("hydrolink", true);
      deviceFirebaseId = preferences.getString("firebase_uid", "");
      preferences.end();
    }

    if (Firebase.ready() && deviceFirebaseId.length() > 0) {
      Serial.println("\nFirebase ready. UID: " + deviceFirebaseId);

      // Optimize buffers for N16R8 PSRAM
      fbdo.setBSSLBufferSize(4096, 1024);
      fbdo.setResponseSize(2048);

      setSystemStatus("online");

      if (auth.token.uid.length() > 0) {
        deviceFirebaseId = auth.token.uid.c_str();
        mapMacToFirebaseUid();
      }

      checkDeviceLinkStatus();
      if (!isLinkedToUser) {
        Serial.println("Waiting for user link...");
        unsigned long linkStartTime = millis();
        while (millis() - linkStartTime <
               60000) { // Reduced wait to 1 min for setup
          displaySetupScreen();
          if (checkDeviceLinkStatus())
            break;
          delay(5000);
        }
      }

      if (isLinkedToUser) {
        initializeDrumHeight();
        readFirebaseSettings();
      }
    } else {
      Serial.println("Firebase failed or ID missing.");
    }
  } else {
    Serial.println("Failed to connect to WiFi.");
  }

  config.token_status_callback = tokenStatusCallback;
  Serial.println("=== Setup Complete! ===");
}

// CHECK FIREBASE COMMAND
void checkManualRefillCommand() {
  // Check every 2 seconds
  if (millis() - lastManualCheck > 2000) {
    lastManualCheck = millis();

    // Ensure we have a valid ID before checking
    if (deviceFirebaseId == "")
      return;

    String path =
        "hydrolink/devices/" + deviceFirebaseId + "/control/manualRefill";
    FirebaseData cmdData;
    FirebaseJson json;

    if (Firebase.RTDB.getJSON(&cmdData, path)) {
      json.setJsonData(cmdData.jsonString());
      FirebaseJsonData result;

      json.get(result, "command");
      if (result.success) {
        bool cmd = result.boolValue;

        if (cmd) {

          // Get Target Level first
          FirebaseJsonData targetResult;
          json.get(targetResult, "targetLevel");
          if (targetResult.success)
            manualTargetLevel = targetResult.intValue;

          // Trigger Start (only if not already running)
          if (!isManualRefilling) {
            Serial.println("Manual Refill: START");
            isManualRefilling = true;
            isRefilling = true;

            //  Call openSolenoid() to handle setup
            openSolenoid();
          }
        } else {
          //  STOP COMMAND
          if (isManualRefilling) {
            Serial.println("Manual Refill: STOP");

            //Use closeSolenoid()
            // This function calculates liters used and saves to History
            closeSolenoid("User Stopped");

            isManualRefilling = false;
            isRefilling = false;
          }
        }
      }
    }
  }
}

//Send Notification
void sendFirebaseNotification(String title, String message, String type) {
  if (deviceFirebaseId == "")
    return;

  // Prevent spamming "Failed" notifications if we've already sent one in this sequence
  if (type == "error" || title.indexOf("Failed") != -1 ||
      message.indexOf("No Source Water") != -1) {
    if (hasSentFailedNotification) {
      Serial.println("Suppressed duplicate failed notification: " + title);
      return;
    }
    hasSentFailedNotification = true;
  } else if (type == "success" || title.indexOf("Completed") != -1 ||
             message.indexOf("Target Reached") != -1 || type == "info") {
    // Reset the notification flag only when a successful or standard info notification is processed
    hasSentFailedNotification = false;
  }

  String path = "hydrolink/devices/" + deviceFirebaseId + "/notifications";

  FirebaseJson json;
  json.set("title", title);
  json.set("message", message);
  json.set("type", type); // "info", "warning", "success", "error"
  json.set("timestamp/.sv", "timestamp");
  json.set("read", false);

  Serial.print("Sending Notification: ");
  Serial.println(title);

  // pushJSON automatically creates a unique key (e.g., -Nj7...)
  Firebase.RTDB.pushJSON(&fbdo, path, &json);
}

//  REFILL LOGIC
void runRefillLogic() {

  // Calculate current flow rate
  float currentFlow = calculateFlowRate();

  // REFILL PROCESS (Active)
  if (isRefilling) {
    digitalWrite(SOLENOID_PIN, HIGH); // Open Valve

    // Determine Target
    int currentTarget =
        isManualRefilling ? manualTargetLevel : maxFillLevelPercentage;

    // Update UI Status (Throttled every 2 seconds)
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 2000 && deviceFirebaseId != "") {
      lastStatusUpdate = millis();
      FirebaseData statusData;
      String status = isManualRefilling ? "REFILLING_MANUAL" : "REFILLING_AUTO";
      Firebase.RTDB.setString(&statusData,
                              "hydrolink/devices/" + deviceFirebaseId +
                                  "/status/refillStatus",
                              status);
    }

    //  CONTINUOUS NO WATER DETECTION
    unsigned long flowCheckDuration = millis() - refillStartTime;

    // Calculate total pulses since refill started
    noInterrupts();
    unsigned long totalPulsesThisRefill =
        flowPulseCount - startRefillPulseCount;
    interrupts();

    // Calculate total liters so far
    float litersSoFar = totalPulsesThisRefill / CALIBRATION_FACTOR;

    // Debug output every 5 seconds
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) {
      lastDebugTime = millis();
      Serial.printf("Refill Status - Time: %.1fs, Pulses: %lu, Liters: %.2f, "
                    "Flow Rate: %.2f L/min, Water: %d%%\n",
                    flowCheckDuration / 1000.0, totalPulsesThisRefill,
                    litersSoFar, currentFlowRateLPM, waterPercentage);
    }

    if (currentFlowRateLPM > 0.05) {
      // Real water flow detected - reset the no-water timer
      lastFlowDetectedTime = millis();
      noWaterWarningActive = false;
    }

    if (flowCheckDuration < 3000) {
      noWaterWarningActive = false;
    } else if (lastFlowDetectedTime == 0) {
      // Flow NEVER detected since solenoid opened — no water at source
      if (flowCheckDuration > 15000) {
        Serial.println(" ERROR: No water detected at all (0 flow in 15s)!");

        closeSolenoid("No Source Water");
        isRefilling = false;
        isManualRefilling = false;
        lastNoWaterErrorTime = millis();

        sendFirebaseNotification(
            "Refill Failed",
            "No Source Water detected! Refill attempts will continue every " +
                String(refillRetrySeconds) + " seconds.",
            "error");

        if (deviceFirebaseId != "") {
          FirebaseData safetyData;
          String basePath = "hydrolink/devices/" + deviceFirebaseId;
          Firebase.RTDB.setString(
              &safetyData, basePath + "/status/refillStatus", "NO_WATER");
          Firebase.RTDB.setBool(
              &safetyData, basePath + "/control/manualRefill/command", false);
        }
        return;
      }
    } else {
      // Flow WAS detected before — check if it stopped mid-refill
      unsigned long timeSinceFlow = millis() - lastFlowDetectedTime;

      if (timeSinceFlow > 10000 && !noWaterWarningActive) {
        noWaterWarningActive = true;
        Serial.println(" Flow stopped - warning active");
      }
      if (timeSinceFlow > 15000) {
        Serial.println(" Water stopped flowing for 15s - closing solenoid");

        closeSolenoid("Water Stopped");

        isRefilling = false;
        isManualRefilling = false;
        lastNoWaterErrorTime = millis();
        noWaterWarningActive = false;

        sendFirebaseNotification(
            "Refill Interrupted",
            "Water stopped flowing during the refill process.", "warning");

        if (deviceFirebaseId != "") {
          FirebaseData safetyData;
          String basePath = "hydrolink/devices/" + deviceFirebaseId;
          Firebase.RTDB.setString(
              &safetyData, basePath + "/status/refillStatus", "INTERRUPTED");
          Firebase.RTDB.setBool(
              &safetyData, basePath + "/control/manualRefill/command", false);
        }
        return;
      }
    }

    // Force-close solenoid after 5 minutes regardless of sensor state to prevent flooding if all detection fails
    if (flowCheckDuration > 300000) {
      Serial.println(" SAFETY: Solenoid open for 5 min — force closing!");

      closeSolenoid("Safety Timeout");
      isRefilling = false;
      isManualRefilling = false;
      lastNoWaterErrorTime = millis();
      noWaterWarningActive = false;

      sendFirebaseNotification(
          "Refill Safety Timeout",
          "Solenoid was open for 5 minutes. Force closed for safety.", "error");

      if (deviceFirebaseId != "") {
        FirebaseData safetyData;
        String basePath = "hydrolink/devices/" + deviceFirebaseId;
        Firebase.RTDB.setString(&safetyData, basePath + "/status/refillStatus",
                                "SAFETY_TIMEOUT");
        Firebase.RTDB.setBool(
            &safetyData, basePath + "/control/manualRefill/command", false);
      }
      return;
    }

    //  STOP CONDITION: Target Reached
    if (waterPercentage >= currentTarget) {
      Serial.printf(" Target Reached (%d%%). Total pulses: %lu, Liters: %.2f\n",
                    currentTarget, totalPulsesThisRefill, litersSoFar);

      if (isManualRefilling) {
        closeSolenoid("Target Reached");
        isManualRefilling = false;
        sendFirebaseNotification("Auto Refill Completed",
                                 "Manual refill target successfully reached.",
                                 "success");
      } else {
        closeSolenoid("Target Reached");
        sendFirebaseNotification("Auto Refill Completed",
                                 "Drum max fill level successfully reached.",
                                 "success");
      }

      isRefilling = false;

      if (deviceFirebaseId != "") {
        FirebaseData stopData;
        Firebase.RTDB.setString(&stopData,
                                "hydrolink/devices/" + deviceFirebaseId +
                                    "/status/refillStatus",
                                "COMPLETED");
        Firebase.RTDB.setBool(&stopData,
                              "hydrolink/devices/" + deviceFirebaseId +
                                  "/control/manualRefill/command",
                              false);
      }
    }
  } else {
    // Idle state - Ensure solenoid is closed
    digitalWrite(SOLENOID_PIN, LOW);
  }

  //AUTO-START (Only if NOT manual)
  //  DETERMINE IF WE SHOULD REFILL
  bool shouldRefill = false;

  if (waterPercentage < refillThresholdPercentage) {
    shouldRefill = true;
    // Mark that a refill has started so we can persist it if interrupted
    if (persistentRefillEnabled && !isRefillPending) {
      isRefillPending = true;
      saveSettingsToPreferences();
    }
  } else if (persistentRefillEnabled && isRefillPending &&
             waterPercentage < maxFillLevelPercentage) {
    // If water isn't below threshold, BUT a refill was pending and we haven't reached max level,   Resume
    shouldRefill = true;
  }

  //  CLEAR PENDING FLAG WHEN FULL
  if (waterPercentage >= maxFillLevelPercentage && isRefillPending) {
    isRefillPending = false;
    saveSettingsToPreferences();
  }

  //  TRIGGER THE SOLENOID
  // Wait configured seconds after a "No Water" error before retrying
  if (!isRefilling && !isManualRefilling && shouldRefill &&
      (millis() - lastNoWaterErrorTime > (refillRetrySeconds * 1000UL))) {
    openSolenoid();
    isRefilling = true;

    if (isRefillPending && waterPercentage >= refillThresholdPercentage) {
      Serial.println(
          "Persistent Auto-Refill Resumed (Recovering from interruption)");
    } else {
      Serial.println("Auto-Refill Started");
    }
  }
}

void loop() {
  // ALWAYS Handle OTA & WiFi First
  ArduinoOTA.handle();
  handleWiFiReconnection();

  unsigned long currentTime = millis();

  // Block if critical connection is missing
  static unsigned long stuckStartTime = 0;
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB08_tf);

    // Show status
    if (WiFi.status() != WL_CONNECTED) {
      u8g2.drawStr(10, 35, "WiFi Lost!");
    } else {
      u8g2.drawStr(10, 35, "Firebase Error!");
    }
    u8g2.sendBuffer();

    // Restart if stuck for 1 minute
    if (stuckStartTime == 0)
      stuckStartTime = millis();

    if (millis() - stuckStartTime > 60000) { // 60 seconds timeout
      ESP.restart(); // Reboot to bring back HydroLink_Setup AP
    }

    delay(500);
    return; // Stop here, don't run the rest of the loop
  } else {
    // If we are connected, reset the timer
    stuckStartTime = 0;
  }

  // 3. Link Status Check
  if (!isLinkedToUser) {
    displaySetupScreen();
    checkDeviceLinkStatus();
    delay(1000);
    return;
  }

  // 4. READ SENSORS (Timed)
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    // Distance
    float newReading = readUltrasonicCm();
    if (newReading > 0) {
      waterDistanceCm = newReading;
      calculateWaterPercentage();
    } else {
      Serial.printf(
          "SENSOR DEBUG: readUltrasonicCm returned %.1f (no valid reading), "
          "keeping waterDistanceCm=%.1f, drumHeight=%.1f, waterPct=%d%%\n",
          newReading, waterDistanceCm, drumHeightCm, waterPercentage);
    }

    // Battery
    batteryPercentage = readBatteryPercentage();

    lastSensorRead = currentTime;
  }

  // Calculate Flow Rate (call this every loop iteration, not just at sensor interval) This ensures flow rate is updated frequently for the no-water detection
  calculateFlowRate();

  // REFILL LOGIC (Run FAST, not inside the slow sensor timer)
  checkManualRefillCommand(); // Check for start/stop commands
  runRefillLogic();           // Check if we need to close valve NOW

  // Firebase Sync (Timed)
  if (currentTime - lastFirebaseUpdate >= FIREBASE_INTERVAL) {
    updateFirebaseData();
    lastFirebaseUpdate = currentTime;
  }

  //Settings Sync (Timed)
  if (currentTime - lastSettingsCheck >= SETTINGS_INTERVAL) {
    if (isLinkedToUser) {
      //  ADDED: Check if user clicked 'Scan Water Drum Now'
      checkManualDrumMeasurement();

      readFirebaseSettings();
    }
    lastSettingsCheck = currentTime;
  }

  //Update Display
  displayHomeScreen();

  // Small delay to keep OTA responsive
  delay(10);
}

//  OLED Display Functions

void displayHydroLinkAnimation() {
  const String appName = "HydroLink";

  // Animation Constants
  const int waterSurfaceY = SCREEN_HEIGHT - 10;
  const int dropEndX = SCREEN_WIDTH / 2;
  const int dropStartY = -15;
  const int dropEndY = waterSurfaceY;

  unsigned long startTime = millis();
  unsigned long totalDuration = 3000;
  unsigned long dropDuration = 1000

  while (millis() - startTime < totalDuration) {
    unsigned long elapsed = millis() - startTime;
    u8g2.clearBuffer();

    // Draw Water Surface (Bottom of screen)
    // A filled box representing the water pool
    u8g2.drawBox(0, waterSurfaceY, SCREEN_WIDTH, SCREEN_HEIGHT - waterSurfaceY);

    //Animation Logic
    if (elapsed < dropDuration) {
      //PHASE 1: The Drop Falling
      // Use quadratic easing for gravity effect (starts slow, speeds up)
      float progress = (float)elapsed / dropDuration;
      int currentY =
          dropStartY + (dropEndY - dropStartY) * (progress * progress);

      // Draw Droplet (Circle + Triangle for teardrop shape)
      int r = 6; // Radius of droplet
      u8g2.drawDisc(dropEndX, currentY, r);
      u8g2.drawTriangle(dropEndX - r, currentY, dropEndX + r, currentY,
                        dropEndX, currentY - r * 2.5);

    } else {
      //PHASE 2: Splash, Ripple & Text
      unsigned long rippleTime = elapsed - dropDuration;

      // Draw Ripples (Expanding Ellipses)
      if (rippleTime < 1000) {
        int rx = map(rippleTime, 0, 1000, 2, 50); // Expand width
        int ry = map(rippleTime, 0, 1000, 1, 8);  // Expand height (perspective)
        u8g2.drawEllipse(dropEndX, waterSurfaceY, rx, ry);

        // Second inner ripple
        if (rippleTime > 200) {
          u8g2.drawEllipse(dropEndX, waterSurfaceY, rx / 2, ry / 2);
        }
      }

      // Fade In / Typewriter Text "HydroLink"
      u8g2.setFont(u8g2_font_helvB18_tf);
      int textWidth = u8g2.getUTF8Width(appName.c_str());
      int textX = (SCREEN_WIDTH - textWidth) / 2;
      int textY = (SCREEN_HEIGHT / 2) - 5; // Centered above water

      // Show text after splash starts
      if (rippleTime > 300) {
        u8g2.drawStr(textX, textY, appName.c_str());
      }
    }

    u8g2.sendBuffer();
    delay(20); // Small delay to control frame rate
  }
}

void displaySetupScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tf);

  u8g2.drawStr(0, 12, "Device Setup");
  u8g2.drawStr(0, 28, "Enter MAC on website:");
  // Format MAC with colons for better readability on screen
  String formattedMac = "";
  for (int i = 0; i < deviceMacAddress.length(); i += 2) {
    formattedMac += deviceMacAddress.substring(i, i + 2);
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

  //  Water Level
  u8g2.drawStr(0, 28, "Water Level:");
  char buffer[10];
  sprintf(buffer, "%d%%", waterPercentage);
  u8g2.drawStr(u8g2.getUTF8Width("Water Level: ") + 2, 28, buffer);

  //  Status (UPDATED)
  u8g2.drawStr(0, 43, "Status: ");

  // 1. Check if Solenoid is OPEN (Refilling)
  if (digitalRead(SOLENOID_PIN) == HIGH) {
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 2, 43, "Refilling");
  }
  // 2. Check if Water is Above Threshold (Standby)
  else if (waterPercentage > refillThresholdPercentage) {
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 2, 43, "Standby");
  }
  // 3. Water Low + Solenoid Closed = No Source Water (Safety Stop)
  else {
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 2, 43, "No Water");
  }

  //  Threshold
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
  u8g2.drawBox(x + width, y + (height - notchHeight) / 2, notchWidth,
               notchHeight);
  int fillWidth = map(percentage, 0, 100, 0, width - 4);
  if (fillWidth < 0)
    fillWidth = 0;
  if (fillWidth > width - 4)
    fillWidth = width - 4;
  u8g2.drawBox(x + 2, y + 2, fillWidth, height - 4);

  u8g2.setFont(u8g2_font_7x13_mf);
  u8g2.setFontMode(1);
  char percBuffer[5];
  sprintf(percBuffer, "%d%%", percentage);
  u8g2.drawStr(x - u8g2.getUTF8Width(percBuffer) - 2,
               y + height / 2 + u8g2.getFontAscent() / 2 - 1, percBuffer);
}

void drawWiFiIcon() {
  int x_start = 5;
  int y_bottom = 15;
  int bar_width = 3;
  int bar_spacing = 2;
  if (wifiConnected) {
    u8g2.drawBox(x_start, y_bottom - 4, bar_width, 4);
    u8g2.drawBox(x_start + bar_width + bar_spacing, y_bottom - 7, bar_width, 7);
    u8g2.drawBox(x_start + 2 * (bar_width + bar_spacing), y_bottom - 10,
                 bar_width, 10);
    u8g2.drawBox(x_start + 3 * (bar_width + bar_spacing), y_bottom - 13,
                 bar_width, 13);
  }
}