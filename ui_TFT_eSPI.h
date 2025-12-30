
#pragma once
#include <Arduino.h>
#include "ui_IUiAdapter.h"
#include <TFT_eSPI.h>               // Bodmer's TFT library (uses your User_Setup)
#include <SPI.h>
#include <XPT2046_Touchscreen.h>    // Paul Stoffregen's touch library

class TftEspiUi : public IUiAdapter
{
public:
    // IUiAdapter
    void begin() override;
    void setStatus(const char *) override;     // Relay/WiFi/MQTT status messages
    void showTelemetry(const char *) override; // Latest telemetry string
    void logInfo(const char *) override;
    void logError(const char *) override;
    ~TftEspiUi() {}

    // Poll touch each loop (non-blocking)
    void loop();

    // App-wired callbacks for UI actions (plain function pointers)
    void (*onTestRelay)() = nullptr; // momentary ON then OFF
    void (*onRelayOff)()  = nullptr; // force OFF

private:
    // --- Theme ---
    // Dark BG + white text; blue header; red accent divider; grey panels
    uint16_t bg       = TFT_BLACK;
    uint16_t fg       = TFT_WHITE;
    uint16_t headerBG = TFT_BLUE;     // top bar background color (blue)
    uint16_t accent   = TFT_RED;      // thin divider under header
    uint16_t subtext  = TFT_LIGHTGREY;
    uint16_t panel    = TFT_DARKGREY; // button bar background

    // --- Display ---
    TFT_eSPI tft;
    static constexpr uint8_t ROTATION_LANDSCAPE = 1; // Use 1 or 3 for landscape

    // --- Backlight ---
    static constexpr int TFT_BL_PIN = 21;

    // --- Touch (XPT2046) pins per LCDWiki 2.8" board ---
    static constexpr int TP_IRQ_PIN = 36; // optional IRQ (low when pressed)
    static constexpr int TP_CS_PIN  = 33;
    static constexpr int TP_CLK_PIN = 25;
    static constexpr int TP_MISO_PIN= 39;
    static constexpr int TP_MOSI_PIN= 32;

    // Separate SPI for touch
    SPIClass tsSPI{VSPI};                 // Use VSPI, re-init with pins below
    XPT2046_Touchscreen ts{TP_CS_PIN, TP_IRQ_PIN};

    // Calibration (raw -> screen). (Set from your last working mapping.)
    uint16_t cal_x_min = 300, cal_x_max = 3800;
    uint16_t cal_y_min = 300, cal_y_max = 3800;
    bool swap_xy  = false;   // do not swap axes
    bool mirror_x = false;   // matches your fixed mapping
    bool mirror_y = false;   // matches your fixed mapping

    // Layout (landscape)
    // Header row
    int yHeader   = 0;

    // Info row
    int yInfo     = 30;

    // Combined status row: WiFi (left) + MQTT (right)
    int yStatus   = 56;      // baseline Y for the split row
    int colPad    = 8;       // padding from edges
    int colGap    = 8;       // gap between two columns
    int leftColX  = 8;       // left column start (label + badge)
    int rightColX = 0;       // computed in begin()

    // Relay line
    int yRelay    = 84;

    // Telemetry block (under WiFi+MQTT row)
    int yTelHdr   = 110;
    int yTel      = 130;

    // Divider above buttons
    int yFooter   = 194;

    // Data lines
    String wifiLine, mqttLine, relayLine, telLine, statusLine;

    // Drawing helpers
    void header(const char* title);
    void drawFooterDivider();
    void ensureBacklightOn();

    // Rows (with optional badge)
    void drawLine(int y, const char* label, const String& value, uint16_t textColor=TFT_WHITE);
    void drawLineWithBadge(int y, const char* label, const String& value, uint16_t textColor);
    uint16_t badgeColorFor(const char* label, const String& value);

    // Split row (WiFi left, MQTT right)
    void drawSplitStatusRow();

    // Spinner & icons
    void drawSpinnerFrame(int x, int y);             // draws the current spinner glyph
    void drawRelayCheckOrX(int x, int y, bool on);   // draws ✔ when ON, × when OFF
    bool mqttIsConnecting() const;                   // helper to detect connecting state
    bool relayIsOn() const;                          // helper to detect relay ON/OFF

    // Spinner state
    uint8_t  spinnerIndex = 0;                       // 0..3 for "|/-\"
    uint32_t lastSpinnerMs = 0;
    static constexpr uint32_t SPINNER_INTERVAL_MS = 200;

    // Touch helpers
    bool readTouch(uint16_t& sx, uint16_t& sy);

    // --- Buttons area (bottom bar) ---
    int barY      = 200;  // computed in begin()
    int barH      = 56;   // bigger bar for glove-friendly taps
    int btnPad    = 12;
    int btnW;             // computed in begin()
    int btnH;             // computed in begin()
    int btn1X, btn1Y;     // "Test Relay"
    int btn2X, btn2Y;     // "Relay OFF"
    void drawButtons();
    bool inRect(uint16_t x, uint16_t y, int rx, int ry, int rw, int rh);

    // Touch debounce
    uint32_t lastTouchMs = 0;
    static constexpr uint32_t TOUCH_DEBOUNCE_MS = 250;

    // Status parsing helpers
    void updateLinesFromStatus(const char* s);
    void updateLinesFromInfo(const char* m);
};