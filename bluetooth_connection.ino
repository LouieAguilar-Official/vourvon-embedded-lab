#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

#define LED_PIN 2

bool blinking = false;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_LED_LOUIE"); // cleaner name

  pinMode(LED_PIN, OUTPUT);

  Serial.println("Bluetooth ready. Send 1 to blink, 0 to stop.");
}

void loop() {
  // Read Bluetooth command
  if (SerialBT.available()) {
    char command = SerialBT.read();

    if (command == '1') {
      blinking = true;
    } 
    else if (command == '0') {
      blinking = false;
      digitalWrite(LED_PIN, LOW);
    }
  }

  // Fast blinking
  if (blinking) {
    digitalWrite(LED_PIN, HIGH);
    delay(20);  // fast ON
    digitalWrite(LED_PIN, LOW);
    delay(20);  // fast OFF
  }
}