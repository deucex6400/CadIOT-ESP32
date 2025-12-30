# CadIOT_MultiDevice_v6 (Relay build)

**Targets**: Headless, SSD1306, M5CoreS3, TFT_eSPI (select via build flag)

**Features**
- Relay control via **Azure IoT Hub** using MQTT over TLS **8883**.
- **Direct Methods**: `$iothub/methods/POST/activateRelay/?$rid=...` → relay ON; `relayOff` → relay OFF.
- **C2D** messages: subscribes to `devices/{deviceId}/messages/devicebound/#` and acts on `{'cmd':'activateRelay'}` / `{'cmd':'relayOff'}`.
- **No temperature telemetry** (removed). Command/control only.
- **Serial logging** with target label and timestamps.
- TLS trust anchors embedded in `secrets.h`: **DigiCert Global Root G2** + **Microsoft RSA Root CA 2017**.

**Why these roots?** Azure IoT Hub in global Azure chains to **DigiCert Global Root G2**. Microsoft recommends also trusting **Microsoft RSA Root CA 2017** to avoid disruptions if intermediate chains change. Migration to DigiCert G2 completed on **September 30, 2024**. 

**MQTT requirements**: MQTT v3.1.1 on **port 8883** with fixed topics for methods and devicebound. If 8883 is blocked, switch to **MQTT over WebSockets (443)**.

## Configure
Edit `secrets.h`:
```cpp
static const char* WIFI_SSID = "<YOUR-SSID>";
static const char* WIFI_PASS = "<YOUR-PASS>";
static const char* IOTHUB_HOST = "BGVfd-IOT-Hub.azure-devices.net";
static const char* DEVICE_ID   = "Relay-1";
static const char* BASE64_DEVICE_KEY = "<PASTE-BASE64-DEVICE-KEY>";
```

## Build (PlatformIO)
```bash
pio run -t upload -e headless
pio run -t upload -e ssd1306
pio run -t upload -e m5cores3
pio run -t upload -e tftespi
```

## Files
- `main_all_targets.ino` — Target selection, relay logic, Direct Methods/C2D handlers, Serial logging.
- `secrets.h` — CA bundle + Wi‑Fi/Hub/Device config.
- `azure_AzIoTSasToken.h/.cpp` — SAS token helper (60‑min token; auto renew in reconnect path).
- `azure_sdk_compat.h` — legacy `char*/size` wrappers.
- `ui_*` — minimal UI adapters for each target.
- `platformio.ini`, `README.md`.
