#include <WiFi.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <time.h>
#include <Preferences.h>

// --- Configuration Constants ---
const char* AP_SSID = "HydroLink_Setup";
const char* AP_PASSWORD = "password123";
const int CONFIG_PORTAL_TIMEOUT_SECONDS = 180;

// Pin Definitions
//for OLED screen
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

//Ultrasonic Sensor/Water Level sensor
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18

//Battery
#define BATTERY_SENSE_PIN 34
#define RELAY_PIN 23

// Firebase configuration
#define FIREBASE_HOST_STR "hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA"
#define FIREBASE_DATABASE_URL "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app/"

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800; // GMT+8 for Philippines
const int daylightOffset_sec = 0;

// --- Global Variables ---
WiFiManager wm;
Preferences preferences;

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

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
const unsigned long SENSOR_INTERVAL = 5000;     // 5 seconds
const unsigned long FIREBASE_INTERVAL = 10000;  // 10 seconds
const unsigned long SETTINGS_INTERVAL = 30000;  // 30 seconds

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

// --- Helper Functions ---

void loadSettingsFromPreferences() {
    preferences.begin("hydrolink", true);
    
    // Load all persistent settings
    drumHeightCm = preferences.getFloat("drumHeight", 0.0);
    refillThresholdPercentage = preferences.getInt("refillThreshold", 25);
    maxFillLevelPercentage = preferences.getInt("maxFillLevel", 75);
    waterDistanceCm = preferences.getFloat("lastWaterDistance", 0.0);
    waterPercentage = preferences.getInt("lastWaterPercent", 0);
    waterAvailable = preferences.getBool("lastWaterAvailable", true);
    batteryPercentage = preferences.getInt("lastBattery", 100);
    
    // Check if drum height is initialized
    drumHeightInitialized = (drumHeightCm > 0);
    isDeviceFullyConfigured = drumHeightInitialized;
    
    preferences.end();
    
    Serial.println("=== Settings Loaded from Preferences ===");
    Serial.println("Drum height: " + String(drumHeightCm) + " cm");
    Serial.println("Refill threshold: " + String(refillThresholdPercentage) + "%");
    Serial.println("Max fill level: " + String(maxFillLevelPercentage) + "%");
    Serial.println("Device configured: " + String(isDeviceFullyConfigured ? "Yes" : "No"));
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
    if (Firebase.get(firebaseData, linkPath)) {
        if (firebaseData.jsonString() != "null" && firebaseData.jsonString().length() > 4) {
            Serial.println("Device is linked to user account");
            isLinkedToUser = true;
            return true;
        }
    }
    
    isLinkedToUser = false;
    return false;
}

void initializeDrumHeight() {
    // Settings already loaded from preferences in loadSettingsFromPreferences()
    if (drumHeightInitialized) {
        Serial.println("Drum height already initialized: " + String(drumHeightCm) + " cm");
        return;
    }
    
    Serial.println("Initializing drum height...");
    
    // Try Firebase if device is linked
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
    
    if (Firebase.getFloat(firebaseData, heightPath)) {
        if (firebaseData.dataType() == "float" || firebaseData.dataType() == "int") {
            float newDrumHeight = firebaseData.floatData();
            if (newDrumHeight > 0) {
                drumHeightCm = newDrumHeight;
                drumHeightInitialized = true;
                isDeviceFullyConfigured = true;
                saveSettingsToPreferences();
                
                Serial.print("Drum height fetched from Firebase: ");
                Serial.println(newDrumHeight);
                return true;
            } else {
                Serial.println("Invalid drum height from Firebase (<=0)");
            }
        } else {
            Serial.print("Invalid Firebase data type: ");
            Serial.println(firebaseData.dataType());
        }
    } else {
        Serial.print("Failed to fetch drum height: ");
        Serial.println(firebaseData.errorReason());
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

    // Calculate average
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

    // Update Firebase if device is linked
    if (isLinkedToUser && !deviceFirebaseId.isEmpty()) {
        String heightPath = "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
        if (Firebase.setFloat(firebaseData, heightPath, drumHeightCm)) {
            Serial.println("Drum height updated in Firebase");
        } else {
            Serial.print("Failed to update Firebase: ");
            Serial.println(firebaseData.errorReason());
        }
    }
}

float readUltrasonicCm() {
    // Clear trigger
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    // Send pulse
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    // Read echo with timeout
    unsigned long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000); // 30ms timeout

    if (duration == 0) {
        Serial.println("Ultrasonic sensor timeout");
        return -1.0;
    }

    // Calculate distance (speed of sound = 343 m/s = 0.0343 cm/µs)
    float distance = (duration * 0.0343) / 2.0;
    
    // Validate range (typical HC-SR04 range: 2-400cm)
    if (distance < 2.0 || distance > 400.0) {
        Serial.print("Distance out of range: ");
        Serial.println(distance);
        return -1.0;
    }

    return distance;
}

void calculateWaterPercentage() {
    if (drumHeightCm <= 0) {
        waterPercentage = 0;
        waterAvailable = false;
        Serial.println("Cannot calculate percentage: drum height not calibrated");
        return;
    }

    // Calculate water level (drum height - distance to water surface)
    float waterLevelCm = drumHeightCm - waterDistanceCm;
    
    // Clamp values
    waterLevelCm = constrain(waterLevelCm, 0, drumHeightCm);
    
    // Calculate percentage
    float effectiveHeight = drumHeightCm;
    if (maxFillLevelPercentage > 0 && maxFillLevelPercentage < 100) {
        effectiveHeight = drumHeightCm * (maxFillLevelPercentage / 100.0);
    }
    
    waterPercentage = (waterLevelCm / effectiveHeight) * 100;
    waterPercentage = constrain(waterPercentage, 0, 100);
    
    // Update water availability
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
    json.set("currentWaterLevelCm", round(waterDistanceCm * 10) / 10.0); // Round to 1 decimal
    json.set("waterPercentage", waterPercentage);
    json.set("waterAvailable", waterAvailable);
    json.set("batteryPercentage", batteryPercentage);
    json.set("lastUpdated", Firebase.getCurrentTime());
    json.set("deviceId", deviceFirebaseId);
    json.set("macAddress", deviceMacAddress);
    json.set("drumHeightCm", drumHeightCm);
    json.set("isConfigured", isDeviceFullyConfigured);
    json.set("isLinked", isLinkedToUser);

    if (Firebase.updateNode(firebaseData, basePath, json)) {
        Serial.println("Firebase data updated successfully");
        // Save current state to preferences periodically
        saveSettingsToPreferences();
    } else {
        Serial.print("Firebase update failed: ");
        Serial.println(firebaseData.errorReason());
    }
}

void readFirebaseSettings() {
    if (deviceFirebaseId.isEmpty() || !isLinkedToUser) {
        Serial.println("Cannot read settings: no Firebase ID or device not linked");
        return;
    }

    String settingsPath = "hydrolink/devices/" + deviceFirebaseId + "/settings";
    
    if (Firebase.get(firebaseData, settingsPath)) {
        if (firebaseData.dataType() == "json") {
            FirebaseJson &json = firebaseData.jsonObject();
            
            // Read settings with current values as defaults
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
            Serial.print("  Refill threshold: ");
            Serial.println(refillThresholdPercentage);
            Serial.print("  Max fill level: ");
            Serial.println(maxFillLevelPercentage);
            Serial.print("  Drum height: ");
            Serial.println(drumHeightCm);
            
            // Save updated settings to preferences
            saveSettingsToPreferences();
        }
    } else {
        // Only create default settings if this is a newly linked device
        if (isLinkedToUser) {
            Serial.println("Creating default settings in Firebase");
            
            FirebaseJson defaultSettings;
            defaultSettings.set("refillThresholdPercentage", refillThresholdPercentage);
            defaultSettings.set("maxFillLevelPercentage", maxFillLevelPercentage);
            defaultSettings.set("drumHeightCm", drumHeightCm);
            defaultSettings.set("measureDrum", false);
            
            if (Firebase.set(firebaseData, settingsPath, defaultSettings)) {
                Serial.println("Default settings created");
            } else {
                Serial.print("Failed to create settings: ");
                Serial.println(firebaseData.errorReason());
            }
        }
    }
}

void setSystemStatus(String status) {
    if (deviceFirebaseId.isEmpty()) return;
    
    String statusPath = "hydrolink/devices/" + deviceFirebaseId + "/status/systemStatus";
    if (Firebase.setString(firebaseData, statusPath, status)) {
        Serial.println("System status: " + status);
    }
}

void checkManualDrumMeasurement() {
    if (deviceFirebaseId.isEmpty() || !isLinkedToUser) return;
    
    String measurePath = "hydrolink/devices/" + deviceFirebaseId + "/settings/measureDrum";
    if (Firebase.getBool(firebaseData, measurePath)) {
        if (firebaseData.boolData()) {
            Serial.println("Manual drum measurement requested");
            measureDrumHeight();
            
            // Reset flag
            Firebase.setBool(firebaseData, measurePath, false);
        }
    }
}

int readBatteryPercentage() {
    // TODO: Implement actual battery reading
    // For now, return a simulated value
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
            
            // Re-establish Firebase connection
            Firebase.reconnectWiFi(true);
            setupFirebaseAuthentication();
        }
    }
}

void setupFirebaseAuthentication() {
    Serial.println("Setting up Firebase authentication...");
    
    firebaseConfig.host = FIREBASE_HOST_STR;
    firebaseConfig.database_url = FIREBASE_DATABASE_URL;
    firebaseConfig.api_key = FIREBASE_API_KEY;

    // Try to get stored Firebase ID
    String storedUid = getStoredFirebaseId();
    
    if (storedUid.length() > 0) {
        Serial.println("Found stored Firebase UID: " + storedUid);
        deviceFirebaseId = storedUid;
        
        // Check if this device is already linked to a user
        Firebase.begin(&firebaseConfig, &firebaseAuth);
        Firebase.reconnectWiFi(true);
        
        // Wait for Firebase to be ready
        unsigned long startTime = millis();
        while (!Firebase.ready() && (millis() - startTime < 15000)) {
            Serial.print(".");
            delay(500);
        }
        Serial.println();
        
        if (Firebase.ready()) {
            // Check link status
            checkDeviceLinkStatus();
            
            if (isLinkedToUser) {
                Serial.println("Device is already linked - using stored UID");
                return;
            } else {
                Serial.println("Device not linked yet - continuing with stored UID");
                return;
            }
        }
    }

    // Only create new anonymous user if no stored UID exists
    Serial.println("No stored UID found - creating new anonymous user...");
    
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);

    // Wait for Firebase to be ready
    unsigned long startTime = millis();
    while (!Firebase.ready() && (millis() - startTime < 15000)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (Firebase.ready() && firebaseAuth.token.uid.length() > 0) {
        deviceFirebaseId = firebaseAuth.token.uid.c_str();
        Serial.println("Firebase authenticated. UID: " + deviceFirebaseId);
        storeFirebaseId(deviceFirebaseId);
    } else {
        Serial.println("Attempting anonymous signup...");
        if (Firebase.signUp(&firebaseConfig, &firebaseAuth, "", "")) {
            deviceFirebaseId = firebaseAuth.token.uid.c_str();
            Serial.println("Anonymous user created. UID: " + deviceFirebaseId);
            storeFirebaseId(deviceFirebaseId);
        } else {
            Serial.print("Firebase auth failed: ");
            Serial.println(firebaseData.errorReason());
            deviceFirebaseId = "";
        }
    }
}

void mapMacToFirebaseUid() {
    if (deviceFirebaseId.isEmpty() || deviceMacAddress.isEmpty()) return;
    
    String path = "hydrolink/macToFirebaseUid/" + deviceMacAddress;
    if (Firebase.setString(firebaseData, path, deviceFirebaseId)) {
        Serial.println("MAC to UID mapping updated");
    } else {
        Serial.print("MAC mapping failed: ");
        Serial.println(firebaseData.errorReason());
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

// --- Setup Function ---
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== HydroLink ESP32 Starting ===");

    // Initialize pins
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Load all settings from preferences FIRST
    loadSettingsFromPreferences();

    // Setup WiFi
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

        // Get MAC address
        deviceMacAddress = getMacAddress();
        Serial.println("Device MAC: " + deviceMacAddress);

        // Setup Firebase authentication (will use stored UID if available)
        setupFirebaseAuthentication();
        
        if (!deviceFirebaseId.isEmpty()) {
            // Configure NTP
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            
            // Wait for time sync
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                Serial.println("NTP time synchronized");
            }

            // Set up Firebase buffer sizes
            firebaseData.setBSSLBufferSize(4096, 1024);
            firebaseData.setResponseSize(2048);

            // Initialize system
            setSystemStatus("online");
            mapMacToFirebaseUid();
            
            // Check device link status
            checkDeviceLinkStatus();
            
            // Initialize drum height (will use stored value if available)
            initializeDrumHeight();
            
            // Read Firebase settings only if device is linked
            if (isLinkedToUser) {
                readFirebaseSettings();
            }

            Serial.println("Setup complete!");
            Serial.println("Device ID: " + deviceFirebaseId);
            Serial.println("Device Linked: " + String(isLinkedToUser ? "Yes" : "No"));
            Serial.println("Device Configured: " + String(isDeviceFullyConfigured ? "Yes" : "No"));
        } else {
            Serial.println("Firebase authentication failed!");
        }
    } else {
        Serial.println("WiFi connection failed!");
    }

    // Wait for device linking only if not already linked
    if (!deviceFirebaseId.isEmpty() && Firebase.ready() && !isLinkedToUser) {
        Serial.println("Waiting for device linking...");
        unsigned long linkStartTime = millis();
        
        while (millis() - linkStartTime < 300000) { // Wait up to 5 minutes
            if (checkDeviceLinkStatus()) {
                Serial.println("Device linked to user account!");
                readFirebaseSettings(); // Load settings from Firebase after linking
                break;
            }
            Serial.print(".");
            delay(5000);
        }
        
        if (!isLinkedToUser) {
            Serial.println("Device linking timeout - continuing without user link");
        }
    }

    Serial.println("System ready!");
}

// --- Main Loop ---
void loop() {
    unsigned long currentTime = millis();
    
    // Handle WiFi reconnection
    handleWiFiReconnection();
    
    // Only proceed if we have WiFi and Firebase is ready
    if (!wifiConnected || !ensureFirebaseConnection() || deviceFirebaseId.isEmpty()) {
        delay(5000);
        return;
    }

    // Read sensors at intervals
    if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
        float newReading = readUltrasonicCm();
        if (newReading > 0) {
            waterDistanceCm = newReading;
            calculateWaterPercentage();
        }
        
        batteryPercentage = readBatteryPercentage();
        lastSensorRead = currentTime;
    }

    // Update Firebase at intervals (only if linked or for basic status)
    if (currentTime - lastFirebaseUpdate >= FIREBASE_INTERVAL) {
        updateFirebaseData();
        lastFirebaseUpdate = currentTime;
    }

    // Check settings at intervals (only if linked)
    if (currentTime - lastSettingsCheck >= SETTINGS_INTERVAL) {
        // Periodically check link status
        checkDeviceLinkStatus();
        
        if (isLinkedToUser) {
            readFirebaseSettings();
            checkManualDrumMeasurement();
        }
        lastSettingsCheck = currentTime;
    }

    // Small delay to prevent watchdog issues
    delay(100);
}