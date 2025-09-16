#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h> 
#include <time.h>
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>

// --- Configuration Constants ---
const char* AP_SSID = "HydroLink_Setup";
const char* AP_PASSWORD = "password123";
const int CONFIG_PORTAL_TIMEOUT_SECONDS = 180;

// Pin Definitions for ESP32
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 4   
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18
#define BATTERY_SENSE_PIN 1
// #define RELAY_PIN 13

// Firebase configuration
#define FIREBASE_HOST_STR "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_API_KEY "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA"
#define FIREBASE_DATABASE_URL "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app/"

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800; // GMT+8 for Philippines
const int daylightOffset_sec = 0;

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64

// U8g2 Display Object
// This is for a 128x64 SH1106, using full buffer, hardware I2C.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Water Droplet Icon Bitmap Data (30x54 pixels)
// This bitmap is generated for a 30x54 pixel water droplet.
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

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// System variables
float waterDistanceCm = 0.0;
int waterPercentage = 0;
bool waterAvailable = true;
int refillThresholdPercentage = 25; // Default value
int maxFillLevelPercentage = 75;    // Default value
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
void setupFirebaseAuthentication();
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
// --- OLED Display Function Prototypes ---
void displayHydroLinkAnimation();
void displayHomeScreen();
void drawBatteryIcon(int percentage);
void drawWiFiIcon();
void tokenStatusCallback(TokenInfo info);

// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== HydroLink ESP32 Starting ===");

  // --- OLED Initialization ---
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.print("Initializing OLED on SDA: "); Serial.print(I2C_SDA_PIN);
  Serial.print(", SCL: "); Serial.println(I2C_SCL_PIN);
  u8g2.begin(); // This initializes the display hardware
  Serial.println("U8g2 OLED initialized.");

  // --- Display Startup Animation ---
  displayHydroLinkAnimation();
  delay(1500); // Hold animation for a bit

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  // pinMode(RELAY_PIN, OUTPUT);
  // digitalWrite(RELAY_PIN, LOW);

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

    deviceMacAddress = getMacAddress();
    Serial.println("Device MAC: " + deviceMacAddress);

    setupFirebaseAuthentication();

    if (!deviceFirebaseId.isEmpty()) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        Serial.println("NTP time synchronized");
      }

      fbdo.setBSSLBufferSize(4096, 1024);
      fbdo.setResponseSize(2048);

      setSystemStatus("online");
      mapMacToFirebaseUid();
      
      checkDeviceLinkStatus();
      initializeDrumHeight();

      if (isLinkedToUser) {
        readFirebaseSettings();
      }

      Serial.println("Setup complete!");
      Serial.println("Device ID: " + deviceFirebaseId);
      Serial.println("Device Linked: " + String(isLinkedToUser ? "Yes" : "No"));
      Serial.println("Device Configured: " + String(isDeviceFullyConfigured ? "Yes" : "No"));
    }
  }

  if (!deviceFirebaseId.isEmpty() && Firebase.ready() && !isLinkedToUser) {
    Serial.println("Waiting for device linking...");
    unsigned long linkStartTime = millis();
    
    while (millis() - linkStartTime < 300000) {
      if (checkDeviceLinkStatus()) {
        Serial.println("Device linked to user account!");
        readFirebaseSettings();
        break;
      }
      Serial.print(".");
      delay(5000);
    }
  }

  Serial.println("System ready!");
}

void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_ready) {
    Serial.println("[Firebase] Token ready");
    // Save UID from token
    if (auth.token.uid.length() > 0) {
      preferences.begin("firebase", false);
      preferences.putString("uid", auth.token.uid.c_str());  // ✅ convert MB_String -> const char*
      preferences.end();
      Serial.println(String("[Firebase] UID saved: ") + auth.token.uid.c_str());
    }
  }
}

void loadSettingsFromPreferences() {
  Serial.println("Loading settings from preferences...");
  
  debugPreferences();
  if (!preferences.begin("hydrolink", true)) {
    Serial.println("Failed to open preferences namespace!");
    handleFactoryReset();
    return;
  }
  
  // Load all persistent settings with validation
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
  
  // Load linked status
  isLinkedToUser = preferences.getBool("isLinked", false);
  
  // Check if drum height is initialized
  drumHeightInitialized = (drumHeightCm > 0);
  isDeviceFullyConfigured = drumHeightInitialized;
  
  preferences.end();
  Serial.println("=== Settings Loaded from Preferences ===");
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
  if (deviceFirebaseId.isEmpty() || !Firebase.ready()) {
    return false;
  }
  
  String linkPath = "hydrolink/devices/" + deviceFirebaseId + "/linkedUsers";
  
  // Request JSON data from RTDB
  if (Firebase.RTDB.getJSON(&fbdo, linkPath.c_str())) {
    String jsonStr = fbdo.jsonString();
    if (jsonStr != "null" && jsonStr.length() > 4) {
      if (!isLinkedToUser) {
        isLinkedToUser = true;
        saveSettingsToPreferences(); // Save the new linked status
        Serial.println("Device is now linked to user account");
      }
      return true;
    }
  } else {
    Serial.print("Firebase get failed: ");
    Serial.println(fbdo.errorReason());
  }

  if (isLinkedToUser) {
    isLinkedToUser = false;
    saveSettingsToPreferences(); // Save the new unlinked status
    Serial.println("Device is no longer linked to user account");
  }
  return false;
}

void handleFactoryReset() {
  Serial.println("=== FACTORY RESET DETECTED ===");
  
  preferences.begin("hydrolink", false);
  preferences.clear();
  preferences.end();
  
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
  if (deviceFirebaseId.isEmpty()) {
    Serial.println("Cannot fetch from Firebase: Device Firebase ID not set.");
    return false;
  }
  
  String heightPath = "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
  Serial.print("Reading drum height from: ");
  Serial.println(heightPath);

  if (Firebase.RTDB.getFloat(&fbdo, heightPath)) {
    if (fbdo.dataType() == "float" || fbdo.dataType() == "int") {
      float newDrumHeight = fbdo.floatData();
      if (newDrumHeight > 0) {
        drumHeightCm = newDrumHeight;
        drumHeightInitialized = true;
        isDeviceFullyConfigured = true;
        saveSettingsToPreferences();
        
        Serial.print("Drum height fetched from Firebase: ");
        Serial.println(newDrumHeight);
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
      Serial.print("Measurement ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(reading);
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
  
  Serial.print("Drum height measured and saved: ");
  Serial.println(drumHeightCm);

  if (isLinkedToUser && !deviceFirebaseId.isEmpty()) {
    String heightPath = "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
    if (Firebase.RTDB.setFloat(&fbdo, heightPath, drumHeightCm)) {
      Serial.println("Drum height updated in Firebase");
    } else {
      Serial.print("Failed to update Firebase: ");
      Serial.println(fbdo.errorReason());
    }
  }
}

float readUltrasonicCm() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(25);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // Use pulseInLong with longer timeout (60ms)
  unsigned long duration = pulseInLong(ULTRASONIC_ECHO_PIN, HIGH, 60000);
  if (duration == 0) {
    Serial.println("Ultrasonic sensor timeout");
    return -1.0;
  }

  float distance = duration * 0.034 / 2.0;
  return distance;
}

void calculateWaterPercentage() {
  if (drumHeightCm <= 0) {
    waterPercentage = 0;
    waterAvailable = false;
    Serial.println("Cannot calculate percentage: drum height not calibrated");
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
  
  Serial.print("Water: ");
  Serial.print(waterPercentage);
  Serial.print("% (");
  Serial.print(waterLevelCm, 1);
  Serial.print("cm / ");
  Serial.print(drumHeightCm, 1);
  Serial.println("cm)");
}

void updateFirebaseData() {
  if (deviceFirebaseId.isEmpty() || !Firebase.ready()) {
    Serial.println("Cannot update Firebase: not ready or no ID");
    return;
  }

  String basePath = "hydrolink/devices/" + deviceFirebaseId + "/status";
  
  FirebaseJson json;
  json.set("currentWaterLevelCm", round(waterDistanceCm * 10) / 10.0);
  json.set("waterPercentage", waterPercentage);
  json.set("waterAvailable", waterAvailable);
  json.set("batteryPercentage", batteryPercentage);
  json.set("lastUpdated", Firebase.getCurrentTime());
  json.set("deviceId", deviceFirebaseId);
  json.set("macAddress", deviceMacAddress);
  json.set("drumHeightCm", drumHeightCm);
  json.set("isConfigured", isDeviceFullyConfigured);
  json.set("isLinked", isLinkedToUser);

  if (Firebase.RTDB.updateNode(&fbdo, basePath, &json)) {
    Serial.println("Firebase data updated successfully");
    saveSettingsToPreferences();
  } else {
    Serial.print("Firebase update failed: ");
    Serial.println(fbdo.errorReason());
  }
}

void readFirebaseSettings() {
  if (deviceFirebaseId.isEmpty() || !isLinkedToUser) {
    return;
  }

  String settingsPath = "hydrolink/devices/" + deviceFirebaseId + "/settings";
  if (Firebase.RTDB.get(&fbdo, settingsPath)) {
    if (fbdo.dataType() == "json") {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData jsonData;
      
      if (json.get(jsonData, "refillThresholdPercentage")) {
        refillThresholdPercentage = jsonData.intValue;
      }
      
      if (json.get(jsonData, "maxFillLevelPercentage")) {
        maxFillLevelPercentage = jsonData.intValue;
      }
      
      if (json.get(jsonData, "drumHeightCm")) {
        float fbDrumHeight = jsonData.floatValue;
        if (fbDrumHeight > 0) {
          drumHeightCm = fbDrumHeight;
          drumHeightInitialized = true;
          isDeviceFullyConfigured = true;
        }
      }
      
      Serial.println("Settings updated from Firebase:");
      Serial.print(" Refill threshold: ");
      Serial.println(refillThresholdPercentage);
      Serial.print(" Max fill level: ");
      Serial.println(maxFillLevelPercentage);
      Serial.print(" Drum height: ");
      Serial.println(drumHeightCm);
      
      saveSettingsToPreferences();
    }
  }
}

void setSystemStatus(String status) {
  if (deviceFirebaseId.isEmpty()) return;
  String statusPath = "hydrolink/devices/" + deviceFirebaseId + "/status/systemStatus";
  if (Firebase.RTDB.setString(&fbdo, statusPath, status)) {
    Serial.println("System status: " + status);
  }
}

void checkManualDrumMeasurement() {
  if (deviceFirebaseId.isEmpty() || !isLinkedToUser) return;
  
  String measurePath = "hydrolink/devices/" + deviceFirebaseId + "/settings/measureDrum";
  if (Firebase.RTDB.getBool(&fbdo, measurePath)) {
    if (fbdo.boolData()) {
      Serial.println("Manual drum measurement requested");
      measureDrumHeight();
      Firebase.RTDB.setBool(&fbdo, measurePath, false);
    }
  }
}

int readBatteryPercentage() {
  static int simulatedBattery = 100;
  simulatedBattery = random(80, 100);
  return simulatedBattery;
}

bool ensureFirebaseConnection() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready, attempting reconnection...");
    setupFirebaseAuthentication();
    return Firebase.ready();
  }
  return true;
}

void handleWiFiReconnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    wifiConnected = wm.autoConnect(AP_SSID, AP_PASSWORD);
    
    if (wifiConnected) {
      Serial.println("WiFi reconnected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Firebase.reconnectWiFi(true);
      setupFirebaseAuthentication();
    }
  }
}

void setupFirebaseAuthentication() {
  Serial.println("Setting up Firebase authentication...");

  config.host = FIREBASE_HOST_STR;
  config.database_url = FIREBASE_DATABASE_URL;
  config.api_key = FIREBASE_API_KEY;
  config.token_status_callback = tokenStatusCallback;

  // Try to load saved refresh token
  preferences.begin("firebase", true);
  String savedRefreshToken = preferences.getString("refreshToken", "");
  preferences.end();

  if (savedRefreshToken.length() > 0) {
    Serial.println("[Firebase] Using saved refresh token...");
    auth.token.refreshToken = savedRefreshToken.c_str();

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    unsigned long startTime = millis();
    while (!Firebase.ready() && (millis() - startTime < 15000)) {
      Serial.print(".");
      delay(500);
    }

    if (Firebase.ready()) {
      Serial.println("\n[Firebase] Successfully authenticated with saved token");
      deviceFirebaseId = auth.token.uid.c_str();
      Serial.println("Device UID: " + deviceFirebaseId);

      if (checkDeviceLinkStatus()) {
        Serial.println("Device is linked. Continuing with existing account.");
        mapMacToFirebaseUid();
        return; // ✅ Done, don’t create new user
      } else {
        Serial.println("Device not linked anymore. Will create a new anonymous user.");
      }
    } else {
      Serial.println("\n[Firebase] Saved token failed (stale or invalid). Will create new anonymous user.");
    }
  }

  // If no saved token or token failed, create a new anonymous user
  Serial.println("Creating a new anonymous user...");
  resetFirebaseAuth();

  if (Firebase.signUp(&config, &auth, "", "")) {
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    unsigned long startTime = millis();
    while (!Firebase.ready() && (millis() - startTime < 10000)) {
      Serial.print(".");
      delay(500);
    }

    if (Firebase.ready()) {
      deviceFirebaseId = auth.token.uid.c_str();
      Serial.println("\n[Firebase] New device UID: " + deviceFirebaseId);
      mapMacToFirebaseUid();
      isLinkedToUser = false; // New user not linked yet
    } else {
      Serial.print("[Firebase] Auth failed: ");
      Serial.println(fbdo.errorReason());
    }
  } else {
    Serial.print("[Firebase] signUp failed: ");
    Serial.println(fbdo.errorReason());
  }
}


void resetFirebaseAuth() {
  // For FirebaseESP32, the most reliable way is to create a new auth instance
  auth = FirebaseAuth(); // Create fresh auth object
  deviceFirebaseId = ""; // Clear stored device ID
  isLinkedToUser = false; // Reset link status
  
  // Clear stored UID from preferences
  preferences.begin("hydrolink", false);
  preferences.remove("firebaseUid");
  preferences.end();
  Serial.println("Firebase auth state reset");
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

void mapMacToFirebaseUid() {
  if (deviceFirebaseId.isEmpty() || deviceMacAddress.isEmpty()) return;
  
  String path = "hydrolink/macToFirebaseUid/" + deviceMacAddress;
  if (Firebase.RTDB.setString(&fbdo, path, deviceFirebaseId)) {
    Serial.println("MAC to UID mapping updated");
  } else {
    Serial.print("MAC mapping failed: ");
    Serial.println(fbdo.errorReason());
  }
}

// --- WiFiManager Callbacks ---
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered WiFi config mode");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void saveConfigCallback() {
  Serial.println("WiFi configuration saved!");
}

String getMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}


// --- Main Loop ---
void loop() {
  unsigned long currentTime = millis();
  
  handleWiFiReconnection();

  if (!wifiConnected || !ensureFirebaseConnection() || deviceFirebaseId.isEmpty()) {
    // Display a simple 'Connecting...' screen on the OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB08_tf);
    u8g2.drawStr(0, 10, "Connecting...");
    u8g2.sendBuffer();
    delay(5000);
    return;
  }

  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    float newReading = readUltrasonicCm();
    if (newReading > 0) {
      waterDistanceCm = newReading;
      calculateWaterPercentage();
    }
    
    batteryPercentage = readBatteryPercentage();
    lastSensorRead = currentTime;
  }

  if (currentTime - lastFirebaseUpdate >= FIREBASE_INTERVAL) {
    updateFirebaseData();
    lastFirebaseUpdate = currentTime;
  }

  if (currentTime - lastSettingsCheck >= SETTINGS_INTERVAL) {
    checkDeviceLinkStatus();
    if (isLinkedToUser) {
      readFirebaseSettings();
      checkManualDrumMeasurement();
    }
    lastSettingsCheck = currentTime;
  }

  // Update the OLED display with the latest data
  displayHomeScreen();
  
  delay(100);
}

// --- OLED Display Functions ---

void displayHydroLinkAnimation() {
  const String appName = "HydroLink";
  int icon_width = 30;
  int icon_height = 54;
  
  // --- Phase 1: Display Water Droplet Centered ---
  u8g2.clearBuffer(); // Clear the internal buffer
  int droplet_x = (SCREEN_WIDTH - icon_width) / 2;
  int droplet_y = (SCREEN_HEIGHT - icon_height) / 2;
  u8g2.drawXBMP(droplet_x, droplet_y, icon_width, icon_height, water_droplet_icon_30x54);
  u8g2.sendBuffer(); // Send the buffer to the display
  delay(1000); // Show droplet for 1 second

  // --- Phase 2: Animate HydroLink Text (Centered) ---
  u8g2.clearBuffer();
  // U8g2 uses specific font names. u8g2_font_helvB18_tf is a good bold font for size 2.
  // The 'tf' at the end usually means 'transparent' background and 'full' character set.
  u8g2.setFont(u8g2_font_helvB18_tf);
  u8g2.setFontMode(1); // Set font mode to transparent (overwrites background only with drawn pixels)
  
  // Calculate starting X for centered text. Get approximate text width.
  // U8g2's getTextWidth is accurate for the chosen font.
  int text_width = u8g2.getUTF8Width(appName.c_str());
  int text_centered_x = (SCREEN_WIDTH - text_width) / 2;
  // For U8g2's drawStr, Y is the baseline, so calculate from font height.
  int text_height = u8g2.getFontAscent() - u8g2.getFontDescent();
  int text_centered_y = (SCREEN_HEIGHT - text_height) / 2 + u8g2.getFontAscent();
  
  for (int i = 0; i <= appName.length(); i++) {
    u8g2.clearBuffer(); // Clear for each new character display
    u8g2.drawStr(text_centered_x, text_centered_y, appName.substring(0, i).c_str());
    u8g2.sendBuffer();
    delay(200); // Delay between letters
  }
}

void displayHomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tf); // Smaller font for home screen

  // --- Top Bar: Battery and Wi-Fi ---
  drawBatteryIcon(batteryPercentage); // Battery on the right
  drawWiFiIcon(); // Wi-Fi icon on the left

  // --- Water Level ---
  u8g2.drawStr(0, 28, "Water Level:");
  char buffer[10];
  sprintf(buffer, "%.0f%%", (float)waterPercentage); // Use global waterPercentage variable
  u8g2.drawStr(u8g2.getUTF8Width("Water Level: ") + 0, 28, buffer);
  
  // --- Water Availability Status (Simplified) ---
  u8g2.drawStr(0, 43, "Status: ");
  if (waterAvailable) { // Use global waterAvailable variable
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 0, 43, "Available");
  } else {
    u8g2.drawStr(u8g2.getUTF8Width("Status: ") + 0, 43, "Unavailable");
  }

  // --- Fill Threshold ---
  u8g2.drawStr(0, 58, "Threshold: ");
  sprintf(buffer, "%d%%", refillThresholdPercentage); // Use global refillThresholdPercentage
  u8g2.drawStr(u8g2.getUTF8Width("Threshold: ") + 0, 58, buffer);


  u8g2.sendBuffer(); // Send all drawn elements to the display
}

void drawBatteryIcon(int percentage) {
  // Battery icon dimensions
  int x = 95; // Right side of screen, starting X position
  int y = 0; // Top of the screen, Y position
  int width = 30;
  int height = 15;
  int notchWidth = 4;
  int notchHeight = 5;

  // Draw main body of battery
  u8g2.drawFrame(x, y, width, height); // drawRect is drawFrame in U8g2
  // Draw battery notch
  u8g2.drawBox(x + width, y + (height / 2) - (notchHeight / 2), notchWidth, notchHeight); // fillRect is drawBox in U8g2

  // Calculate fill level
  int fillWidth = map(percentage, 0, 100, 0, width - 4); // Leave a small border
  if (fillWidth < 0) fillWidth = 0; // Ensure no negative width
  if (fillWidth > width - 4) fillWidth = width - 4; // Ensure not overflowing

  // Draw battery fill
  u8g2.drawBox(x + 2, y + 2, fillWidth, height - 4); // Offset by 2 for border

  // Add percentage text inside or near the icon
  u8g2.setFont(u8g2_font_7x13_mf); // A small, clear font for percentage
  u8g2.setFontMode(1); // Transparent
  char percBuffer[5];
  sprintf(percBuffer, "%d%%", percentage);
  // Position to the left of battery icon, roughly centered vertically
  u8g2.drawStr(x - u8g2.getUTF8Width(percBuffer) - 2, y + (height / 2) + u8g2.getFontAscent() / 2 - 1, percBuffer);
}

// Draw WiFiIcon as 4 progressively larger bars
void drawWiFiIcon() {
  int x_start = 5; // Starting X position for the Wi-Fi icon (left side)
  int y_bottom = 15; // Y position for the bottom of the bars (top of the screen area)
  int bar_width = 3; // Width of each bar
  int bar_spacing = 2; // Space between bars

  // Check if WiFi is connected to decide whether to draw the icon
  if (wifiConnected) {
    // Bar 1 (smallest)
    u8g2.drawBox(x_start, y_bottom - 4, bar_width, 4);
    // Bar 2
    u8g2.drawBox(x_start + bar_width + bar_spacing, y_bottom - 7, bar_width, 7);
    // Bar 3
    u8g2.drawBox(x_start + (bar_width + bar_spacing) * 2, y_bottom - 10, bar_width, 10);
    // Bar 4 (largest)
    u8g2.drawBox(x_start + (bar_width + bar_spacing) * 3, y_bottom - 13, bar_width, 13);
  }
}