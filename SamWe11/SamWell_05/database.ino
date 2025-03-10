#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// QR Scanner Serial Setup
#define RX_PIN 0  
#define TX_PIN 1  
SoftwareSerial qrScanner(RX_PIN, TX_PIN);

// LED Pins
#define RED_LED 4    
#define GREEN_LED 5  

// WiFi & Firebase Setup
#define WIFI_SSID "Shayn"
#define WIFI_PASSWORD "123456789"
#define FIREBASE_HOST "sammuel-17249-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "ft8spYCJK6juy3H0a5MT2kZd1mSmaoumehk3lC2q"

WiFiClient wifiClient;
HttpClient httpClient(wifiClient, FIREBASE_HOST, 80);

bool locked = true; 
bool scanComplete = false; 

// Function Declarations
void displayMessage(const __FlashStringHelper *message);
void displayQRData(String data);
void displayStatus(const __FlashStringHelper *status);
void sendDataToFirebase(String qrData);
String readQRData();
void unlockPC();
void lockPC();
void resetLEDs();
void indicateError();
void connectToWiFi();

void setup() {
    Serial.begin(115200);
    u8g2.begin();

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);

    displayMessage(F("Initializing..."));
    delay(1500);

    qrScanner.begin(9600);
    Keyboard.begin();  

    digitalWrite(RED_LED, HIGH);
    displayMessage(F("System Locked"));
    delay(1500);
    displayMessage(F("Scan To Unlock"));

    connectToWiFi();
}

void loop() {
    if (qrScanner.available() && !scanComplete) {
        String qrData = readQRData();

        if (qrData.length() == 11 && qrData.startsWith("UA") && isDigit(qrData.charAt(2))) {  
            scanComplete = true;  
            Serial.println("QR Data: " + qrData);
            displayQRData(qrData);
            delay(3000);  

            sendDataToFirebase(qrData);

            if (locked) {  
                displayStatus(F("UNLOCKING PC..."));
                unlockPC();
                digitalWrite(GREEN_LED, HIGH);
                digitalWrite(RED_LED, LOW);
                locked = false;
                displayMessage(F("Scan To Lock"));  
            } else {  
                displayStatus(F("LOCKING PC...")); 
                lockPC();
                digitalWrite(RED_LED, HIGH);
                digitalWrite(GREEN_LED, LOW);
                locked = true;
                displayMessage(F("Scan To Unlock"));  
            }

            delay(1500);
            resetLEDs();  
            delay(2000);  
            scanComplete = false;  
        }
    }
}

void connectToWiFi() {
    Serial.print(F("Connecting to WiFi: "));
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
        Serial.print(".");
        delay(1000);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\nConnected to WiFi!"));
        Serial.print(F("IP Address: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("\nWiFi Connection Failed"));
        displayMessage(F("WiFi Failed!"));
        indicateError();
    }
}

void displayMessage(const __FlashStringHelper *message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, reinterpret_cast<const char*>(message));
    u8g2.sendBuffer();
}

void displayQRData(String data) {
    displayMessage(F("QR: "));
    u8g2.drawStr(30, 20, data.c_str());
    u8g2.sendBuffer();
}

void displayStatus(const __FlashStringHelper *status) {
    displayMessage(status);
}

void resetLEDs() {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
}

void indicateError() {
    for (int i = 0; i < 5; i++) {
        digitalWrite(RED_LED, HIGH);
        delay(200);
        digitalWrite(RED_LED, LOW);
        delay(200);
    }
}

void unlockPC() {
    Serial.println("Unlock PC...");
    delay(500);  
    Keyboard.press(KEY_RETURN);
    delay(100);
    Keyboard.releaseAll();
    delay(500);
    Keyboard.print("090503");
    delay(500);
    Keyboard.press(KEY_RETURN);
    delay(100);
    Keyboard.releaseAll();
}

void lockPC() {
    Serial.println("Locking PC...");
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('l');
    delay(100);
    Keyboard.releaseAll();
}

String readQRData() {
    String data = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < 1000) {
        while (qrScanner.available()) {
            char c = qrScanner.read();
            if (c == '\n') break;
            data += c;
            startTime = millis();
        }
    }
    data.trim();
    return data;
}

void sendDataToFirebase(String qrData) {
    String path = "/scannedData";
    String jsonData = "{\"qrCode\": \"" + qrData + "\"}";
    
    httpClient.beginRequest();
    httpClient.post(path);
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();
    
    int statusCode = httpClient.responseStatusCode();
    Serial.println("Firebase Response: " + String(statusCode));
    Serial.println("Data sent to Firebase: " + qrData);
}
