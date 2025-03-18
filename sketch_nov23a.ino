#define BLYNK_TEMPLATE_ID "TMPL3U24IZ3pM"
#define BLYNK_TEMPLATE_NAME "Air Quailty Monitoring"
#define BLYNK_AUTH_TOKEN "hjEaS5i_NV19ZRnc11pFDyEmLRVKtndc"
#define BLYNK_PRINT Serial

#define WIFI_RETRY_DELAY 1000
#define MAX_CONNECTION_ATTEMPTS 3
#define WIFI_TIMEOUT 10000

// Library includes
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <EEPROM.h>
#include <mbedtls/md.h>  // For HMAC
#include <HTTPClient.h>  // For HTTP request

// Pin Definitions
#define DHTPIN 13        // DHT22 data pin
#define MQ135_PIN 32      // Gas sensor pin
#define I2C_SDA 21        // I2C SDA pin for LCD
#define I2C_SCL 22        // I2C SCL pin for LCD

// Constants
#define EEPROM_SIZE 512
#define LCD_COLS 16
#define LCD_ROWS 2
#define SENSOR_READ_INTERVAL 30000  // 30 seconds
#define GAS_THRESHOLD 1200
#define LCD_DISPLAY_TIME 4000       // 4 seconds
#define RECONNECT_INTERVAL 30000    // 30 seconds for reconnection attempts

const char* secretKey = "bcf35bfbe47d73b53007b904d5c54c313c70d7f5b5e3afb6b63f02a9640c8404";

// Initialize objects
hd44780_I2Cexp lcd;
DHT dht(DHTPIN, DHT22);
BlynkTimer timer;

// WiFi credentials
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Hima_lucky";    // Your WiFi name
char pass[] = "kcye3120";          // Your WiFi password

// Global variables
bool wifiConnected = false;
bool blynkConnected = false;
unsigned long lastReconnectAttempt = 0;

struct SensorData {
    float temperature;
    float humidity;
    int gasValue;
    unsigned long timestamp;
};

SensorData lastReading = {0, 0, 0, 0};

// Custom degree symbol for LCD
byte degree_symbol[8] = {
    0b00111,
    0b00101,
    0b00111,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// Function prototypes
void setupWiFi();
void initializeSensors();
void setupLCD();
void checkConnections();
void readAndUpdateSensors();
void updateLCDDisplay();
void updateBlynk();
bool isAirQualityPoor();
void saveToEEPROM();
void handleErrors();
void sendSensor();

BLYNK_CONNECTED() {
    blynkConnected = true;
    Serial.println("Blynk Connected!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Blynk Connected");
    delay(1000);
    Blynk.syncVirtual(V0, V1, V2);
}

void setupWiFi() {
    Serial.println("\n=== WiFi Setup Started ===");
    Serial.printf("Attempting to connect to SSID: %s\n", ssid);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Setup...");
    
    // Complete WiFi shutdown and cleanup
    WiFi.disconnect(true);  // Disconnect and delete old config
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    
    // Set WiFi mode explicitly
    WiFi.mode(WIFI_STA);
    delay(1000);
    
    // Configure WiFi parameters
    WiFi.setAutoReconnect(false);  // We'll handle reconnection manually
    WiFi.persistent(false);        // Don't write to flash
    
    // Optional: Set static IP if DHCP is an issue
    /*
    IPAddress staticIP(192, 168, 1, 200);  // Change to match your network
    IPAddress gateway(192, 168, 1, 1);      // Change to match your network
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(8, 8, 8, 8);
    
    if (!WiFi.config(staticIP, gateway, subnet, dns)) {
        Serial.println("Static IP Configuration Failed");
    }
    */
    
    int attemptCount = 0;
    bool connected = false;
    
    while (attemptCount < MAX_CONNECTION_ATTEMPTS && !connected) {
        attemptCount++;
        Serial.printf("\nConnection attempt %d of %d\n", attemptCount, MAX_CONNECTION_ATTEMPTS);
        
        // Start connection
        WiFi.begin(ssid, pass);
        
        // Wait for connection with timeout
        unsigned long startAttemptTime = millis();
        
        while (millis() - startAttemptTime < WIFI_TIMEOUT) {
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                break;
            }
            
            // Print detailed status every second
            if ((millis() - startAttemptTime) % 1000 == 0) {
                Serial.printf("Status: %d, RSSI: %d dBm\n", WiFi.status(), WiFi.RSSI());
            }
            
            delay(100);
        }
        
        if (!connected) {
            Serial.println("\nConnection attempt failed!");
            Serial.printf("WiFi Status: %d\n", WiFi.status());
            
            switch (WiFi.status()) {
                case WL_NO_SSID_AVAIL:
                    Serial.println("Error: SSID not found!");
                    lcd.setCursor(0, 1);
                    lcd.print("SSID Not Found");
                    break;
                case WL_CONNECT_FAILED:
                    Serial.println("Error: Connection Failed - Check Password");
                    lcd.setCursor(0, 1);
                    lcd.print("Wrong Password");
                    break;
                case WL_DISCONNECTED:
                    Serial.println("Error: Module Not Responding");
                    lcd.setCursor(0, 1);
                    lcd.print("No Response");
                    break;
                default:
                    Serial.printf("Error: Unknown Status %d\n", WiFi.status());
                    lcd.setCursor(0, 1);
                    lcd.print("Unknown Error");
                    break;
            }
            
            // Cleanup before next attempt
            WiFi.disconnect(true);
            delay(WIFI_RETRY_DELAY);
        }
    }
    
    if (connected) {
        wifiConnected = true;
        Serial.println("\nWiFi connected successfully!");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        Serial.printf("Hostname: %s\n", WiFi.getHostname());
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Connected");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
    } else {
        wifiConnected = false;
        Serial.println("\nFailed to connect to WiFi after all attempts");
        Serial.println("Please verify:");
        Serial.println("1. WiFi password is correct");
        Serial.println("2. SSID is visible and in range");
        Serial.println("3. Router is not blocking connection");
        Serial.println("4. Router supports more clients");
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Failed");
        lcd.setCursor(0, 1);
        lcd.print("Check Settings");
    }
    
    Serial.println("=== WiFi Setup Completed ===\n");
}

// Add this new function to verify WiFi credentials
bool verifyWiFiCredentials() {
    if (strlen(ssid) == 0) {
        Serial.println("Error: SSID is empty!");
        return false;
    }
    
    if (strlen(pass) < 8) {
        Serial.println("Error: Password must be at least 8 characters!");
        return false;
    }
    
    Serial.println("WiFi credentials format verified.");
    return true;
}


void checkConnections() {
    unsigned long currentMillis = millis();
    static int reconnectCount = 0;
    
    // Check WiFi
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            Serial.println("WiFi connection lost!");
            Serial.printf("Last known RSSI: %d dBm\n", WiFi.RSSI());
            wifiConnected = false;
        }
        
        if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
            reconnectCount++;
            Serial.printf("Reconnection attempt #%d\n", reconnectCount);
            
            if (reconnectCount >= 5) {
                Serial.println("Multiple reconnection failures. Restarting ESP32...");
                lcd.clear();
                lcd.print("Restarting...");
                delay(1000);
                ESP.restart();
            }
            
            setupWiFi();
            lastReconnectAttempt = currentMillis;
        }
    } else {
        reconnectCount = 0;
    }
    
    // Check Blynk
    if (!blynkConnected && wifiConnected) {
        if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
            Serial.println("Attempting to connect to Blynk...");
            Blynk.connect();
            lastReconnectAttempt = currentMillis;
        }
    }
}



void initializeSensors() {
    dht.begin();
    pinMode(MQ135_PIN, INPUT);
    delay(2000); // Allow sensors to stabilize
    Serial.println("Sensors initialized");
}

void setupLCD() {
    int status = lcd.begin(LCD_COLS, LCD_ROWS);
    if (status) {
        Serial.println("LCD initialization failed!");
        while (1);
    }
    
    lcd.createChar(1, degree_symbol);
    lcd.backlight();
    
    // Display welcome message
    lcd.setCursor(3, 0);
    lcd.print("Air Quality");
    lcd.setCursor(3, 1);
    lcd.print("Monitoring");
    delay(2000);
    lcd.clear();
    Serial.println("LCD initialized");
}



void updateBlynk() {
    if (wifiConnected && blynkConnected) {
        Blynk.virtualWrite(V0, lastReading.temperature);
        Blynk.virtualWrite(V1, lastReading.humidity);
        Blynk.virtualWrite(V2, lastReading.gasValue);
    }
}

void updateLCDDisplay() {
    // Temperature display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temperature ");
    lcd.setCursor(0, 1);
    lcd.print(lastReading.temperature, 1);
    lcd.write(1);
    lcd.print("C");
    delay(LCD_DISPLAY_TIME);
    
    // Humidity display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Humidity ");
    lcd.print(lastReading.humidity, 1);
    lcd.print("%");
    delay(LCD_DISPLAY_TIME);
    
    // Air quality display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gas Value: ");
    lcd.print(lastReading.gasValue);
    lcd.setCursor(0, 1);
    
    if (isAirQualityPoor()) {
        lcd.print("Bad Air");
        if (blynkConnected) {
            Blynk.logEvent("pollution_alert", "Bad Air Quality Detected!");
        }
    } else {
        lcd.print("Fresh Air");
    }
    delay(LCD_DISPLAY_TIME);
}

bool isAirQualityPoor() {
    return lastReading.gasValue >= GAS_THRESHOLD;
}

void saveToEEPROM() {
    EEPROM.put(0, lastReading);
    EEPROM.commit();
}

void handleErrors() {
    static unsigned long lastErrorTime = 0;
    static int errorCount = 0;
    
    if (millis() - lastErrorTime < 60000) {
        errorCount++;
        if (errorCount > 5) {
            lcd.clear();
            lcd.print("Sensor Error!");
            lcd.setCursor(0, 1);
            lcd.print("Check Connection");
            delay(2000);
            errorCount = 0;
        }
    } else {
        errorCount = 1;
    }
    lastErrorTime = millis();
}

// Define the secret key for HMAC
const char* SECRET_KEY = "bcf35bfbe47d73b53007b904d5c54c313c70d7f5b5e3afb6b63f02a9640c8404";

// Function to generate HMAC SHA-256
String generateHMAC(String data) {
    byte hmacResult[32];  // HMAC-SHA256 produces 32-byte output
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)SECRET_KEY, strlen(SECRET_KEY));
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Convert HMAC result to a hex string
    String hmacString = "";
    for (int i = 0; i < 32; i++) {
        if (hmacResult[i] < 16) hmacString += '0';  // Ensure leading zero
        hmacString += String(hmacResult[i], HEX);
    }
    return hmacString;
}

// Function to send data to the server
void sendDataToServer(SensorData data) {
    // Validate sensor readings before sending
    if (data.temperature == 0 || data.humidity == 0) {
        Serial.println("Invalid sensor data. Skipping transmission.");
        return;
    }

    // Create JSON payload
    String payload = "{\"temperature\":" + String(data.temperature, 2) + 
                     ",\"humidity\":" + String(data.humidity, 2) + 
                     ",\"gasValue\":" + String(data.gasValue) + "}";
    
    // Generate HMAC
    String hmac = generateHMAC(payload);

    // Detailed connection diagnostics
    if (!wifiConnected) {
        Serial.println("WiFi not connected. Cannot send data.");
        return;
    }

    HTTPClient http;
    // Use full server URL with protocol and port
    http.begin("http://192.168.109.10:80");  // Replace with EXACT server IP
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Construct POST data
    String postData = "data=" + payload + "&hmac=" + hmac;
    
    Serial.println("Sending Payload: " + payload);
    Serial.println("HMAC: " + hmac);

    int httpResponseCode = http.POST(postData);

    // Comprehensive error handling
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("HTTP Response Code: " + String(httpResponseCode));
        Serial.println("Server Response: " + response);
    } else {
        Serial.println("Error sending data:");
        Serial.println("HTTP Error Code: " + String(httpResponseCode));
        Serial.println("Error Description: " + http.errorToString(httpResponseCode));
    }

    http.end();
}

// Update the sendSensor function to send the hashed data
void sendSensor() {
    lastReading.temperature = dht.readTemperature();
    lastReading.humidity = dht.readHumidity();
    lastReading.gasValue = analogRead(MQ135_PIN);
    lastReading.timestamp = millis();

    if (isnan(lastReading.humidity) || isnan(lastReading.temperature)) {
        Serial.println("Failed to read from DHT sensor!");
        handleErrors();
        return;
    }

    if (wifiConnected && blynkConnected) {
        updateBlynk();
    }

    // Send data and HMAC to the server
    sendDataToServer(lastReading);

    saveToEEPROM();

    Serial.println("\nSensor Readings:");
    Serial.printf("Temperature: %.2f°C\n", lastReading.temperature);
    Serial.printf("Humidity: %.2f%%\n", lastReading.humidity);
    Serial.printf("Gas Value: %d\n", lastReading.gasValue);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Starting Air Quality Monitor ===");
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("I2C Initialized");
    
    // Initialize components
    setupLCD();
    initializeSensors();
    
    // Verify WiFi credentials before attempting connection
    if (verifyWiFiCredentials()) {
        setupWiFi();
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Invalid WiFi");
        lcd.setCursor(0, 1);
        lcd.print("Credentials");
        while(1) delay(1000); // Halt execution
    }
    
    // Initialize Blynk only if WiFi connected
    if (wifiConnected) {
        Blynk.begin(auth, ssid, pass);
    }
    
    timer.setInterval(SENSOR_READ_INTERVAL, sendSensor);
    Serial.println("Setup completed");
}
void loop() {
  Serial.println("------");
    checkConnections();
    
    if (wifiConnected) {
        Blynk.run();
    }
    
    timer.run();
    updateLCDDisplay();
    
    // Print debug info every 10 seconds
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 10000) {
        Serial.println("\n--- Status Update ---");
        Serial.printf("WiFi Connected: %s\n", wifiConnected ? "Yes" : "No");
        Serial.printf("Blynk Connected: %s\n", blynkConnected ? "Yes" : "No");
        Serial.printf("WiFi Signal: %ddBm\n", WiFi.RSSI());
        Serial.printf("Free Memory: %d bytes\n", ESP.getFreeHeap());
        lastDebugPrint = millis();
    }
}