#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// QR Scanner Serial Setup
#define RX_PIN 2  
#define TX_PIN 3  
SoftwareSerial qrScanner(RX_PIN, TX_PIN);

// LED Pins
#define RED_LED 4    
#define GREEN_LED 5  

bool locked = true; // 🔥 Assume locked at startup
bool scanComplete = false; 

void setup() {
  Serial.begin(115200);
  u8g2.begin();

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Ensure all LEDs are OFF at startup
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  displayMessage(" Initializing...");
  delay(2000);

  qrScanner.begin(9600);
  Keyboard.begin();  

  // 🔥 Correcting LED state at startup
  digitalWrite(RED_LED, HIGH); // 🔴 Always start with RED LED ON (PC is locked)
  digitalWrite(GREEN_LED, LOW);

  displayMessage(" System Is Locked");
  delay(2000);
  displayMessage(" Scan To Unlock");
}

void loop() {
  if (qrScanner.available() && !scanComplete) {
    String qrData = readQRData();

    if (qrData.length() > 0) {  
      scanComplete = true;  
      Serial.println("QR Data:  "  +  qrData);

      displayQRData(qrData);
      delay(4000);  

      if (locked) {  
        displayStatus(" UNLOCK PC...");
        unlockPC();
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        locked = false;
      } else {  
        displayStatus(" LOCKING PC..."); 
        lockPC();
        digitalWrite(RED_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);
        locked = true;
      }

      delay(2000);  
      resetLEDs();  

      delay(5000);  
      scanComplete = false;  
      displayMessage(" Scan To Lock");
    }
  }
}

void lockPC() {
  Serial.println(" Locking PC...");
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('l');
  delay(100);
  Keyboard.releaseAll();
}

void unlockPC() {
  Serial.println(" Unlock PC...");
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

void displayMessage(const char* msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(15, 20, msg);
  u8g2.sendBuffer();
}

void displayQRData(const String& qrData) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(15, 20, " QR Data: " );
  int maxCharsPerLine = 15;  
  int yOffset = 35;
  for (int i = 0; i < qrData.length(); i += maxCharsPerLine) {
    String line = qrData.substring(i, i + maxCharsPerLine);
    u8g2.drawStr(0, yOffset, line.c_str());
    yOffset += 12;  
  }
  u8g2.sendBuffer();  
}

void displayStatus(const char* status) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 30, status);
  u8g2.sendBuffer();
}

String readQRData() {
    String data = "";
    unsigned long startTime = millis();

    while (millis() - startTime < 1000) {  
        while (qrScanner.available()) {
            char c = qrScanner.read();
            if (c != '\r' && c != '\n') {  
                data += c;
            }
        }
    }

    data.trim();  
    return data;
}

void resetLEDs() {
  digitalWrite(RED_LED, LOW);  
  digitalWrite(GREEN_LED, LOW);  
}
