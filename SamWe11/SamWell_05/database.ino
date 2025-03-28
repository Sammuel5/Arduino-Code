#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>  // Added LiquidCrystal I2C Library

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// LCD I2C Setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// QR Scanner Serial Setup
#define RX_PIN 2  
#define TX_PIN 3  
SoftwareSerial qrScanner(RX_PIN, TX_PIN);

// LED Pins
#define RED_LED 4    
#define GREEN_LED 5  

// Relay Pin
#define RELAY_PIN 6  // Define the pin connected to the relay

// WiFi & Firebase Setup
#define WIFI_SSID "Sam"
#define WIFI_PASSWORD "123456789"
#define FIREBASE_HOST "sammuel-17249-default-rtdb.firebaseio.com"

WiFiSSLClient wifiClient;
HttpClient httpClient(wifiClient, "sammuel-17249-default-rtdb.firebaseio.com", 443);

// NTP Client Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
#define GMT_OFFSET_SEC 8 * 3600  // Change this to your timezone offset in seconds (e.g., GMT+8 = 8*3600)
#define NTP_UPDATE_INTERVAL_MS 60000  // Update time every minute

// Month names array for formatting
const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool locked = true; 
bool scanComplete = false;
bool firstScanDone = false;  // New flag to track if first scan is done
String lastQRData = "";      // Store the QR data from first scan
unsigned long lastNtpUpdate = 0;
unsigned long firstScanTime = 0;  // Time when first scan was completed

// Relay timing variables
unsigned long relayStartTime = 0;
bool relayActive = false;

// Initial time setup as fallback
#define INITIAL_YEAR 2025
#define INITIAL_MONTH 3
#define INITIAL_DAY 13
#define INITIAL_HOUR 3
#define INITIAL_MINUTE 11
#define INITIAL_SECOND 0

void displayMessage(const char *message);
void displayQRData(String data);
void sendDataToFirebase(String qrData, bool isTimeIn);
String readQRData();
void connectToWiFi();
String getFormattedDate();
String getFormattedTime();
void setupTime();
void updateTimeFromNTP();
void updateLCDMessage(const char *line1, const char *line2);

void setup() {
    Serial.begin(115200);
    
    // Initialize LCD first thing
    lcd.init();
    lcd.backlight();
    updateLCDMessage("AUTHENTIKEY", "SYSTEM");
    
    // Add a longer delay for welcome message to be visible
    delay(3000);
    
    // Initialize other components
    u8g2.begin();
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);  // Initialize relay pin
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RELAY_PIN, LOW);  // Ensure relay is off initially

    displayMessage("Initializing...");
    delay(1500);

    // Initialize scanner with debug message
    qrScanner.begin(9600);
    Serial.println("QR Scanner initialized on pins RX:" + String(RX_PIN) + " TX:" + String(TX_PIN));
    Keyboard.begin();

    // Use initial time as fallback
    setupTime();
    Serial.println("Initial time set to: " + getFormattedDate() + " " + getFormattedTime());

    digitalWrite(RED_LED, HIGH);
    displayMessage("System Locked");
    delay(1500);

    connectToWiFi();
    
    // Initialize and update NTP client after WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.begin();
        timeClient.setTimeOffset(GMT_OFFSET_SEC);
        updateTimeFromNTP();
    }
}

void setupTime() {
    // Set the time to the values defined in the initial constants as fallback
    setTime(INITIAL_HOUR, INITIAL_MINUTE, INITIAL_SECOND, INITIAL_DAY, INITIAL_MONTH, INITIAL_YEAR);
}

void updateTimeFromNTP() {
    Serial.println("Updating time from NTP server...");
    displayMessage("Syncing time...");
    
    if (timeClient.update()) {
        // Get full date and time from NTP
        unsigned long epochTime = timeClient.getEpochTime();
        
        // Convert to time_t and sync with TimeLib
        setTime(epochTime);
        
        Serial.println("✅ NTP time sync successful!");
        Serial.println("Current time: " + getFormattedDate() + " " + getFormattedTime());
        displayMessage("Time synced!");
        delay(1000);
        
        if (locked) {
            displayMessage("Scan for Time-In");
        } else {
            displayMessage("Scan To Lock");
        }
        
        lastNtpUpdate = millis();
    } else {
        Serial.println("❌ NTP time sync failed!");
        displayMessage("Time sync failed");
        delay(1000);
        
        if (locked) {
            displayMessage("Scan for Time-In");
        } else {
            displayMessage("Scan To Lock");
        }
    }
}

void loop() {
    // Check if it's time to update from NTP
    if (WiFi.status() == WL_CONNECTED && (millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL_MS)) {
        updateTimeFromNTP();
    }
    
    // Add debug for scanner availability
    if (qrScanner.available()) {
        Serial.println("QR Scanner has data available");
    }
    
    if (qrScanner.available() && !scanComplete) {
        String qrData = readQRData();
        
        Serial.println("QR Data received: " + qrData);
        Serial.println("QR Data length: " + String(qrData.length()));
        
        // Check if QR data is valid
        if (qrData.length() == 11 && qrData.startsWith("UA") && qrData.substring(2).toInt() > 0) {  
            scanComplete = true;  
            Serial.println("Valid QR Data: " + qrData);
            displayQRData(qrData);
            
            // Activate the relay
            digitalWrite(RELAY_PIN, HIGH);  // Turn on the relay
            relayStartTime = millis();       // Record the time when relay is activated
            relayActive = true;              // Set relay active flag

            // Send data to Firebase (if needed)
            sendDataToFirebase(qrData, true); // Assuming this is a time-in scan

            // Reset the scanner to prevent multiple reads
            delay(2000);  // Optional: Delay to allow for processing
        } else if (qrData.length() > 0) {
            // We got data but it doesn't match our format
            Serial.println("Invalid QR format: " + qrData);
            displayMessage("Invalid QR code");
            delay(2000);
        }
    }
    
    // Handle relay timing
    if (relayActive) {
        // Check if 10 seconds have passed
        if (millis() - relayStartTime >= 10000) {
            digitalWrite(RELAY_PIN, LOW);  // Turn off the relay
            relayActive = false;            // Reset relay active flag
            scanComplete = false;           // Allow for new scans
        }
    }
}

String getFormattedDate() {
    // Format date as "Mmm dd yyyy" (e.g., "Mar 14 2025")
    char dateBuffer[12];
    sprintf(dateBuffer, "%s %d %04d", 
            monthNames[month() - 1], day(), year());
    return String(dateBuffer);
}

String getFormattedTime() {
    // Format time as "hh:mm:ss AM/PM" (e.g., "02:30:45 PM")
    char timeBuffer[12];
    int h = hour();
    const char* ampm = h >= 12 ? "PM" : "AM";
    h = h > 12 ? h - 12 : h;  // Handle 24-hour to 12-hour conversion
    h = h == 0 ? 12 : h;  // Handle midnight (0 hour) as 12 AM
    
    sprintf(timeBuffer, "%02d:%02d:%02d %s", 
            h, minute(), second(), ampm);
    return String(timeBuffer);
}

void sendDataToFirebase(String qrData, bool isTimeIn) {
    // Get current date and time at the moment of scanning
    String formattedDate = getFormattedDate();
    String formattedTime = getFormattedTime();
    
    // Create a path using the QR code as a key
    String sanitizedDate = formattedDate;
    sanitizedDate.replace(" ", "_");  // Replace spaces with underscores
    
    // Generate a timestamp-based ID for this scan session
    unsigned long epochTime = (unsigned long)now();  
    String timestamp = String(epochTime);
    
    // Create the path - organizing by QR code, then by session timestamp
    String path = "/scannedData/" + qrData + "/sessions/" + timestamp + ".json";
    
    String jsonData;
    if (isTimeIn) {
        // Time-in record
        jsonData = "{\"date\": \"" + formattedDate + "\", "
                + "\"time\": \"" + formattedTime + "\", "
                + "\"status\": \"in\"}";
    } else {
        // Time-out record
        jsonData = "{\"date\": \"" + formattedDate + "\", "
                + "\"time\": \"" + formattedTime + "\", "
                + "\"status\": \"out\"}";
    }

    Serial.println("Sending data to Firebase...");
    Serial.println("Path: " + path);
    Serial.println("Data: " + jsonData);

    httpClient.setTimeout(5000);
    httpClient.beginRequest();
    httpClient.put(path);  // Using PUT instead of POST to set data at a specific path
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();

    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();

    Serial.print("HTTP Status code: ");
    Serial.println(statusCode);
    Serial.println("Firebase Response: " + response);

    if (statusCode == 200) {
        Serial.println("✅ Data sent successfully!");
        
        // Display time info on OLED
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_mr);
        u8g2.drawStr(0, 10, "QR Scanned:");
        u8g2.drawStr(0, 20, qrData.c_str());
        u8g2.drawStr(0, 30, isTimeIn ? "Time IN:" : "Time OUT:");
        u8g2.drawStr(0, 40, formattedTime.c_str());
        u8g2.drawStr(0, 50, formattedDate.c_str());
        u8g2.sendBuffer();
    } else {
        Serial.println("❌ Failed to send data!");
        displayMessage("Firebase Error");
        delay(1000);
    }
}

void displayMessage(const char *message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, message);
    u8g2.sendBuffer();
}

void displayQRData(String data) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, "QR Scanned:");
    u8g2.drawStr(0, 20, data.c_str());
    u8g2.sendBuffer();
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (attempts < 30) {  // Increased timeout
        delay(1000);
        Serial.print(".");
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✅ WiFi Connected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            
            // Check if IP is valid
            if (WiFi.localIP()[0] != 0) {
                displayMessage("WiFi Connected");
                delay(1500);
                displayMessage("Scan for Time-In");
                return;
            } else {
                Serial.println("Invalid IP address. Waiting for valid IP...");
            }
        }
        attempts++;
    }
    
    Serial.println("\n❌ WiFi Connection Failed!");
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
    displayMessage("WiFi Failed!");
}

String readQRData() {
    String data = "";
    unsigned long startTime = millis();
    
    // Increase timeout to 3 seconds (was 1 second)
    while (millis() - startTime < 3000) {
        if (qrScanner.available()) {
            char c = qrScanner.read();
            // Debug what's being received
            Serial.print("Scanner received char: ");
            Serial.print(c);
            Serial.print(" (hex: ");
            Serial.print(c, HEX);
            Serial.println(")");
            
            if (c == '\n') break;
            data += c;
            startTime = millis(); // Reset timeout when we receive data
        }
    }
    data.trim();
    Serial.println("QR Data read complete: " + data);
    return data;
}

// Function to update LCD display - keep this function for initial setup only
void updateLCDMessage(const char *line1, const char *line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}
