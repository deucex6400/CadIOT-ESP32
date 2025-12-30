#pragma once
#include <Arduino.h>

class IUiAdapter
{
public:
    virtual void begin() = 0;
    virtual void setStatus(const char *) = 0;
    virtual void showTelemetry(const char *) = 0;
    virtual void logInfo(const char *) = 0;
    virtual void logError(const char *) = 0;
    virtual ~IUiAdapter() {}
};