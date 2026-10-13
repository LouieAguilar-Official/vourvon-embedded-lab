#include <SPI.h>

#define PIN_CS   5
#define PIN_DRDY 4

#define REG_CMM      0x01
#define REG_CCX1     0x04
#define REG_TMRC     0x0B
#define REG_MX       0x24

SPIClass spi = SPIClass(VSPI);

void writeRegister(uint8_t reg, uint8_t value)
{
  digitalWrite(PIN_CS, LOW);
  spi.transfer(reg);
  spi.transfer(value);
  digitalWrite(PIN_CS, HIGH);
}

uint8_t readRegister(uint8_t reg)
{
  digitalWrite(PIN_CS, LOW);
  spi.transfer(reg | 0x80);
  uint8_t val = spi.transfer(0x00);
  digitalWrite(PIN_CS, HIGH);
  return val;
}

void readXYZ(int32_t *x, int32_t *y, int32_t *z)
{
  uint8_t buffer[9];

  digitalWrite(PIN_CS, LOW);
  spi.transfer(REG_MX | 0x80);

  for (int i = 0; i < 9; i++)
    buffer[i] = spi.transfer(0x00);

  digitalWrite(PIN_CS, HIGH);

  *x = ((int32_t)buffer[0] << 16) | ((int32_t)buffer[1] << 8) | buffer[2];
  *y = ((int32_t)buffer[3] << 16) | ((int32_t)buffer[4] << 8) | buffer[5];
  *z = ((int32_t)buffer[6] << 16) | ((int32_t)buffer[7] << 8) | buffer[8];

  if (*x & 0x800000) *x |= 0xFF000000;
  if (*y & 0x800000) *y |= 0xFF000000;
  if (*z & 0x800000) *z |= 0xFF000000;
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DRDY, INPUT);

  digitalWrite(PIN_CS, HIGH);

  spi.begin(18,19,23,5); // SCK, MISO, MOSI, CS

  delay(100);

  // Cycle count configuration
  writeRegister(0x04, 0x00);
  writeRegister(0x05, 0xC8);
  writeRegister(0x06, 0x00);
  writeRegister(0x07, 0xC8);
  writeRegister(0x08, 0x00);
  writeRegister(0x09, 0xC8);

  // continuous measurement mode
  writeRegister(REG_CMM, 0x79);

  // sample rate
  writeRegister(REG_TMRC, 0x94);

  Serial.println("RM3100 Initialized");
}

void loop()
{
  if (digitalRead(PIN_DRDY) == HIGH)
  {
    int32_t x, y, z;

    readXYZ(&x, &y, &z);

    Serial.print("X: ");
    Serial.print(x);
    Serial.print("  Y: ");
    Serial.print(y);
    Serial.print("  Z: ");
    Serial.println(z);
  }
}