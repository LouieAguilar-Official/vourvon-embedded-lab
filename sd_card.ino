//SD Card Tester

#include <SPI.h>
#include <SD.h>

#define SD_CS 5
#define LED_PIN 2   // built-in LED

bool sdReady = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Check SD card
  if (SD.begin(SD_CS)) {
    Serial.println("SD Card Ready");
    sdReady = true;
  } else {
    Serial.println("SD Card Failed");
    sdReady = false;
  }
}

void loop() {

  if (sdReady) {
    // ✅ Blink every 1 second
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(1000);
  } else {
    // ❌ Stay OFF if failed
    digitalWrite(LED_PIN, LOW);
  }
}