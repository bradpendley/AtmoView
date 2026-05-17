#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TFT_eSPI.h>
#include <time.h>

// ── I2C pins for ESP32 CYD ──────────────────────────────────────────────────
// The CYD exposes a JST connector on the back with 3.3V, GND, IO22, IO27
// Use those for I2C: SDA = 22, SCL = 27
#define I2C_SDA 22
#define I2C_SCL 27

// ── BME280 ───────────────────────────────────────────────────────────────────
// Default I2C address is 0x76 (SDO tied to GND) or 0x77 (SDO tied to VCC)
#define BME280_ADDRESS 0x77

Adafruit_BME280 bme;
TFT_eSPI tft = TFT_eSPI();

// ── Layout constants ─────────────────────────────────────────────────────────
// Color theme selection: Set USE_BW_THEME to true for black & white theme
const bool USE_BW_THEME = true;

// Color palette - Dark Neon Theme
const uint16_t COLOR_BG_DARK        = 0x0000;   // Deep black
const uint16_t COLOR_QUAD_BG_DARK   = 0x1082;   // Dark charcoal
const uint16_t COLOR_ACCENT_1_DARK  = 0x07FF;   // Cyan
const uint16_t COLOR_ACCENT_2_DARK  = 0x07E0;   // Mint green
const uint16_t COLOR_ACCENT_3_DARK  = 0xFD20;   // Soft orange
const uint16_t COLOR_ACCENT_4_DARK  = 0x00FF;   // Light cyan
const uint16_t COLOR_LABEL_DARK     = 0x8410;   // Subtle gray
const uint16_t COLOR_ERROR_DARK     = TFT_RED;
const uint16_t COLOR_BORDER_DARK    = 0x2945;   // Subtle border color

// Color palette - Black & White Theme
const uint16_t COLOR_BG_BW          = TFT_BLACK;
const uint16_t COLOR_QUAD_BG_BW     = TFT_BLACK;
const uint16_t COLOR_ACCENT_1_BW    = TFT_WHITE;
const uint16_t COLOR_ACCENT_2_BW    = TFT_WHITE;
const uint16_t COLOR_ACCENT_3_BW    = TFT_WHITE;
const uint16_t COLOR_ACCENT_4_BW    = TFT_WHITE;
const uint16_t COLOR_LABEL_BW       = 0x8410;   // Light gray
const uint16_t COLOR_ERROR_BW       = TFT_RED;
const uint16_t COLOR_BORDER_BW      = 0x4208;   // Dark gray border

// Active theme
const uint16_t COLOR_BG        = USE_BW_THEME ? COLOR_BG_BW : COLOR_BG_DARK;
const uint16_t COLOR_QUAD_BG   = USE_BW_THEME ? COLOR_QUAD_BG_BW : COLOR_QUAD_BG_DARK;
const uint16_t COLOR_ACCENT_1  = USE_BW_THEME ? COLOR_ACCENT_1_BW : COLOR_ACCENT_1_DARK;
const uint16_t COLOR_ACCENT_2  = USE_BW_THEME ? COLOR_ACCENT_2_BW : COLOR_ACCENT_2_DARK;
const uint16_t COLOR_ACCENT_3  = USE_BW_THEME ? COLOR_ACCENT_3_BW : COLOR_ACCENT_3_DARK;
const uint16_t COLOR_ACCENT_4  = USE_BW_THEME ? COLOR_ACCENT_4_BW : COLOR_ACCENT_4_DARK;
const uint16_t COLOR_LABEL     = USE_BW_THEME ? COLOR_LABEL_BW : COLOR_LABEL_DARK;
const uint16_t COLOR_ERROR     = USE_BW_THEME ? COLOR_ERROR_BW : COLOR_ERROR_DARK;
const uint16_t COLOR_BORDER    = USE_BW_THEME ? COLOR_BORDER_BW : COLOR_BORDER_DARK;

bool bmeOk = false;
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 2000;  // ms

// ── Previous values for partial redraw ───────────────────────────────────────
float prevTemp = -999, prevHum = -999, prevPres = -999, prevAlt = -999;

// ─────────────────────────────────────────────────────────────────────────────
void drawStaticUI() {
  tft.fillScreen(COLOR_BG);

  // 2x2 quadrants with spacing and beveled corners
  const uint16_t MARGIN = 3;         // Gap between quadrants
  const uint16_t QUAD_W = 156;       // Slightly smaller quadrants
  const uint16_t QUAD_H = 116;

  // Helper function to draw rounded rectangle
  auto drawRoundedQuad = [&](int x, int y, uint16_t w, uint16_t h, const char* label, int label_y) {
    const int radius = 6;
    
    // Fill center rectangle
    tft.fillRect(x + radius, y, w - 2*radius, h, COLOR_QUAD_BG);
    tft.fillRect(x, y + radius, w, h - 2*radius, COLOR_QUAD_BG);
    
    // Fill corner circles for rounded corners
    for (int i = 0; i < radius; i++) {
      int offset = i * i;  // For smooth curve
      for (int j = 0; j < radius; j++) {
        if (i*i + j*j <= radius*radius) {
          // Top-left
          tft.drawPixel(x + radius - 1 - i, y + radius - 1 - j, COLOR_QUAD_BG);
          // Top-right
          tft.drawPixel(x + w - radius + i, y + radius - 1 - j, COLOR_QUAD_BG);
          // Bottom-left
          tft.drawPixel(x + radius - 1 - i, y + h - radius + j, COLOR_QUAD_BG);
          // Bottom-right
          tft.drawPixel(x + w - radius + i, y + h - radius + j, COLOR_QUAD_BG);
        }
      }
    }
    
    // Draw border with rounded corners
    // Top edge
    tft.drawFastHLine(x + radius, y, w - 2*radius, COLOR_BORDER);
    // Bottom edge
    tft.drawFastHLine(x + radius, y + h - 1, w - 2*radius, COLOR_BORDER);
    // Left edge
    tft.drawFastVLine(x, y + radius, h - 2*radius, COLOR_BORDER);
    // Right edge
    tft.drawFastVLine(x + w - 1, y + radius, h - 2*radius, COLOR_BORDER);
    
    // Draw corner curves
    for (int i = 0; i < radius; i++) {
      for (int j = 0; j < radius; j++) {
        if (i*i + j*j == radius*radius || (i*i + j*j < radius*radius && (i == radius-1 || j == radius-1))) {
          // Top-left corner
          tft.drawPixel(x + radius - 1 - i, y + radius - 1 - j, COLOR_BORDER);
          // Top-right corner
          tft.drawPixel(x + w - radius + i, y + radius - 1 - j, COLOR_BORDER);
          // Bottom-left corner
          tft.drawPixel(x + radius - 1 - i, y + h - radius + j, COLOR_BORDER);
          // Bottom-right corner
          tft.drawPixel(x + w - radius + i, y + h - radius + j, COLOR_BORDER);
        }
      }
    }
    
    // Label
    tft.setTextColor(COLOR_LABEL, COLOR_QUAD_BG);
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + w/2, label_y);
  };

  // Top-left (Temperature)
  drawRoundedQuad(0, 0, QUAD_W, QUAD_H, "TEMP", 12);

  // Top-right (Humidity)
  drawRoundedQuad(QUAD_W + MARGIN, 0, QUAD_W, QUAD_H, "HUMIDITY", 12);

  // Bottom-left (Pressure)
  drawRoundedQuad(0, QUAD_H + MARGIN, QUAD_W, QUAD_H, "PRESSURE", QUAD_H + MARGIN + 12);

  // Bottom-right (Altitude)
  drawRoundedQuad(QUAD_W + MARGIN, QUAD_H + MARGIN, QUAD_W, QUAD_H, "ALT", QUAD_H + MARGIN + 12);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw value in a specific quadrant with smooth font
// quad_x, quad_y: top-left corner of quadrant
// quad_w, quad_h: quadrant dimensions
void drawQuadValue(float val, float &prev, uint16_t accentColor, 
                   int quad_x, int quad_y, int quad_w, int quad_h,
                   const char* fmt, const char* unit) {
  if (val == prev) return;
  prev = val;

  // Calculate center positions for value display
  int value_y = quad_y + (quad_h / 2);

  // Erase value area
  tft.fillRect(quad_x + 10, value_y - 18, quad_w - 20, 36, COLOR_QUAD_BG);

  char buf[32];
  snprintf(buf, sizeof(buf), fmt, val);

  // Display value (large font, centered)
  tft.setTextFont(4);
  tft.setTextColor(accentColor, COLOR_QUAD_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(buf, quad_x + (quad_w / 2), value_y);

  // Display unit (Font 2 - medium, positioned close to the right of value)
  tft.setTextFont(2);
  tft.setTextColor(accentColor, COLOR_QUAD_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(unit, quad_x + (quad_w / 2) + 30, value_y - 3);
}

// ─────────────────────────────────────────────────────────────────────────────
void drawDate() {
  // Get current date from system time
  time_t now = time(nullptr);
  
  // If time is not set (before 2000), set a default
  if (now < 946684800) {  // Timestamp for 2000-01-01
    now = 1747939200;      // Default to May 17, 2026
  }
  
  struct tm* timeinfo = localtime(&now);

  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%b %d", timeinfo);

  // Draw date at the center gap area between quadrants
  int centerX = 159;
  int centerY = 118;

  // Draw date with accent color - no background fill
  tft.setTextFont(4);
  tft.setTextColor(COLOR_ACCENT_1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(dateStr, centerX, centerY);
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

  Wire.begin(I2C_SDA, I2C_SCL);


  if (!bme.begin(BME280_ADDRESS, &Wire)) {
    Serial.println("BME280 not found! Check address (0x76 / 0x77) and wiring.");
    showError("Not found at 0x76");
    bmeOk = false;
  } else {
    Serial.println("BME280 found.");

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

  Serial.printf("Temp: %.2f °F | Hum: %.2f %% | Pres: %.2f hPa | Alt: %.1f m\n",
                temp, hum, pres, alt);

  // Quadrant dimensions with spacing and beveled corners
  const uint16_t MARGIN = 3;
  const uint16_t QUAD_W = 156;
  const uint16_t QUAD_H = 116;

  // Top-left: Temperature
  drawQuadValue(temp, prevTemp, COLOR_ACCENT_1, 0, 0, QUAD_W, QUAD_H, "%.1f", "F");

  // Top-right: Humidity
  drawQuadValue(hum, prevHum, COLOR_ACCENT_2, QUAD_W + MARGIN, 0, QUAD_W, QUAD_H, "%.1f", "%");

  // Bottom-left: Pressure
  drawQuadValue(pres, prevPres, COLOR_ACCENT_3, 0, QUAD_H + MARGIN, QUAD_W, QUAD_H, "%.1f", " hPa");

  // Bottom-right: Altitude
  drawQuadValue(alt, prevAlt, COLOR_ACCENT_4, QUAD_W + MARGIN, QUAD_H + MARGIN, QUAD_W, QUAD_H, "%.0f", "m");

  // Update date in center
  drawDate();
}