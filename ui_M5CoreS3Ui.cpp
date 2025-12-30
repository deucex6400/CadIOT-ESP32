#include <M5Unified.h>
#include "ui_M5CoreS3Ui.h"
#include <Arduino.h>

static bool g_disp_ready = false;
static uint16_t fg = TFT_WHITE, bg = TFT_BLACK;

static void header(const char *title)
{
  M5.Display.fillScreen(bg);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setCursor(10, 10);
  M5.Display.println(title);
}

void M5CoreS3Ui::begin()
{
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.wakeup();
  M5.Display.powerSaveOff();
  M5.Display.setBrightness(200);
  M5.Display.setRotation(1); // landscape-right
  M5.Display.setTextSize(2);
  fg = TFT_WHITE;
  bg = TFT_BLACK;
  header("CadIOT v6 - M5CoreS3");
  g_disp_ready = true;
}

void M5CoreS3Ui::setStatus(const char *s)
{
  if (!g_disp_ready)
    return;
  M5.Display.fillRect(0, 40, M5.Display.width(), 20, bg);
  M5.Display.setTextColor(TFT_CYAN, bg);
  M5.Display.setCursor(10, 40);
  M5.Display.printf("Status: %s", s);
  M5.Display.setTextColor(fg, bg);
}

void M5CoreS3Ui::showTelemetry(const char *p)
{
  if (!g_disp_ready)
    return;
  M5.Display.fillRect(0, 70, M5.Display.width(), 20, bg);
  M5.Display.setTextColor(TFT_YELLOW, bg);
  M5.Display.setCursor(10, 70);
  M5.Display.printf("Telemetry: %s", p);
  M5.Display.setTextColor(fg, bg);
}

void M5CoreS3Ui::logInfo(const char *m)
{
  if (!g_disp_ready)
    return;
  M5.Display.setTextColor(TFT_GREEN, bg);
  M5.Display.setCursor(10, 100);
  M5.Display.printf("INFO: %s\n", m);
  M5.Display.setTextColor(fg, bg);
}

void M5CoreS3Ui::logError(const char *m)
{
  if (!g_disp_ready)
    return;
  M5.Display.setTextColor(TFT_RED, bg);
  M5.Display.setCursor(10, 120);
  M5.Display.printf("ERROR: %s\n", m);
  M5.Display.setTextColor(fg, bg);
}
