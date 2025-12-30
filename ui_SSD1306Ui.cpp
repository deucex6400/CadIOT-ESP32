#include <Arduino.h>
#include "ui_SSD1306Ui.h"

void SSD1306Ui::begin() { Serial.begin(115200); }
void SSD1306Ui::setStatus(const char *s) { Serial.printf("[SSD1306 STATUS] %s\n", s); }
void SSD1306Ui::showTelemetry(const char *p) { Serial.printf("[SSD1306 TELEMETRY] %s\n", p); }
void SSD1306Ui::logInfo(const char *m) { Serial.printf("[SSD1306 INFO] %s\n", m); }
void SSD1306Ui::logError(const char *m) { Serial.printf("[SSD1306 ERROR] %s\n", m); }