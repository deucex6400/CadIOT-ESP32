
// CadIOT_MultiDevice_v6 (Relay build; target selection; no temperature telemetry)
// Define ONE of: -D TARGET_HEADLESS, -D TARGET_SSD1306, -D TARGET_M5CORES3, -D TARGET_TFT_ESPI
#define TARGET_TFT_ESPI

#if defined(TARGET_HEADLESS)
#define TARGET_NAME "HEADLESS"
#include "ui_HeadlessUi.h"
#ifndef RELAY_PIN
#define RELAY_PIN 18
#endif

#elif defined(TARGET_SSD1306)
#define TARGET_NAME "SSD1306"
#include "ui_SSD1306Ui.h"
#ifndef RELAY_PIN
#define RELAY_PIN 18
#endif

#elif defined(TARGET_M5CORES3)
#define TARGET_NAME "M5CORES3"
#include "ui_M5CoreS3Ui.h"
#ifndef RELAY_PIN
#define RELAY_PIN 18
#endif

#elif defined(TARGET_TFT_ESPI)
#define TARGET_NAME "TFT_eSPI"
#include "ui_TFT_eSPI.h"
#ifndef RELAY_PIN
#define RELAY_PIN 3
#endif

#else
#error "Define one of: TARGET_HEADLESS, TARGET_SSD1306, TARGET_M5CORES3, TARGET_TFT_ESPI"
#endif

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <az_iot_hub_client.h>
#include "azure_AzIoTSasToken.h"
#include "azure_sdk_compat.h"
#include "ui_IUiAdapter.h"
#include "secrets.h"

#define ENABLE_SERIAL_LOG 1
#if ENABLE_SERIAL_LOG
#define LOG(fmt, ...) \
  do { \
    static char __b[256]; \
    snprintf(__b, sizeof(__b), "[%lu][" TARGET_NAME "] " fmt, (unsigned long)millis(), ##__VA_ARGS__); \
    Serial.println(__b); \
  } while (0)
#else
#define LOG(fmt, ...) do { } while (0)
#endif

WiFiClientSecure net;
PubSubClient mqtt(net);
az_iot_hub_client hubClient;
az_span host     = az_span_create((uint8_t *)IOTHUB_HOST, strlen(IOTHUB_HOST));
az_span deviceId = az_span_create((uint8_t *)DEVICE_ID, strlen(DEVICE_ID));

static uint8_t sigbuf[128];
static uint8_t sasbuf[256];
AzIoTSasToken sas(&hubClient,
  az_span_create((uint8_t *)BASE64_DEVICE_KEY, strlen(BASE64_DEVICE_KEY)),
  az_span_create(sigbuf, sizeof(sigbuf)),
  az_span_create(sasbuf, sizeof(sasbuf)));

#if defined(TARGET_HEADLESS)
HeadlessUi ui;
#elif defined(TARGET_SSD1306)
SSD1306Ui ui;
#elif defined(TARGET_M5CORES3)
M5CoreS3Ui ui;
#elif defined(TARGET_TFT_ESPI)
TftEspiUi ui;
#endif

// --- Relay control helpers ---
static void activateRelay(const char *src)
{
  digitalWrite(RELAY_PIN, HIGH);
  LOG("Relay ON src=%s", src);
  ui.setStatus("Relay ON");
}

static void deactivateRelay(const char *src)
{
  digitalWrite(RELAY_PIN, LOW);
  LOG("Relay OFF src=%s", src);
  ui.setStatus("Relay OFF");
}

// Momentary test (UI button)
static void testRelayMomentary()
{
  activateRelay("ui_test");
  delay(2000); // pulse length; adjust to taste
  deactivateRelay("ui_test");
}

// --- Named handlers for UI function pointers (no lambdas required) ---
static void onUiTestRelay() { testRelayMomentary(); }
static void onUiRelayOff()  { deactivateRelay("ui_button"); }

static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
  String t(topic);
  String p;
  p.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) p += (char)payload[i];

  LOG("MQTT RX topic=%s", t.c_str());
  LOG("MQTT RX payload=%s", p.c_str());

  if (t.startsWith("$iothub/methods/POST/"))
  {
    const String prefix("$iothub/methods/POST/");
    int start = prefix.length();
    int slash = t.indexOf('/', start);
    String method = (slash > start) ? t.substring(start, slash) : "";
    int ridPos = t.indexOf("?$rid=");
    String rid = (ridPos > 0) ? t.substring(ridPos + 6) : "0";
    int status = 404;
    String body = "{'error':'method_not_found'}";

    if (method == "activateRelay") {
      activateRelay("direct_method");
      status = 200; body = "{'status':'relay_on'}";
    } else if (method == "relayOff") {
      deactivateRelay("direct_method");
      status = 200; body = "{'status':'relay_off'}";
    }

    String resp = String("$iothub/methods/res/") + status + "/?$rid=" + rid;
    mqtt.publish(resp.c_str(), body.c_str());
    return;
  }

  if (t.startsWith("devices/") && t.indexOf("/messages/devicebound") > 0)
  {
    if (p.indexOf("{'cmd':'activateRelay'}") >= 0) {
      activateRelay("c2d");
    } else if (p.indexOf("{'cmd':'relayOff'}") >= 0) {
      deactivateRelay("c2d");
    } else {
      LOG("C2D payload unrecognized");
    }
  }
}

static void connectWiFi()
{
  ui.logInfo("Connecting WiFi...");
  Serial.begin(115200);
  delay(50);
  LOG("WiFi SSID='%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(250); }
  ui.setStatus("WiFi connected");
  LOG("WiFi IP=%s RSSI=%d", WiFi.localIP().toString().c_str(), WiFi.RSSI());

  ui.logInfo("WiFi connected");
  ui.setStatus("WiFi connected");
  {
    char buf[96];
    snprintf(buf, sizeof(buf), "SSID=%s IP=%s RSSI=%d",
             WIFI_SSID, WiFi.localIP().toString().c_str(), WiFi.RSSI());
    ui.showTelemetry(buf);
  }
}

static void setupTime()
{
  ui.logInfo("Syncing NTP time...");
  LOG("NTP sync start");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(NULL);
  while (now < 1609459200) { delay(250); now = time(NULL); }
  ui.setStatus("Time synced");
  LOG("NTP synced epoch=%lu", (unsigned long)now);
}

static bool buildMqttCredentials()
{
  LOG("Init IoT Hub client");
  if (az_result_failed(az_iot_hub_client_init(&hubClient, host, deviceId, NULL)))
  {
    ui.logError("hub_client_init failed");
    LOG("ERROR: hub_client_init failed");
    return false;
  }

  char username[256]; size_t ulen = 0;
  if (az_result_failed(azure_compat::get_user_name(&hubClient, username, sizeof(username), &ulen)))
  {
    ui.logError("get_user_name failed");
    LOG("ERROR: get_user_name failed");
    return false;
  }
  String mqtt_user(username, ulen);
  LOG("MQTT username len=%u", (unsigned)ulen);

  char client_id[128]; size_t clen = 0;
  if (az_result_failed(azure_compat::get_client_id(&hubClient, client_id, sizeof(client_id), &clen)))
  {
    ui.logError("get_client_id failed");
    LOG("ERROR: get_client_id failed");
    return false;
  }
  String mqtt_cid(client_id, clen);
  LOG("MQTT clientId='%s'", mqtt_cid.c_str());

  LOG("Generating SAS (60m)");
  if (az_result_failed(sas.Generate(60)))
  {
    ui.logError("SAS generate failed");
    LOG("ERROR: SAS generate failed");
    return false;
  }
  String mqtt_pass((char *)az_span_ptr(sas.Get()), az_span_size(sas.Get()));
  LOG("SAS size=%u", (unsigned)mqtt_pass.length());

  net.setCACert(CA_BUNDLE_PEM);
  LOG("TLS CA bundle loaded");

  mqtt.setServer(IOTHUB_HOST, 8883);
  mqtt.setKeepAlive(120);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(onMqttMessage);

  ui.setStatus("Connecting MQTT...");
  LOG("MQTT connect host=%s", IOTHUB_HOST);
  if (!mqtt.connect(mqtt_cid.c_str(), mqtt_user.c_str(), mqtt_pass.c_str()))
  {
    ui.logError("MQTT connect failed");
    LOG("ERROR: MQTT connect failed; state=%d", mqtt.state());
    return false;
  }
  ui.setStatus("MQTT connected");
  LOG("MQTT connected");

  mqtt.subscribe("$iothub/methods/POST/#");
  String c2d = String("devices/") + DEVICE_ID + "/messages/devicebound/#";
  mqtt.subscribe(c2d.c_str());
  LOG("Subscribed methods + %s", c2d.c_str());

  ui.logInfo("MQTT connected");
  {
    char buf[96];
    snprintf(buf, sizeof(buf), "Host=%s KeepAlive=%d", IOTHUB_HOST, 120);
    ui.showTelemetry(buf);
  }
  return true;
}

static bool ensureConnected()
{
  if (mqtt.connected() && !sas.IsExpiringSoon(300)) return true;

  if (sas.IsExpiringSoon(300) || sas.IsExpired())
  {
    ui.logInfo("Renewing SAS...");
    LOG("SAS renewing");
    if (az_result_failed(sas.Generate(60)))
    {
      ui.logError("SAS renew failed");
      LOG("ERROR: SAS renew failed");
      return false;
    }
  }

  LOG("Reconnect");
  {
    char buf[96];
    snprintf(buf, sizeof(buf), "SAS OK (60m)");
    ui.showTelemetry(buf);
  }
  return buildMqttCredentials();
}

void setup()
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.begin(115200);
  delay(50);
  LOG("Boot");

  ui.begin();
  LOG("UI begin");

  // --- Assign function pointers (NO brace-init; NO lambdas required) ---
#if defined(TARGET_TFT_ESPI)
  ui.onTestRelay = onUiTestRelay;
  ui.onRelayOff  = onUiRelayOff;
#endif

  connectWiFi();
  setupTime();
  buildMqttCredentials();
}

void loop()
{
  ensureConnected();
  mqtt.loop();

#if defined(TARGET_M5CORES3)
  M5.update(); // ensure touch/display services run on CoreS3
#endif

#if defined(TARGET_TFT_ESPI)
  ui.loop();   // poll touch & handle button presses
#endif

  delay(10);
}