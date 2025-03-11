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

// PC Password
#define PC_PASSWORD "090503"

WiFiClient wifiClient;
HttpClient httpClient(wifiClient, "sammuel-17249-default-rtdb.firebaseio.com", 80);

bool locked = true; 
bool scanComplete = false; 

// Function Declarations
void displayMessage(const char *message);
void displayMessage(const __FlashStringHelper *message);
void displayQRData(String data);
void displayStatus(const char *status);
void sendDataToFirebase(String qrData);
String readQRData();
void resetLEDs();
void indicateError();
void connectToWiFi();
void unlockPC();

void setup() {
    Serial.begin(115200);
    u8g2.begin();

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);

    displayMessage("Initializing...");
    delay(1500);

    qrScanner.begin(9600);
    Keyboard.begin();  

    digitalWrite(RED_LED, HIGH);
    displayMessage("System Locked");
    delay(1500);
    displayMessage("Scan To Unlock");

    connectToWiFi();
}

void loop() {
    if (qrScanner.available() && !scanComplete) {
        String qrData = readQRData();

        if (qrData.length() == 11 && qrData.startsWith("UA") && qrData.substring(2).toInt() > 0) {  
            scanComplete = true;  
            Serial.println("QR Data: " + qrData);
            displayQRData(qrData);
            delay(3000);  

            sendDataToFirebase(qrData);

            delay(2000);  
            scanComplete = false;  
        } else {
            displayMessage("NOT VALID");
            indicateError();
            delay(5000);
            displayMessage("Scan To Unlock");
        }
    }
}

void sendDataToFirebase(String qrData) {
    String path = "/scannedData.json";
    String jsonData = "{\"qrCode\": \"" + qrData + "\"}";

    httpClient.beginRequest();
    httpClient.put(path);
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();

    // Check Firebase response
    int statusCode = httpClient.responseStatusCode();
    String responseBody = httpClient.responseBody();

    Serial.println("Firebase Response Code: " + String(statusCode));
    Serial.println("Firebase Response Body: " + responseBody);

    if (statusCode != 200) {
        displayMessage(F("Firebase Error!"));
        indicateError();
    }
}

void displayMessage(const char *message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, message);
    u8g2.sendBuffer();
}

void displayMessage(const __FlashStringHelper *message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, reinterpret_cast<const char *>(message));
    u8g2.sendBuffer();
}

void displayQRData(String data) {
    displayMessage("QR: ");
    u8g2.drawStr(30, 20, data.c_str());
    u8g2.sendBuffer();
}

void indicateError() {
    for (int i = 0; i < 5; i++) {
        digitalWrite(RED_LED, HIGH);
        delay(200);
        digitalWrite(RED_LED, LOW);
        delay(200);
    }
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
        Serial.print(".");
        delay(1000);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi Connection Failed");
        displayMessage("WiFi Failed!");
        indicateError();
    }
}

void unlockPC() {
    Serial.println("Unlock PC...");
    delay(500);  
    Keyboard.press(KEY_RETURN);
    delay(100);
    Keyboard.releaseAll();
    delay(500);
    Keyboard.print(PC_PASSWORD);
    delay(500);
    Keyboard.press(KEY_RETURN);
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
