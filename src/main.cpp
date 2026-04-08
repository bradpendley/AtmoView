#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TFT_eSPI.h>

// ── I2C pins for ESP32 CYD ──────────────────────────────────────────────────
// The CYD exposes a JST connector on the back with 3.3V, GND, IO22, IO27
// We use those for I2C: SDA = 22, SCL = 27
#define I2C_SDA 22
#define I2C_SCL 27

// ── BME280 ───────────────────────────────────────────────────────────────────
// Default I2C address is 0x76 (SDO tied to GND) or 0x77 (SDO tied to VCC)
#define BME280_ADDRESS 0x77

Adafruit_BME280 bme;
TFT_eSPI tft = TFT_eSPI();

// ── Layout constants ─────────────────────────────────────────────────────────
const uint16_t COLOR_BG     = TFT_BLACK;
const uint16_t COLOR_TITLE  = TFT_CYAN;
const uint16_t COLOR_LABEL  = TFT_DARKGREY;
const uint16_t COLOR_TEMP   = TFT_YELLOW;
const uint16_t COLOR_HUM    = 0x07FF;   // aqua
const uint16_t COLOR_PRES   = 0xFD20;   // orange
const uint16_t COLOR_ALT    = TFT_GREEN;
const uint16_t COLOR_ERROR  = TFT_RED;

bool bmeOk = false;
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 2000;  // ms

// ── Previous values for partial redraw ───────────────────────────────────────
float prevTemp = -999, prevHum = -999, prevPres = -999, prevAlt = -999;

// ─────────────────────────────────────────────────────────────────────────────
void drawStaticUI() {
  tft.fillScreen(COLOR_BG);

  // Title bar
  tft.fillRect(0, 0, 320, 36, TFT_NAVY);
  tft.setTextColor(COLOR_TITLE, TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("Atomo View", 160, 18);

  // Divider
  tft.drawFastHLine(0, 36, 320, TFT_DARKGREY);

  // Row backgrounds (alternating)
  tft.fillRect(0,  50, 320, 44, 0x1082);
  tft.fillRect(0, 100, 320, 44, COLOR_BG);
  tft.fillRect(0, 150, 320, 44, 0x1082);
  tft.fillRect(0, 200, 320, 44, COLOR_BG);

  // Labels (left column)
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);

  tft.setTextColor(COLOR_LABEL, 0x1082);
  tft.drawString("TEMPERATURE", 8, 72);

  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.drawString("HUMIDITY", 8, 122);

  tft.setTextColor(COLOR_LABEL, 0x1082);
  tft.drawString("PRESSURE", 8, 172);

  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.drawString("ALTITUDE", 8, 222);
}

// ─────────────────────────────────────────────────────────────────────────────
void drawValue(float val, float &prev, uint16_t color, uint16_t bgColor,
               int y, const char* fmt, const char* unit) {
  if (val == prev) return;
  prev = val;

  // Erase old value area
  tft.fillRect(150, y + 4, 162, 32, bgColor);

  char buf[24];
  snprintf(buf, sizeof(buf), fmt, val);

  // Value (large)
  tft.setTextFont(4);
  tft.setTextColor(color, bgColor);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(buf, 264, y + 22);

  // Unit (small)
  tft.setTextFont(2);
  tft.drawString(unit, 312, y + 22);
}

// ─────────────────────────────────────────────────────────────────────────────
void showError(const char* msg) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_ERROR, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("BME280 ERROR", 160, 100);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString(msg, 160, 140);
  tft.drawString("Check wiring & I2C address", 160, 160);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // TFT init
  tft.init();
  tft.setRotation(1);   // landscape, USB on the right
  tft.fillScreen(COLOR_BG);

  // Backlight on
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  // I2C init on CYD pins
  Wire.begin(I2C_SDA, I2C_SCL);

  // BME280 init
  if (!bme.begin(BME280_ADDRESS, &Wire)) {
    Serial.println("BME280 not found! Check address (0x76 / 0x77) and wiring.");
    showError("Not found at 0x76");
    bmeOk = false;
  } else {
    Serial.println("BME280 found.");
    // Weather-station sampling: 1x oversampling, normal mode
    bme.setSampling(
      Adafruit_BME280::MODE_NORMAL,
      Adafruit_BME280::SAMPLING_X1,   // temperature
      Adafruit_BME280::SAMPLING_X1,   // pressure
      Adafruit_BME280::SAMPLING_X1,   // humidity
      Adafruit_BME280::FILTER_OFF,
      Adafruit_BME280::STANDBY_MS_500
    );
    bmeOk = true;
    drawStaticUI();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  if (!bmeOk) return;

  if (millis() - lastRead < READ_INTERVAL) return;
  lastRead = millis();

  float temp = bme.readTemperature() * 1.8 + 32; // °F
  float hum  = bme.readHumidity();               // %
  float pres = bme.readPressure() / 100.0f;      // hPa
  float alt  = bme.readAltitude(1013.25f);       // m (sea-level ref)

  // Serial output
  Serial.printf("Temp: %.2f °C | Hum: %.2f %% | Pres: %.2f hPa | Alt: %.1f m\n",
                temp, hum, pres, alt);

  // Screen update (partial redraws only)
  uint16_t bgEven = 0x1082;   // dark row bg
  uint16_t bgOdd  = COLOR_BG;

  drawValue(temp, prevTemp, COLOR_TEMP, bgEven, 50,  "%.1f", "C");
  drawValue(hum,  prevHum,  COLOR_HUM,  bgOdd,  100, "%.1f", "%");
  drawValue(pres, prevPres, COLOR_PRES, bgEven, 150, "%.1f", "hPa");
  drawValue(alt,  prevAlt,  COLOR_ALT,  bgOdd,  200, "%.0f", "m");
}