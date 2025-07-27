// esp32base.ino - Fixed Version
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
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18
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

// --- Helper Functions ---

void initializeDrumHeight() {
    Serial.println("Initializing drum height...");
    
    // Try Firebase first
    if (Firebase.ready() && fetchDrumHeightFromFirebase()) {
        Serial.println("Drum height set from Firebase.");
        drumHeightInitialized = true;
        return;
    }

    // Try local preferences
    preferences.begin("hydrolink", true);
    float storedDrumHeight = preferences.getFloat("drumHeight", 0.0);
    preferences.end();
    
    if (storedDrumHeight > 0) {
        drumHeightCm = storedDrumHeight;
        drumHeightInitialized = true;
        Serial.print("Drum height set from preferences: ");
        Serial.println(drumHeightCm);
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
                // Save to preferences
                preferences.begin("hydrolink", false);
                preferences.putFloat("drumHeight", newDrumHeight);
                preferences.end();
                
                drumHeightCm = newDrumHeight;
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

    // Save to preferences
    preferences.begin("hydrolink", false);
    preferences.putFloat("drumHeight", drumHeightCm);
    preferences.end();
    
    Serial.print("Drum height measured and saved: ");
    Serial.println(drumHeightCm);

    // Update Firebase
    if (!deviceFirebaseId.isEmpty()) {
        String heightPath = "hydrolink/devices/" + deviceFirebaseId + "/settings/drumHeightCm";
        if (Firebase.setFloat(firebaseData, heightPath, drumHeightCm)) {
            Serial.println("Drum height updated in Firebase");
        } else {
            Serial.print("Failed to update Firebase: ");
            Serial.println(firebaseData.errorReason());
        }
    }
    
    drumHeightInitialized = true;
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

    if (Firebase.updateNode(firebaseData, basePath, json)) {
        Serial.println("Firebase data updated successfully");
    } else {
        Serial.print("Firebase update failed: ");
        Serial.println(firebaseData.errorReason());
    }
}

void readFirebaseSettings() {
    if (deviceFirebaseId.isEmpty()) {
        Serial.println("Cannot read settings: no Firebase ID");
        return;
    }

    String settingsPath = "hydrolink/devices/" + deviceFirebaseId + "/settings";
    
    if (Firebase.get(firebaseData, settingsPath)) {
        if (firebaseData.dataType() == "json") {
            FirebaseJson &json = firebaseData.jsonObject();
            
            // Read settings with defaults
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
                }
            }
            
            Serial.println("Settings loaded from Firebase:");
            Serial.print("  Refill threshold: ");
            Serial.println(refillThresholdPercentage);
            Serial.print("  Max fill level: ");
            Serial.println(maxFillLevelPercentage);
            Serial.print("  Drum height: ");
            Serial.println(drumHeightCm);
            
            // Mark as configured if drum height is set
            isDeviceFullyConfigured = drumHeightInitialized;
        }
    } else {
        // Create default settings if they don't exist
        Serial.println("Creating default settings in Firebase");
        
        FirebaseJson defaultSettings;
        defaultSettings.set("refillThresholdPercentage", refillThresholdPercentage);
        defaultSettings.set("maxFillLevelPercentage", maxFillLevelPercentage);
        defaultSettings.set("drumHeightCm", 0);
        defaultSettings.set("measureDrum", false);
        
        if (Firebase.set(firebaseData, settingsPath, defaultSettings)) {
            Serial.println("Default settings created");
        } else {
            Serial.print("Failed to create settings: ");
            Serial.println(firebaseData.errorReason());
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
    if (deviceFirebaseId.isEmpty()) return;
    
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

    // Try to load saved UID
    preferences.begin("hydrolink", true);
    String savedUid = preferences.getString("firebaseUid", "");
    preferences.end();

    if (savedUid.length() > 0) {
        Serial.print("Found saved UID: ");
        Serial.println(savedUid);
    }

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
        
        // Save UID to preferences
        preferences.begin("hydrolink", false);
        preferences.putString("firebaseUid", deviceFirebaseId);
        preferences.end();
    } else {
        Serial.println("Attempting anonymous signup...");
        if (Firebase.signUp(&firebaseConfig, &firebaseAuth, "", "")) {
            deviceFirebaseId = firebaseAuth.token.uid.c_str();
            Serial.println("Anonymous user created. UID: " + deviceFirebaseId);
            
            preferences.begin("hydrolink", false);
            preferences.putString("firebaseUid", deviceFirebaseId);
            preferences.end();
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

    // Setup WiFi
    WiFi.mode(WIFI_STA);
    wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SECONDS);

    Serial.println("Connecting to WiFi...");
    wifiConnected = wm.autoConnect(AP_SSID, AP_PASSWORD);

        // Get MAC address
    deviceMacAddress = getMacAddress();
    Serial.println("Device MAC: " + deviceMacAddress);

    if (wifiConnected) {
        Serial.println("WiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        // Setup Firebase
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
            initializeDrumHeight();
            readFirebaseSettings();

            Serial.println("Setup complete!");
        } else {
            Serial.println("Firebase authentication failed!");
        }
    } else {
        Serial.println("WiFi connection failed!");
    }

    // Wait for device linking
    Serial.println("Waiting for device linking...");
    while (!deviceFirebaseId.isEmpty() && Firebase.ready()) {
        String linkPath = "hydrolink/devices/" + deviceFirebaseId + "/linkedUsers";
        if (Firebase.get(firebaseData, linkPath)) {
            if (firebaseData.jsonString() != "null") {
                Serial.println("Device linked to user account!");
                break;
            }
        }
        Serial.print(".");
        delay(5000);
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

    // Update Firebase at intervals
    if (currentTime - lastFirebaseUpdate >= FIREBASE_INTERVAL) {
        if (isDeviceFullyConfigured) {
            updateFirebaseData();
        }
        lastFirebaseUpdate = currentTime;
    }

    // Check settings at intervals
    if (currentTime - lastSettingsCheck >= SETTINGS_INTERVAL) {
        readFirebaseSettings();
        checkManualDrumMeasurement();
        lastSettingsCheck = currentTime;
    }

    // Small delay to prevent watchdog issues
    delay(100);
}