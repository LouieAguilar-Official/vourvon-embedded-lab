#include <SPI.h>
#include <SD.h>

// ---------------- PIN DEFINITIONS ----------------
#define RM3100_CS      5
#define RM3100_DRDY    4
#define SD_CS          22
#define TRIGGER_BTN    26
#define STATUS_LED     25
#define SD_LED         2
#define SD_WRITE_LED   27

// ---------------- RM3100 REGISTERS ----------------
#define REG_CMM   0x01
#define REG_TMRC  0x0B
#define REG_MX    0x24

// ---------------- SPI ----------------
SPIClass spi(VSPI);
SPISettings rmSPI(1000000, MSBFIRST, SPI_MODE0);

// ---------------- VARIABLES ----------------
File logFile;
bool loggingActive = false;
bool lastButtonState = HIGH;

unsigned long timestamp = 0;
unsigned long sdLedTimer = 0;
unsigned long sdWriteLedTimer = 0;

const unsigned long ledDuration = 100;

int fileIndex = 0;
char filename[32];


// =================================================
// BOOT INDICATOR
// =================================================
void bootBlink() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(150);
    digitalWrite(STATUS_LED, LOW);
    delay(150);
  }
}


// =================================================
// START BLINK INDICATOR (1s delay + 5 fast blink)
// =================================================
void startBlinkIndicator() {

  delay(1000);

  for (int i = 0; i < 5; i++) {
    digitalWrite(SD_WRITE_LED, LOW);
    delay(100);
    digitalWrite(SD_WRITE_LED, HIGH);
    delay(100);
  }
}


// =================================================
// STOP BLINK INDICATOR (1s delay + 5 fast blink)
// =================================================
void stopBlinkIndicator() {

  delay(1000);

  for (int i = 0; i < 5; i++) {
    digitalWrite(SD_WRITE_LED, LOW);
    delay(100);
    digitalWrite(SD_WRITE_LED, HIGH);
    delay(100);
  }
}


// =================================================
// RM3100 WRITE
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
// RM3100 READ XYZ
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
// CREATE NEW FILE
// =================================================
void createNewLogFile() {

  fileIndex++;
  sprintf(filename, "/ScanFile_%03d.csv", fileIndex);

  logFile = SD.open(filename, FILE_WRITE);

  if (logFile) {
    logFile.println("Time(ms),X,Y,Z");
    logFile.close();

    Serial.print("New file created: ");
    Serial.println(filename);
  } 
  else {
    Serial.println("Failed to create file!");
  }
}


// =================================================
// SETUP
// =================================================
void setup() {

  Serial.begin(115200);

  pinMode(RM3100_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(RM3100_DRDY, INPUT);
  pinMode(TRIGGER_BTN, INPUT_PULLUP);

  pinMode(STATUS_LED, OUTPUT);
  pinMode(SD_LED, OUTPUT);
  pinMode(SD_WRITE_LED, OUTPUT);

  digitalWrite(RM3100_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  digitalWrite(STATUS_LED, LOW);
  digitalWrite(SD_LED, LOW);
  digitalWrite(SD_WRITE_LED, HIGH);

  spi.begin(18, 19, 23, RM3100_CS);

  delay(100);

  bootBlink();

  // ---------------- SD CARD INIT ----------------
  if (!SD.begin(SD_CS, spi)) {
    Serial.println("SD Card Failed!");
    while (1);
  }

  Serial.println("SD Card Ready");

  // ---------------- RM3100 CONFIG ----------------
  rmWrite(0x04, 0x00);
  rmWrite(0x05, 0xC8);
  rmWrite(0x06, 0x00);
  rmWrite(0x07, 0xC8);
  rmWrite(0x08, 0x00);
  rmWrite(0x09, 0xC8);

  rmWrite(REG_CMM, 0x79);
  rmWrite(REG_TMRC, 0x9C);

  Serial.println("RM3100 Ready");
}


// =================================================
// LOOP
// =================================================
void loop() {

  bool buttonState = digitalRead(TRIGGER_BTN);

  // ---------------- BUTTON TOGGLE ----------------
  if (buttonState == LOW && lastButtonState == HIGH) {

    loggingActive = !loggingActive;

    if (loggingActive) {

      Serial.println("LOGGING STARTED");

      startBlinkIndicator();

      digitalWrite(STATUS_LED, HIGH);

      createNewLogFile();
    }
    else {

      Serial.println("LOGGING STOPPED");

      stopBlinkIndicator();

      digitalWrite(STATUS_LED, LOW);
    }

    delay(200); // debounce
  }

  lastButtonState = buttonState;


  // ---------------- LOGGING DATA ----------------
  if (loggingActive && digitalRead(RM3100_DRDY)) {

    int32_t x, y, z;

    rmReadXYZ(x, y, z);

    timestamp = millis();

    Serial.print("Time: ");
    Serial.print(timestamp);
    Serial.print(" | X: ");
    Serial.print(x);
    Serial.print(" Y: ");
    Serial.print(y);
    Serial.print(" Z: ");
    Serial.println(z);

    logFile = SD.open(filename, FILE_APPEND);

    if (logFile) {

      logFile.print(timestamp);
      logFile.print(",");
      logFile.print(x);
      logFile.print(",");
      logFile.print(y);
      logFile.print(",");
      logFile.println(z);

      logFile.close();

      digitalWrite(SD_LED, HIGH);
      sdLedTimer = millis();

      digitalWrite(SD_WRITE_LED, LOW);
      sdWriteLedTimer = millis();
    }
  }


  // ---------------- LED TIMER HANDLING ----------------
  if (digitalRead(SD_LED) == HIGH && millis() - sdLedTimer > ledDuration) {
    digitalWrite(SD_LED, LOW);
  }

  if (digitalRead(SD_WRITE_LED) == LOW && millis() - sdWriteLedTimer > ledDuration) {
    digitalWrite(SD_WRITE_LED, HIGH);
  }
}