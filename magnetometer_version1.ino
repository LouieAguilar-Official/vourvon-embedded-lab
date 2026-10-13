#include <SPI.h>
#include <SD.h>
#include "BluetoothSerial.h"

// ---------------- BLUETOOTH ----------------
BluetoothSerial SerialBT;

// ---------------- PIN DEFINITIONS ----------------
#define RM3100_CS      5
#define RM3100_DRDY    4
#define SD_CS          22
#define STATUS_LED     25
#define SD_LED         2
#define SD_WRITE_LED   27

// ---------------- RM3100 ----------------
#define REG_CMM   0x01
#define REG_TMRC  0x0B
#define REG_MX    0x24

SPIClass spi(VSPI);
SPISettings rmSPI(1000000, MSBFIRST, SPI_MODE0);

// ---------------- FILE ----------------
File logFile;
bool loggingActive = false;

// ---------------- DETECTION ----------------
float avgX = 0, avgY = 0, avgZ = 0;
bool baselineInit = false;

// ---------------- SENSITIVITY ----------------
int sensitivity = 5;

// ---------------- SIGNAL ----------------
float lastStrength = 0;

// ---------------- FILTER ----------------
const float alpha = 0.03;

// ---------------- BLUETOOTH INPUT ----------------
String btInput = "";

// =================================================
// RM3100 WRITE  (FIXED - REQUIRED)
// =================================================
void rmWrite(uint8_t reg, uint8_t value) {
    spi.beginTransaction(rmSPI);
    digitalWrite(RM3100_CS, LOW);
    spi.transfer(reg & 0x7F);
    spi.transfer(value);
    digitalWrite(RM3100_CS, HIGH);
    spi.endTransaction();
}

// =================================================
// SENSOR READ
// =================================================
void rmReadXYZ(int32_t &x, int32_t &y, int32_t &z) {
    uint8_t buf[9];

    spi.beginTransaction(rmSPI);
    digitalWrite(RM3100_CS, LOW);
    spi.transfer(REG_MX | 0x80);

    for (int i = 0; i < 9; i++) {
        buf[i] = spi.transfer(0x00);
    }

    digitalWrite(RM3100_CS, HIGH);
    spi.endTransaction();

    x = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2];
    y = ((int32_t)buf[3] << 16) | ((int32_t)buf[4] << 8) | buf[5];
    z = ((int32_t)buf[6] << 16) | ((int32_t)buf[7] << 8) | buf[8];

    if (x & 0x800000) x |= 0xFF000000;
    if (y & 0x800000) y |= 0xFF000000;
    if (z & 0x800000) z |= 0xFF000000;
}

// =================================================
// THRESHOLD
// =================================================
int getThreshold() {
    return map(sensitivity, 1, 10, 1000, 150);
}

// =================================================
// BLUETOOTH COMMANDS
// =================================================
void handleBT(String cmd) {
    cmd.trim();
    cmd.toUpperCase();

    // START / STOP
    if (cmd == "START") {
        loggingActive = true;
        SerialBT.println("LOG STARTED");
    }

    if (cmd == "STOP") {
        loggingActive = false;
        SerialBT.println("LOG STOPPED");
    }

    // sensitivity DOWN (1)
    if (cmd == "1") {
        sensitivity = max(1, sensitivity - 1);
        SerialBT.print("Sensitivity: ");
        SerialBT.println(sensitivity);
    }

    // sensitivity UP (0)
    if (cmd == "0") {
        sensitivity = min(10, sensitivity + 1);
        SerialBT.print("Sensitivity: ");
        SerialBT.println(sensitivity);
    }
}

// =================================================
// SETUP
// =================================================
void setup() {
    Serial.begin(115200);
    SerialBT.begin("RM3100_DETECTOR");

    pinMode(RM3100_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(RM3100_DRDY, INPUT);
    pinMode(STATUS_LED, OUTPUT);
    pinMode(SD_LED, OUTPUT);
    pinMode(SD_WRITE_LED, OUTPUT);

    digitalWrite(RM3100_CS, HIGH);
    digitalWrite(SD_CS, HIGH);

    spi.begin(18, 19, 23, RM3100_CS);

    if (!SD.begin(SD_CS, spi)) {
        Serial.println("SD Failed");
        while (1);
    }

    Serial.println("READY");
    SerialBT.println("READY");

    // RM3100 CONFIG
    rmWrite(0x04, 0x00);
    rmWrite(0x05, 0xC8);
    rmWrite(0x06, 0x00);
    rmWrite(0x07, 0xC8);
    rmWrite(0x08, 0x00);
    rmWrite(0x09, 0xC8);
    rmWrite(REG_CMM, 0x79);
    rmWrite(REG_TMRC, 0x9C);
}

// =================================================
// LOOP
// =================================================
void loop() {

    // ---------------- BLUETOOTH INPUT ----------------
    while (SerialBT.available()) {
        char c = SerialBT.read();

        if (c == '\n' || c == '\r') {
            if (btInput.length() > 0) {
                handleBT(btInput);
                btInput = "";
            }
        } else {
            btInput += c;
        }
    }

    // ---------------- SENSOR ----------------
    if (loggingActive && digitalRead(RM3100_DRDY)) {

        int32_t x, y, z;
        rmReadXYZ(x, y, z);

        // ---------------- BASELINE ----------------
        if (!baselineInit) {
            avgX = x;
            avgY = y;
            avgZ = z;
            baselineInit = true;
        }

        int dx = abs(x - avgX);
        int dy = abs(y - avgY);
        int dz = abs(z - avgZ);

        int strength = dx + dy + dz;

        float deltaStrength = abs(strength - lastStrength);
        lastStrength = strength;

        int threshold = getThreshold();

        bool stable = (deltaStrength < 50);

        bool target = false;
        if (!stable && strength > threshold) {
            target = true;
        }

        // baseline update only when stable
        if (stable && !target) {
            avgX = (alpha * x) + (1 - alpha) * avgX;
            avgY = (alpha * y) + (1 - alpha) * avgY;
            avgZ = (alpha * z) + (1 - alpha) * avgZ;
        }

        int confidence = map(strength, 0, 5000, 0, 100);
        confidence = constrain(confidence, 0, 100);

        // ---------------- OUTPUT ----------------
        Serial.print("STR:");
        Serial.print(strength);

        Serial.print(" dSTR:");
        Serial.print(deltaStrength);

        Serial.print(" CONF:");
        Serial.print(confidence);
        Serial.print("% S:");
        Serial.print(sensitivity);

        Serial.println(target ? " >>> TARGET" : " >>> SOIL");

        SerialBT.print("X:");
        SerialBT.print(x);

        SerialBT.print(" Y:");
        SerialBT.print(y);

        SerialBT.print(" Z:");
        SerialBT.print(z);
        
        SerialBT.print("STR:");
        SerialBT.print(strength);

        SerialBT.print(" dSTR:");
        SerialBT.print(deltaStrength);

        SerialBT.print(" CONF:");
        SerialBT.print(confidence);
        SerialBT.print("% S:");
        SerialBT.print(sensitivity);

        SerialBT.println(target ? " >>> TARGET" : " >>> SOIL");
    }
}