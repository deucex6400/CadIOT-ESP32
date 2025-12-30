#include "ui_TFT_eSPI.h"

// --- Fallbacks: build safely even if stale header is being used ---
#ifndef TOUCH_DEBOUNCE_MS
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 250;
static uint32_t s_lastTouchMs = 0;
#define USE_LOCAL_DEBOUNCE 1
#endif

void TftEspiUi::begin()
{
  Serial.begin(115200);
  delay(10);
  ensureBacklightOn();

  // --- Display init (ST7789) ---
  tft.init();
  tft.setRotation(ROTATION_LANDSCAPE);
  tft.fillScreen(bg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);

  // Header (blue background, bigger title text)
  tft.setTextSize(2);
  header("CadIOT Relay");
  tft.setTextSize(1);

  // Initial info row
  drawLine(yInfo, "Info", "[Boot]", subtext);

  // Compute right column X for split row (WiFi left, MQTT right)
  const int width  = tft.width();    // ~320
  rightColX = width / 2 + colGap;    // right column starts near middle with small gap

  // Initial split status row (empty values)
  drawSplitStatusRow();

  // Telemetry header block (under WiFi/MQTT)
  tft.fillRect(0, yTelHdr - 2, width, 46, bg);
  tft.setTextColor(TFT_CYAN, bg);
  tft.setCursor(8, yTelHdr);
  tft.print("Telemetry");

  // Relay line (initial)
  drawLineWithBadge(yRelay, "Relay", relayLine, subtext);

  // Divider above buttons
  drawFooterDivider();

  // --- Compute button geometry based on actual width/height ---
  const int height = tft.height();   // ~240
  barY = height - barH;              // bottom bar start
  btnH = barH - 2 * btnPad;          // taller buttons
  btnW = (width - 3 * btnPad) / 2;   // two buttons

  btn1X = btnPad;                    btn1Y = barY + btnPad;       // Left
  btn2X = btnPad * 2 + btnW;         btn2Y = barY + btnPad;       // Right

  // --- Touch init (XPT2046 on separate SPI bus) ---
  tsSPI.begin(TP_CLK_PIN, TP_MISO_PIN, TP_MOSI_PIN, TP_CS_PIN);
  ts.begin(tsSPI);
  ts.setRotation(ROTATION_LANDSCAPE);

  drawButtons();
}

void TftEspiUi::setStatus(const char *s)
{
  updateLinesFromStatus(s);

  String msg(s);
  if (msg.startsWith("WiFi")) {
    wifiLine = msg;
    drawSplitStatusRow(); // re-render WiFi (left) + MQTT (right)
  } else if (msg.startsWith("MQTT")) {
    mqttLine = msg;
    drawSplitStatusRow();
  } else if (msg.indexOf("Relay") >= 0) {
    relayLine = msg;
    drawLineWithBadge(yRelay, "Relay", relayLine, subtext);
  } else {
    drawLine(yInfo, "Info", msg, subtext);
  }

  Serial.printf("[TFT STATUS] %s\n", s);
}

void TftEspiUi::showTelemetry(const char *p)
{
  telLine = p;
  tft.fillRect(0, yTelHdr, tft.width(), 46, bg);

  // Telemetry header (accent color already set in begin)
  tft.setTextColor(TFT_CYAN, bg);
  tft.setCursor(8, yTelHdr);
  tft.print("Telemetry");

  // Telemetry value under the split status row
  tft.setTextColor(fg, bg);
  tft.setCursor(8, yTel);
  tft.print(telLine);

  Serial.printf("[TFT TELEMETRY] %s\n", p);
}

void TftEspiUi::logInfo(const char *m)
{
  updateLinesFromInfo(m);
  drawLine(yInfo, "Info", String(m), TFT_GREEN);
  Serial.printf("[TFT INFO] %s\n", m);
}

void TftEspiUi::logError(const char *m)
{
  drawLine(yInfo, "Error", String(m), TFT_RED);
  Serial.printf("[TFT ERROR] %s\n", m);
}

// --- Visuals ---

void TftEspiUi::header(const char* title)
{
  // Blue header panel and red divider
  tft.fillRect(0, yHeader, tft.width(), 30, headerBG);
  tft.fillRect(0, yHeader + 30, tft.width(), 2, accent);

  tft.setTextColor(TFT_WHITE, headerBG);
  tft.setCursor(8, yHeader + 6);
  tft.print(title);
}

void TftEspiUi::drawLine(int y, const char* label, const String& value, uint16_t textColor)
{
  // Clear row area
  tft.fillRect(0, y - 2, tft.width(), 22, bg);

  // Label
  tft.setTextColor(textColor, bg);
  tft.setCursor(20, y);
  tft.print(label);
  tft.print(": ");

  // Value
  tft.setTextColor(fg, bg);
  tft.print(value);
}

void TftEspiUi::drawLineWithBadge(int y, const char* label, const String& value, uint16_t textColor)
{
  // Clear row area
  tft.fillRect(0, y - 2, tft.width(), 22, bg);

  // Badge
  uint16_t bcol = badgeColorFor(label, value);
  tft.fillCircle(10, y + 8, 5, bcol);

  // Optional icon next to Relay: ✔ (on) or × (off)
  if (String(label).equalsIgnoreCase("Relay")) {
    bool on = relayIsOn();
    drawRelayCheckOrX(20, y + 3, on); // small icon near label
  }

  // Label + value
  tft.setTextColor(textColor, bg);
  tft.setCursor(34, y); // shift right to account for icon
  tft.print(label);
  tft.print(": ");

  tft.setTextColor(fg, bg);
  tft.print(value);
}

uint16_t TftEspiUi::badgeColorFor(const char* label, const String& value)
{
  String v = value; v.toLowerCase();
  String lbl(label); lbl.toLowerCase();

  // Relay badge
  if (lbl == "relay") {
    if (v.indexOf("on") >= 0)   return TFT_GREEN;
    if (v.indexOf("off") >= 0)  return TFT_RED;
    return TFT_YELLOW;
  }

  // WiFi/MQTT badge
  if (lbl == "wifi" || lbl == "mqtt") {
    if (v.indexOf("connected") >= 0)    return TFT_GREEN;
    if (v.indexOf("connecting") >= 0 ||
        v.indexOf("reconnect") >= 0)    return TFT_YELLOW;
    if (v.indexOf("disconnected") >= 0 ||
        v.indexOf("failed") >= 0  ||
        v.indexOf("error") >= 0)        return TFT_RED;
    return subtext; // unknown state
  }

  // Default
  return subtext;
}

void TftEspiUi::drawSplitStatusRow()
{
  const int width = tft.width();
  // Clear the split row area
  tft.fillRect(0, yStatus - 2, width, 22, bg);

  // --- Left: WiFi (with badge) ---
  {
    const int leftX = leftColX;
    const int baseline = yStatus;
    // Badge color derived from WiFi line
    uint16_t bWiFi = badgeColorFor("WiFi", wifiLine);
    tft.fillCircle(leftX, baseline + 8, 5, bWiFi);

    tft.setTextColor(subtext, bg);
    tft.setCursor(leftX + 12, baseline);
    tft.print("WiFi: ");

    tft.setTextColor(fg, bg);
    tft.print(wifiLine);
  }

  // --- Right: MQTT (with badge + spinner when connecting) ---
  {
    const int rightX = rightColX;
    const int baseline = yStatus;
    uint16_t bMqtt = badgeColorFor("MQTT", mqttLine);
    tft.fillCircle(rightX, baseline + 8, 5, bMqtt);

    tft.setTextColor(subtext, bg);
    tft.setCursor(rightX + 12, baseline);
    tft.print("MQTT: ");

    // Optional spinner glyph just before the value
    if (mqttIsConnecting()) {
      // Draw spinner at a fixed spot near the label
      drawSpinnerFrame(rightX + 45, baseline); // adjust offset to taste
    }

    tft.setTextColor(fg, bg);
    tft.setCursor(rightX + 45, baseline); // value starts after the spinner
    tft.print(mqttLine);
  }
}

void TftEspiUi::drawFooterDivider()
{
  // Thin divider above the button bar
  tft.drawFastHLine(0, yFooter, tft.width(), panel);
}

void TftEspiUi::ensureBacklightOn()
{
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH); // Backlight ON (per LCDWiki mapping)
}

// --- Spinner & icons ---

bool TftEspiUi::mqttIsConnecting() const
{
  String v = mqttLine; v.toLowerCase();
  return (v.indexOf("connecting") >= 0 || v.indexOf("reconnect") >= 0);
}

bool TftEspiUi::relayIsOn() const
{
  String v = relayLine; v.toLowerCase();
  return (v.indexOf("on") >= 0);
}

void TftEspiUi::drawSpinnerFrame(int x, int y)
{
  // Erase spinner area first (fixed small rect)
  tft.fillRect(x, y - 2, 10, 14, bg);

  // Define spinner frames locally in this scope
  static const char frames[4] = { '|', '/', '-', '\\' };
  char c = frames[spinnerIndex & 0x03];

  tft.setTextColor(TFT_YELLOW, bg);
  tft.setCursor(x + 2, y);
  tft.print(c);
}


void TftEspiUi::drawRelayCheckOrX(int x, int y, bool on)
{
  // Draw a small ✔ (two lines) or × (two crossing lines)
  if (on) {
    // ✔ green
    tft.drawLine(x + 1, y + 6, x + 4, y + 9, TFT_GREEN);
    tft.drawLine(x + 4, y + 9, x + 10, y + 2, TFT_GREEN);
  } else {
    // × red
    tft.drawLine(x + 1, y + 1, x + 9, y + 9, TFT_RED);
    tft.drawLine(x + 9, y + 1, x + 1, y + 9, TFT_RED);
  }
}

// --- Touch ---

// Convert raw touch to screen coords and return true if pressed
bool TftEspiUi::readTouch(uint16_t& sx, uint16_t& sy)
{
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint(); // x, y, z (pressure)
  if (p.z < 40) return false; // threshold 0..255; tweak as needed

  // Axes & mirrors (set via flags)
  uint32_t rawX = swap_xy ? p.y : p.x;
  uint32_t rawY = swap_xy ? p.x : p.y;
  if (mirror_x) rawX = (cal_x_min + cal_x_max) - rawX;
  if (mirror_y) rawY = (cal_y_min + cal_y_max) - rawY;

  const int width  = tft.width();
  const int height = tft.height();
  sx = (uint16_t) map(rawX, cal_x_min, cal_x_max, 0, width);
  sy = (uint16_t) map(rawY, cal_y_min, cal_y_max, 0, height);

  // Clamp
  if (sx > width)  sx = width;
  if (sy > height) sy = height;
  return true;
}

// --- Buttons ---

void TftEspiUi::drawButtons()
{
  // Bar background
  tft.fillRect(0, barY, tft.width(), barH, panel);

  // Use bigger text for button labels
  tft.setTextSize(2);

  // Left button: Test Relay
  tft.fillRoundRect(btn1X, btn1Y, btnW, btnH, 10, TFT_NAVY);
  tft.drawRoundRect(btn1X, btn1Y, btnW, btnH, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setCursor(btn1X + 14, btn1Y + (btnH/2 - 7));
  tft.print("Test Relay");

  // Right button: Relay OFF
  tft.fillRoundRect(btn2X, btn2Y, btnW, btnH, 10, TFT_MAROON);
  tft.drawRoundRect(btn2X, btn2Y, btnW, btnH, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.setCursor(btn2X + 14, btn2Y + (btnH/2 - 7));
  tft.print("Relay OFF");

  // Restore default row text size
  tft.setTextSize(1);
}

bool TftEspiUi::inRect(uint16_t x, uint16_t y, int rx, int ry, int rw, int rh)
{
  return (x >= rx && x < (rx + rw) && y >= ry && y < (ry + rh));
}

void TftEspiUi::loop()
{
  uint16_t x, y;
  if (!readTouch(x, y)) {
    // No touch; animate spinner if needed
    if (mqttIsConnecting()) {
      uint32_t now = millis();
      if (now - lastSpinnerMs >= SPINNER_INTERVAL_MS) {
        lastSpinnerMs = now;
        spinnerIndex = (spinnerIndex + 1) & 0x03;
        // Re-render the split row to refresh spinner glyph
        drawSplitStatusRow();
      }
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastTouchMs < TOUCH_DEBOUNCE_MS) return; // debounce
  lastTouchMs = now;

  if (inRect(x, y, btn1X, btn1Y, btnW, btnH)) {
    // Visual press effect
    tft.fillRoundRect(btn1X, btn1Y, btnW, btnH, 10, TFT_BLUE);
    tft.drawRoundRect(btn1X, btn1Y, btnW, btnH, 10, TFT_WHITE);
    delay(100);
    drawButtons();

    // Action
    if (onTestRelay) onTestRelay();
    else logInfo("No handler: onTestRelay");
  }
  else if (inRect(x, y, btn2X, btn2Y, btnW, btnH)) {
    tft.fillRoundRect(btn2X, btn2Y, btnW, btnH, 10, TFT_RED);
    tft.drawRoundRect(btn2X, btn2Y, btnW, btnH, 10, TFT_WHITE);
    delay(100);
    drawButtons();

    if (onRelayOff) onRelayOff();
    else logInfo("No handler: onRelayOff");
  }
}

// --- Update helpers (SAS fully removed) ---

void TftEspiUi::updateLinesFromStatus(const char* s)
{
  String msg(s);
  if (msg.startsWith("WiFi")) {
    wifiLine = msg;
    drawSplitStatusRow();
  } else if (msg.startsWith("MQTT")) {
    mqttLine = msg;
    drawSplitStatusRow();
  } else if (msg.startsWith("Relay")) {
    relayLine = msg;
    drawLineWithBadge(yRelay, "Relay", relayLine, subtext);
  } else {
    drawLine(yInfo, "Info", msg, subtext);
  }
}

void TftEspiUi::updateLinesFromInfo(const char* m)
{
  String msg(m);
  if (msg.startsWith("WiFi")) {
    wifiLine = msg;
    drawSplitStatusRow();
  } else if (msg.startsWith("MQTT")) {
    mqttLine = msg;
    drawSplitStatusRow();
  } else if (msg.startsWith("Relay")) {
    relayLine = msg;
    drawLineWithBadge(yRelay, "Relay", relayLine, subtext);
  } else {
    drawLine(yInfo, "Info", msg, subtext);
  }
}