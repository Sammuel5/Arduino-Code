#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

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

// Button Pins - UPDATED WITH NEW BUTTONS
#define RIGHT_BUTTON 6  // Yellow button for increasing PC lab number
#define LEFT_BUTTON 7   // Red button for decreasing PC lab number
#define UP_BUTTON 8     // Yellow Button for increasing PC number
#define DOWN_BUTTON 9   // Red Button for decreasing PC number

// WiFi Setup
#define WIFI_SSID "Sam"
#define WIFI_PASSWORD "123456789"

// API Endpoint
#define API_HOST "authentikey-1.onrender.com"
#define API_PATH "/firebase/push-log"
#define API_PORT 443

// Eligibility API endpoint
#define ELIGIBILITY_API_PATH "/check-student-eligibility"
bool isEligible = false;

// JSON Document size for parsing API responses
#define JSON_BUFFER_SIZE 256

WiFiSSLClient wifiClient;
HttpClient httpClient(wifiClient, API_HOST, API_PORT);

// NTP Client Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
#define GMT_OFFSET_SEC 8 * 3600  // Change this to your timezone offset in seconds (e.g., GMT+8 = 8*3600)
#define NTP_UPDATE_INTERVAL_MS 60000  // Update time every minute
#define NTP_TIMEOUT 10000  // 10 second timeout for NTP requests

// Array of NTP servers for fallback
const char* ntpServers[] = {"pool.ntp.org", "time.nist.gov", "time.google.com", "asia.pool.ntp.org"};
const int ntpServerCount = 4;  // Number of servers in the array

// Month names array for formatting
const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool locked = true; 
bool scanComplete = false;
bool firstScanDone = false;  // Flag to track if first scan is done
String lastQRData = "";      // Store the QR data from first scan
String timeInValue = "";     // Store the time-in value
String timeOutValue = "";    // Store the time-out value
String dateValue = "";       // Store the date value
unsigned long lastNtpUpdate = 0;
unsigned long firstScanTime = 0;  // Time when first scan was completed
int currentNtpServer = 0;  // Index of current NTP server

// PC Lab Configuration - UPDATED
int pcNumber = 1;      // Default PC number
int pcLabNumber = 1;   // Default lab number
const int MIN_PC = 1;  // Minimum PC number
const int MAX_PC = 60; // Maximum PC number - adjust this as needed
const int MIN_LAB = 1; // Minimum lab number
const int MAX_LAB = 3; // Maximum lab number - adjust this as needed

// Button state tracking - UPDATED
bool leftButtonPressed = false;
bool rightButtonPressed = false;
bool upButtonPressed = false;
bool downButtonPressed = false;
unsigned long lastButtonPressTime = 0;
#define BUTTON_DEBOUNCE_DELAY 300 // Debounce delay in milliseconds

// Initial time setup as fallback
#define INITIAL_YEAR 2025
#define INITIAL_MONTH 3
#define INITIAL_DAY 13
#define INITIAL_HOUR 3
#define INITIAL_MINUTE 11
#define INITIAL_SECOND 0

// OLED Display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

void displayMessage(const char *message);
void displayQRData(String data);
void sendLogToAPI(String qrData, String timeIn, String timeOut, String date);
String readQRData();
void connectToWiFi();
String getFormattedDate();
String getFormattedTime();
String getFormattedISODate(); // New ISO date format function
void setupTime();
void updateTimeFromNTP();
void updateLCDMessage(const char *line1, const char *line2);
void unlockPC();  // Function to unlock the PC
void lockPC();    // Function to lock the PC
void checkButtons(); // New function for checking button presses
void displayLabConfig(); // New function to display lab configuration
bool checkStudentEligibility(String studentID); // New function to check student eligibility

// New function to display centered text on OLED
void displayCenteredText(int y, const char *text) {
  int width = u8g2.getStrWidth(text);
  int x = (OLED_WIDTH - width) / 2;
  if (x < 0) x = 0;  // Ensure x is never negative
  u8g2.drawStr(x, y, text);
}

void setup() {
    Serial.begin(115200);
    
    // Initialize LCD first thing
    lcd.init();
    lcd.backlight();
  
    lcd.clear();
  
    lcd.setCursor(2, 0); // Center "Authentikey" on top
    lcd.print("AuthentiKey");

    lcd.setCursor(5, 1); // Center "System" on bottom
    lcd.print("System");

    // Add a longer delay for welcome message to be visible
    delay(3000);
    
    // Initialize other components
    u8g2.begin();
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    
    // Set up button pins - UPDATED
    pinMode(LEFT_BUTTON, INPUT_PULLUP);  // Use pull-up resistors
    pinMode(RIGHT_BUTTON, INPUT_PULLUP);
    pinMode(UP_BUTTON, INPUT_PULLUP);    // New button for increasing PC number
    pinMode(DOWN_BUTTON, INPUT_PULLUP);  // New button for decreasing PC number

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
    
    // Show initial lab configuration
    displayLabConfig();
    delay(2000);
    
    displayMessage("System Locked");
    delay(1500);

    connectToWiFi();
    
    // Initialize and update NTP client after WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.begin();
        timeClient.setTimeOffset(GMT_OFFSET_SEC);
        // Increase update interval
        timeClient.setUpdateInterval(NTP_TIMEOUT);
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
    
    // Check WiFi signal strength
    Serial.print("WiFi signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    if (WiFi.RSSI() < -80) {
        Serial.println("⚠️ Warning: WiFi signal is weak, may affect NTP sync");
    }
    
    bool ntpSuccess = false;
    
    // Try primary NTP server first
    Serial.print("Trying NTP server: ");
    Serial.println(ntpServers[currentNtpServer]);
    timeClient.setPoolServerName(ntpServers[currentNtpServer]);
    
    // Attempt update with current server
    bool result = timeClient.update();
    Serial.print("NTP update result: ");
    Serial.println(result ? "Success" : "Failure");
    
    if (result) {
        ntpSuccess = true;
    } else {
        // Try all fallback servers
        for (int i = 0; i < ntpServerCount; i++) {
            // Skip the one we just tried
            if (i == currentNtpServer) continue;
            
            Serial.print("Trying fallback NTP server: ");
            Serial.println(ntpServers[i]);
            timeClient.setPoolServerName(ntpServers[i]);
            
            result = timeClient.update();
            Serial.print("NTP update result: ");
            Serial.println(result ? "Success" : "Failure");
            
            if (result) {
                currentNtpServer = i;  // Remember which server worked
                ntpSuccess = true;
                break;
            }
        }
    }
    
    if (ntpSuccess) {
        // Get full date and time from NTP
        unsigned long epochTime = timeClient.getEpochTime();
        
        // Convert to time_t and sync with TimeLib
        setTime(epochTime);
        
        Serial.println("✅ NTP time sync successful!");
        Serial.print("Using NTP server: ");
        Serial.println(ntpServers[currentNtpServer]);
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
        Serial.println("❌ NTP time sync failed with all servers!");
        // If we're here, all NTP servers failed. Let's rotate to next server for next attempt
        currentNtpServer = (currentNtpServer + 1) % ntpServerCount;
        Serial.print("Next attempt will use: ");
        Serial.println(ntpServers[currentNtpServer]);
        
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
    // First, check for button presses to change lab number
    checkButtons();
    
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

            // Check if the system is locked or unlocked
            if (locked) {
                // First, check if the student is eligible - NEW CODE HERE
                displayMessage("Checking student...");
                if (checkStudentEligibility(qrData)) {
                    // Student is eligible - proceed with unlock
                    lastQRData = qrData;  // Store QR data for verification on second scan
                    timeInValue = getFormattedTime();  // Store the time-in value
                    dateValue = getFormattedDate();    // Store the date
                    firstScanDone = true;  // Set the flag
                    unlockPC();            // Call the unlock function
                    locked = false;        // Unlock the system
                    
                    // Display status messages using centered text
                    u8g2.clearBuffer();
                    u8g2.setFont(u8g2_font_6x10_mr);
                    displayCenteredText(10, "QR Scanned:");
                    displayCenteredText(20, qrData.c_str());
                    displayCenteredText(30, "Time IN recorded:");
                    displayCenteredText(40, timeInValue.c_str());
                    displayCenteredText(50, "Scan again to logout");
                    u8g2.sendBuffer();
                    
                    digitalWrite(RED_LED, LOW);
                    digitalWrite(GREEN_LED, HIGH);
                    
                    delay(2000);
                    displayMessage("System Unlocked");
                    delay(1000);
                    displayMessage("Scan To Lock");
                    
                    Serial.println("Time-In recorded but not sent: " + timeInValue);
                } else {
                    // Student is not eligible - show error and don't unlock
                    u8g2.clearBuffer();
                    u8g2.setFont(u8g2_font_6x10_mr);
                    displayCenteredText(20, "Student ID:");
                    displayCenteredText(30, qrData.c_str());
                    displayCenteredText(40, "Not Registered!");
                    u8g2.sendBuffer();
                    
                    digitalWrite(RED_LED, HIGH);
                    digitalWrite(GREEN_LED, LOW);
                    
                    delay(3000);
                    displayMessage("Access Denied");
                    delay(1000);
                    displayMessage("Scan for Time-In");
                }
            } else {
                // This is the second scan - lock the system
                if (qrData == lastQRData) {  // Fixed logic: check if QR code matches the first scan
                    timeOutValue = getFormattedTime();  // Store the time-out value
                    lockPC();  // Lock the PC
                    locked = true;  // Lock the system
                    
                    // Display status using centered text
                    u8g2.clearBuffer();
                    u8g2.setFont(u8g2_font_6x10_mr);
                    displayCenteredText(10, "QR Scanned:");
                    displayCenteredText(20, qrData.c_str());
                    displayCenteredText(30, "Time OUT recorded:");
                    displayCenteredText(40, timeOutValue.c_str());
                    displayCenteredText(50, "Sending data...");
                    u8g2.sendBuffer();
                    
                    delay(1000);
                    
                    // Now send BOTH time-in and time-out data to API
                    sendLogToAPI(qrData, timeInValue, timeOutValue, dateValue);
                    
                    // Reset flag for next session
                    firstScanDone = false;
                    
                    digitalWrite(RED_LED, HIGH);
                    digitalWrite(GREEN_LED, LOW);
                    
                    displayMessage("System Locked");
                    delay(1000);
                    displayMessage("Scan for Time-In");
                } else {
                    // Different QR code - reject
                    displayMessage("QR Code Mismatch!");
                    delay(2000);
                    displayMessage("Scan again to Lock");
                }
            }

            // Reset the scanner to prevent multiple reads
            delay(2000);  // Optional: Delay to allow for processing
        } else if (qrData.length() > 0) {
            // We got data but it doesn't match our format
            Serial.println("Invalid QR format: " + qrData);
            displayMessage("Invalid QR code");
            delay(2000);
        }
    }
    
    // Reset scanComplete flag to allow for new scans
    scanComplete = false;
}

// COMPLETELY REWRITTEN to properly handle JSON responses
bool checkStudentEligibility(String studentID) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ Not connected to WiFi. Cannot check eligibility.");
        return false;
    }
    
    // Create JSON data for eligibility check - FIXED correctly formatted JSON
    String jsonData = "{\"studentID\":\"" + studentID + "\"}";
    
    Serial.println("Checking student eligibility...");
    Serial.println("Student ID: " + studentID);
    Serial.println("JSON data being sent: " + jsonData);
    
    // Set a timeout for the HTTP request
    httpClient.setTimeout(10000);  // 10 second timeout
    
    // Begin HTTP request
    httpClient.beginRequest();
    httpClient.post(ELIGIBILITY_API_PATH);
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();
    
    // Get response from the server
    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();
    
    Serial.print("Eligibility check - HTTP Status code: ");
    Serial.println(statusCode);
    Serial.println("API Response: " + response);
    
    // Process the response
    if (statusCode >= 200 && statusCode < 300) {
        // Check if the response contains "isEligible"
        if (response.indexOf("isEligible") >= 0) {
            // Check if response contains "true"
            if (response.indexOf("true") >= 0) {
                Serial.println("✅ Student is eligible according to API");
                return true;
            } else {
                Serial.println("❌ Student is not eligible according to API");
                return false;
            }
        } 
        // Fallback check - if the response contains the student ID, assume it's valid
        else if (response.indexOf(studentID) >= 0) {
            Serial.println("✅ Found student ID in response, assuming eligible");
            return true;
        } else {
            Serial.println("❌ No eligibility information found in response");
            return false;
        }
    } else {
        Serial.println("❌ Failed to check eligibility - HTTP error");
        // Fallback behavior - if server is down, allow access
        // Comment out or remove this line if you want to deny access when server is down
        // return true;  
        
        return false;  // Default to denying access on error
    }
}

// Updated function to check all button presses
void checkButtons() {
    // Check if enough time has passed since last button press (debouncing)
    if (millis() - lastButtonPressTime < BUTTON_DEBOUNCE_DELAY) {
        return;
    }
    
    // Read button states (LOW when pressed because of pull-up resistors)
    bool leftButtonState = digitalRead(LEFT_BUTTON) == LOW;
    bool rightButtonState = digitalRead(RIGHT_BUTTON) == LOW;
    bool upButtonState = digitalRead(UP_BUTTON) == LOW;
    bool downButtonState = digitalRead(DOWN_BUTTON) == LOW;
    
    // Check if left button is pressed (to decrease lab number)
    if (leftButtonState && !leftButtonPressed) {
        leftButtonPressed = true;
        lastButtonPressTime = millis();
        
        // Decrease lab number with boundary check
        if (pcLabNumber > MIN_LAB) {
            pcLabNumber--;
            Serial.println("Lab number decreased to: " + String(pcLabNumber));
            
            // Display updated lab configuration
            displayLabConfig();
            delay(5000);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        } else {
            // Already at minimum
            displayMessage("Min Lab Number");
            delay(5000);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        }
    } 
    // Check if right button is pressed (to increase lab number)
    else if (rightButtonState && !rightButtonPressed) {
        rightButtonPressed = true;
        lastButtonPressTime = millis();
        
        // Increase lab number with boundary check
        if (pcLabNumber < MAX_LAB) {
            pcLabNumber++;
            Serial.println("Lab number increased to: " + String(pcLabNumber));
            
            // Display updated lab configuration
            displayLabConfig();
            delay(5000);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        } else {
            // Already at maximum
            displayMessage("Max Lab Number");
            delay(5000);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        }
    }
    // Check if up button is pressed (to increase PC number)
    else if (upButtonState && !upButtonPressed) {
        upButtonPressed = true;
        lastButtonPressTime = millis();
        
        // Increase PC number with boundary check
        if (pcNumber < MAX_PC) {
            pcNumber++;
            Serial.println("PC number increased to: " + String(pcNumber));
            
            // Display updated lab configuration
            displayLabConfig();
            delay(1500);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        } else {
            // Already at maximum
            displayMessage("Max PC Number");
            delay(1000);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        }
    }
    // Check if down button is pressed (to decrease PC number)
    else if (downButtonState && !downButtonPressed) {
        downButtonPressed = true;
        lastButtonPressTime = millis();
        
        // Decrease PC number with boundary check
        if (pcNumber > MIN_PC) {
            pcNumber--;
            Serial.println("PC number decreased to: " + String(pcNumber));
            
            // Display updated lab configuration
            displayLabConfig();
            delay(1500);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        } else {
            // Already at minimum
            displayMessage("Min PC Number");
            delay(1000);
            
            // Return to normal display
            if (locked) {
                displayMessage("Scan for Time-In");
            } else {
                displayMessage("Scan To Lock");
            }
        }
    }
    
    // Reset button states if released
    if (!leftButtonState) {
        leftButtonPressed = false;
    }
    if (!rightButtonState) {
        rightButtonPressed = false;
    }
    if (!upButtonState) {
        upButtonPressed = false;
    }
    if (!downButtonState) {
        downButtonPressed = false;
    }
}

// Updated function to display the current lab configuration with centered text
void displayLabConfig() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    
    displayCenteredText(10, "PC Configuration:");
    
    // Create strings for PC number and lab number
    char pcNumBuffer[20];
    sprintf(pcNumBuffer, "PC Number: %d", pcNumber);
    displayCenteredText(25, pcNumBuffer);
    
    char labNumBuffer[20];
    sprintf(labNumBuffer, "Lab Number: %d", pcLabNumber);
    displayCenteredText(40, labNumBuffer);
    
    // Updated button hint to show all controls
    displayCenteredText(55, "LAB < > | PC ^ v");
    u8g2.sendBuffer();
}

String getFormattedDate() {
    // Format date as "Mmm dd yyyy" (e.g., "Mar 14 2025")
    char dateBuffer[12];
    sprintf(dateBuffer, "%s %d %04d", 
            monthNames[month() - 1], day(), year());
    return String(dateBuffer);
}

// New ISO date format function
String getFormattedISODate() {
    // Format date as ISO format "YYYY-MM-DDThh:mm:ss.000+00:00"
    char dateBuffer[30];
    sprintf(dateBuffer, "%04d-%02d-%02dT%02d:%02d:%02d.000+00:00", 
            year(), month(), day(), hour(), minute(), second());
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

// Modified function to use ISO date format and send the updated PC lab number
void sendLogToAPI(String qrData, String timeIn, String timeOut, String date) {
    // Create correctly formatted JSON data to send to the API
    String jsonData = "{";
    jsonData += "\"studentID\": \"" + qrData + "\", ";
    jsonData += "\"date\": \"" + getFormattedISODate() + "\", ";
    jsonData += "\"pcNumber\": " + String(pcNumber) + ", ";
    jsonData += "\"pcLab\": " + String(pcLabNumber) + ", ";
    jsonData += "\"timeIn\": \"" + timeIn + "\", ";
    jsonData += "\"timeOut\": \"" + timeOut + "\", ";
    jsonData += "\"password\": \"$2a$15$qA8d5vh8g0fC042HrZqNm..gHu9UuoPAG4QBMY2DCr4GiV69tdbr.\"";
    jsonData += "}";

    Serial.println("Sending data to API endpoint...");
    Serial.println("Data: " + jsonData);

    // Set a timeout for the HTTP request
    httpClient.setTimeout(10000);  // 10 second timeout
    
    // Begin HTTP request
    httpClient.beginRequest();
    httpClient.post(API_PATH);  // Using POST to send data to the endpoint
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();

    // Get response from the server
    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();

    Serial.print("HTTP Status code: ");
    Serial.println(statusCode);
    Serial.println("API Response: " + response);

    // Update LED status based on the response
    if (statusCode >= 200 && statusCode < 300) {
        Serial.println("✅ Data sent successfully!");
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        
        // Display success message with time info on OLED using centered text
        // FIXED: String concatenation issues by creating temporary strings
        String inTimeDisplay = "IN: " + timeIn;
        String outTimeDisplay = "OUT: " + timeOut;
        
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_mr);
        displayCenteredText(10, "QR Scanned:");
        displayCenteredText(20, qrData.c_str());
        displayCenteredText(30, "Data Sent:");
        displayCenteredText(40, inTimeDisplay.c_str());
        displayCenteredText(50, outTimeDisplay.c_str());
        u8g2.sendBuffer();
        
        delay(2000);
        
        // Reset LED state
        if (locked) {
            digitalWrite(RED_LED, HIGH);
            digitalWrite(GREEN_LED, LOW);
        } else {
            digitalWrite(RED_LED, LOW);
            digitalWrite(GREEN_LED, HIGH);
        }
    } else {
        Serial.println("❌ Failed to send data!");
        digitalWrite(RED_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);
        
        displayMessage("API Error");
        delay(2000);
        
        if (locked) {
            displayMessage("Scan for Time-In");
        } else {
            displayMessage("Scan To Lock");
        }
    }
}

// Updated function to display centered message
void displayMessage(const char *message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    displayCenteredText(32, message); // Center vertically and horizontally
    u8g2.sendBuffer();
}

// Updated function to display QR data with centered text
void displayQRData(String data) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    displayCenteredText(20, "QR Scanned:");
    displayCenteredText(40, data.c_str());
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
            Serial.print("WiFi signal strength (RSSI): ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
            
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

void unlockPC() {
    Serial.println("Unlocking PC...");
    displayMessage("Unlocking PC...");
    delay(500);
    Keyboard.press(KEY_F1);
    delay(100);
    Keyboard.press(KEY_F1);
    delay(100);
    Keyboard.release(KEY_F1);
    // ito ang dahilan pag mabilis and delay hindi gagana
    delay(1000);
    // Type the password
    Keyboard.print("hannipham");
    delay(100);
    Keyboard.press(KEY_RETURN);
    delay(100);
    Keyboard.release(KEY_RETURN);
    Keyboard.releaseAll();
}

void lockPC() {
    Serial.println("Locking PC...");
    displayMessage("Locking PC...");
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('l');
    delay(100);
    Keyboard.releaseAll();
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
