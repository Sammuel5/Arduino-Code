#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// QR Scanner Serial Setup
#define RX_PIN 2  
#define TX_PIN 3  
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

void displayMessage(const char *message);
void displayQRData(String data);
void sendDataToFirebase(String qrData);
String readQRData();
void resetLEDs();
void unlockPC();
void lockPC();
void connectToWiFi();

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

    connectToWiFi();
}

void loop() {
    if (qrScanner.available() && !scanComplete) {
        String qrData = readQRData();

        if (qrData.length() == 11 && qrData.startsWith("UA") && qrData.substring(2).toInt() > 0) {  
            scanComplete = true;  
            Serial.println("QR Data: " + qrData);
            displayQRData(qrData);
            sendDataToFirebase(qrData);

            if (locked) {  
                unlockPC();
                digitalWrite(GREEN_LED, HIGH);
                digitalWrite(RED_LED, LOW);
                locked = false;
                displayMessage("Scan To Lock");  
            } else {  
                lockPC();
                digitalWrite(RED_LED, HIGH);
                digitalWrite(GREEN_LED, LOW);
                locked = true;
                displayMessage("Scan To Unlock");  
            }

            delay(2000);  
            scanComplete = false;  
        }
    }
}

void sendDataToFirebase(String qrData) {
    String path = "/scannedData.json?auth=" + String(FIREBASE_AUTH);
    String jsonData = "{\"qrCode\": \"" + qrData + "\"}";

    Serial.println("Sending data to Firebase...");

    httpClient.beginRequest();
    httpClient.put(path);
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();

    int statusCode = httpClient.responseStatusCode();
    if (statusCode == 200) {
        Serial.println("Data sent successfully!");
    } else {
        Serial.print("Failed to send data. HTTP Status Code: ");
        Serial.println(statusCode);
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
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
        Serial.print(".");
        delay(1000);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        displayMessage("WiFi Connected");
        delay(1500);  
        displayMessage("Scan To Unlock");
    } else {
        Serial.println("\nWiFi Connection Failed!");
        displayMessage("WiFi Failed!");
    }
}

void unlockPC() {
  Serial.println(" Unlock PC...");
  delay(500);  
  Keyboard.press(KEY_F1);
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
