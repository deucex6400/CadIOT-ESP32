#pragma once
#include "ui_IUiAdapter.h"

class SSD1306Ui : public IUiAdapter
{
public:
    void begin() override;
    void setStatus(const char *) override;
    void showTelemetry(const char *) override;
    void logInfo(const char *) override;
    void logError(const char *) override;
};