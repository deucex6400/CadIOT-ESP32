#include <Arduino.h>
#include "ui_HeadlessUi.h"

void HeadlessUi::begin() { Serial.begin(115200); }
void HeadlessUi::setStatus(const char *s) { Serial.printf("[STATUS] %s\n", s); }
void HeadlessUi::showTelemetry(const char *p) { Serial.printf("[TELEMETRY] %s\n", p); }
void HeadlessUi::logInfo(const char *m) { Serial.printf("[INFO] %s\n", m); }
void HeadlessUi::logError(const char *m) { Serial.printf("[ERROR] %s\n", m); }