#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <Firebase_ESP_Client.h>  // Correct Firebase Library

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// QR Scanner Serial Setup
#define RX_PIN 2  
#define TX_PIN 3  
SoftwareSerial qrScanner(RX_PIN, TX_PIN);

// LED Pins
#define RED_LED 4    
#define GREEN_LED 5  

// Firebase Setup
#define WIFI_SSID "Shayn"
#define WIFI_PASSWORD "123456789"
#define FIREBASE_HOST "sammuel-17249-default-rtdb.firebaseio.com"  
#define FIREBASE_AUTH "ft8spYCJK6juy3H0a5MT2kZd1mSmaoumehk3lC2q"  

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

bool locked = true; 
bool scanComplete = false; 

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
  digitalWrite(GREEN_LED, LOW);
  displayMessage(F("System Locked"));
  delay(1500);
  displayMessage(F("Scan To Unlock"));

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(F("Connecting to WiFi..."));
  }
  Serial.println(F("Connected to WiFi"));

  // Firebase Setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (qrScanner.available() && !scanComplete) {
    String qrData = readQRData();

    if (qrData.length() > 0) {  
      scanComplete = true;  
      Serial.println("QR Data: " + qrData);
      displayQRData(qrData);
      delay(3000);  

      sendToFirebase(qrData);

      if (locked) {  
        displayStatus(F("UNLOCKING PC..."));
        unlockPC();
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        locked = false;
      } else {  
        displayStatus(F("LOCKING PC...")); 
        lockPC();
        digitalWrite(RED_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);
        locked = true;
      }

      delay(1500);
      resetLEDs();  
      delay(3000);  
      scanComplete = false;  
      displayMessage(F("Scan To Lock"));
    }
  }
}

void sendToFirebase(String qrData) {
  String path = "/scanned_data";  
  Serial.println(F("Sending to Firebase..."));
  
  if (Firebase.RTDB.setString(&firebaseData, path.c_str(), qrData.c_str())) {
    Serial.println(F("Firebase: Data sent!"));
  } else {
    Serial.println("Firebase Error: " + firebaseData.errorReason());
  }
}

String readQRData() {
  String data = "";
  while (qrScanner.available()) {
    char c = qrScanner.read();
    data += c;
  }
  return data;
}

void unlockPC() {
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(400);
  Keyboard.print(F("notepad"));
  delay(400);
  Keyboard.press(KEY_RETURN);
  Keyboard.releaseAll();
  delay(400);
  Keyboard.print(F("PC Unlocked!"));
  Keyboard.press(KEY_RETURN);
  Keyboard.releaseAll();
}

void lockPC() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('l');
  delay(100);
  Keyboard.releaseAll();
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
