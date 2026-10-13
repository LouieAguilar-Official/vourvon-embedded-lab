
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------------- WIFI ----------------
const char* ssid = "Vourvon";
const char* password = "InfiniteIntelligence";

WebServer server(80);

// ---------------- PIN DEFINITIONS ----------------
#define RM3100_CS      5
#define RM3100_DRDY    4
#define SD_CS          22
#define STATUS_LED     21
#define SD_LED         2
#define SD_WRITE_LED   25
#define john 10

// ---------------- RM3100 REGISTERS ----------------
#define REG_CMM   0x01
#define REG_TMRC  0x0B
#define REG_MX    0x24

SPIClass spi(VSPI);
SPISettings rmSPI(1000000, MSBFIRST, SPI_MODE0);

// ---------------- VARIABLES ----------------
File logFile;

bool loggingActive = false;

unsigned long timestamp = 0;
unsigned long sdLedTimer = 0;
unsigned long sdWriteLedTimer = 0;

const unsigned long ledDuration = 100;

int fileIndex = 0;
char filename[32];


// =================================================
// WEB PAGE
// =================================================
void handleRoot() {

  String page = "<html><head>";

  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<style>";
  page += "body{font-family:Arial;text-align:center;background:#111;color:white;}";
  page += "button{width:80%;height:120px;font-size:40px;margin:20px;border-radius:15px;border:none;}";
  page += ".start{background:#00cc66;color:white;}";
  page += ".stop{background:#ff3333;color:white;}";
  page += ".download{background:#3399ff;color:white;}";
  page += "</style>";

  page += "</head><body>";

  page += "<h1>RM3100 Logger</h1>";

  if (loggingActive)
    page += "<h2 style='color:#00ff99;'>LOGGING ACTIVE</h2>";
  else
    page += "<h2 style='color:#ff5555;'>LOGGING STOPPED</h2>";

  page += "<a href='/start'><button class='start'>START</button></a>";
  page += "<br>";
  page += "<a href='/stop'><button class='stop'>STOP</button></a>";
  page += "<br>";
  page += "<a href='/download'><button class='download'>DOWNLOAD FILE</button></a>";

  page += "</body></html>";

  server.send(200, "text/html", page);
}


// =================================================
// START LOGGING
// =================================================
void handleStart() {

  if (!loggingActive) {

    loggingActive = true;

    Serial.println("LOGGING STARTED");

    digitalWrite(STATUS_LED, HIGH);

    createNewLogFile();
  }

  server.sendHeader("Location","/");
  server.send(303);
}


// =================================================
// STOP LOGGING
// =================================================
void handleStop() {

  if (loggingActive) {

    loggingActive = false;

    Serial.println("LOGGING STOPPED");

    digitalWrite(STATUS_LED, LOW);
  }

  server.sendHeader("Location","/");
  server.send(303);
}


// =================================================
// DOWNLOAD FILE
// =================================================
void handleDownload() {

  File file = SD.open(filename);

  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=" + String(filename));

  server.streamFile(file, "text/csv");

  file.close();
}


// =================================================
// BOOT LED
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

  for (int i = 0; i < 9; i++)
    buf[i] = spi.transfer(0x00);

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
// CREATE FILE
// =================================================
void createNewLogFile() {

  fileIndex++;

  sprintf(filename, "/ScanFile_%03d.csv", fileIndex);

  logFile = SD.open(filename, FILE_WRITE);

  if (logFile) {

    logFile.println("Time(ms),X,Y,Z");
    logFile.close();

    Serial.print("Created file: ");
    Serial.println(filename);
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

  pinMode(STATUS_LED, OUTPUT);
  pinMode(SD_LED, OUTPUT);
  pinMode(SD_WRITE_LED, OUTPUT);

  digitalWrite(RM3100_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  spi.begin(18,19,23,RM3100_CS);

  delay(100);

  bootBlink();

  // SD INIT
  if (!SD.begin(SD_CS, spi)) {

    Serial.println("SD Card Failed!");
    while (1);
  }

  Serial.println("SD Card Ready");

  // RM3100 CONFIG
  rmWrite(0x04,0x00);
  rmWrite(0x05,0xC8);
  rmWrite(0x06,0x00);
  rmWrite(0x07,0xC8);
  rmWrite(0x08,0x00);
  rmWrite(0x09,0xC8);

  rmWrite(REG_CMM,0x79);
  rmWrite(REG_TMRC,0x9C);

  Serial.println("RM3100 Ready");

  // WIFI
  WiFi.begin(ssid,password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/download", handleDownload);

  server.begin();
}


// =================================================
// LOOP
// =================================================
void loop() {

  server.handleClient();

  if (loggingActive && digitalRead(RM3100_DRDY)) {

    int32_t x,y,z;

    rmReadXYZ(x,y,z);

    timestamp = millis();

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

      digitalWrite(SD_LED,HIGH);
      sdLedTimer = millis();

      digitalWrite(SD_WRITE_LED,LOW);
      sdWriteLedTimer = millis();
    }
  }

  if (digitalRead(SD_LED)==HIGH && millis()-sdLedTimer>ledDuration)
    digitalWrite(SD_LED,LOW);

  if (digitalRead(SD_WRITE_LED)==LOW && millis()-sdWriteLedTimer>ledDuration)
    digitalWrite(SD_WRITE_LED,HIGH);
}
