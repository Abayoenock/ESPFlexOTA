/**
 * FullFeatured.ino — ESPFlexOTA Example 3
 *
 * Production-ready template showing:
 *   • Both local /update and remote URL OTA
 *   • Auth-protected update page
 *   • Rollback after failed self-test
 *   • Interrupted-update detection
 *   • Custom error handling by error code
 *   • Version info printed on boot
 *   • Status LED patterns
 *
 * Intended as a starting point you can copy into real projects.
 */

#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include <ESPFlexOTA.h>

// ── Project identity (bump these in each build) ───────────────────────────────
#define FW_VERSION   "2.1.0"
#define DEVICE_NAME  "MySmartSensor"

// ── Credentials ───────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASS     = "YOUR_PASSWORD";
const char* OTA_USER      = "admin";
const char* OTA_PASS      = "changeme!";       // CHANGE before deploying

// ── Remote OTA ────────────────────────────────────────────────────────────────
const char* VER_CHECK_URL = "http://ota.example.com/firmware/version.json";
const char* FW_DL_URL     = "http://ota.example.com/firmware/latest.bin";

// ── Hardware ──────────────────────────────────────────────────────────────────
#define STATUS_LED   2    // built-in LED
#define BOOT_BTN     0    // BOOT / FLASH button

// LED blink patterns (period ms, duty cycle)
#define LED_IDLE_MS       3000   // slow blink = idle
#define LED_OTA_MS         200   // fast blink = OTA in progress
#define LED_ERROR_MS       100   // very fast = error

// ─────────────────────────────────────────────────────────────────────────────
// Application self-test — returns false if hardware is not healthy after OTA
// ─────────────────────────────────────────────────────────────────────────────
bool runSelfTest() {
    // TODO: replace with real sensor / peripheral checks
    Serial.println(F("[SelfTest] Running..."));
    delay(500);
    Serial.println(F("[SelfTest] All checks passed."));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// LED helpers
// ─────────────────────────────────────────────────────────────────────────────
enum LedMode { LED_OFF, LED_SLOW, LED_FAST, LED_VFAST };
static LedMode  ledMode = LED_SLOW;
static bool     ledOn   = false;
static unsigned long lastLed = 0;

void updateLED() {
    unsigned long period;
    switch (ledMode) {
        case LED_OFF:   digitalWrite(STATUS_LED, HIGH); return;
        case LED_SLOW:  period = LED_IDLE_MS;  break;
        case LED_FAST:  period = LED_OTA_MS;   break;
        case LED_VFAST: period = LED_ERROR_MS; break;
        default:        period = LED_IDLE_MS;  break;
    }
    if (millis() - lastLed > period / 2) {
        lastLed = millis();
        ledOn   = !ledOn;
        digitalWrite(STATUS_LED, ledOn ? LOW : HIGH);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(600);

    // Print boot banner
    Serial.println(F("\n╔══════════════════════════════════╗"));
    Serial.printf( "║  %-32s║\n", DEVICE_NAME);
    Serial.printf( "║  FW: %-28s║\n", FW_VERSION);
    Serial.printf( "║  ESPFlexOTA: %-20s║\n", ESPFlexOTAClass::version());
    Serial.printf( "║  Platform: %-22s║\n", ESPFlexOTAClass::platform());
    Serial.println(F("╚══════════════════════════════════╝\n"));

    pinMode(STATUS_LED, OUTPUT);
    pinMode(BOOT_BTN, INPUT_PULLUP);
    digitalWrite(STATUS_LED, HIGH);

    // ── WiFi ───────────────────────────────────────────────────────────────
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - wifiStart > 20000) {
            Serial.println(F("\nWiFi timeout! Rebooting in 5 s..."));
            delay(5000);
            ESP.restart();
        }
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    Serial.printf("WiFi connected. IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());

    // ── OTA config ─────────────────────────────────────────────────────────
    OTAConfig cfg;
    cfg.currentVersion      = FW_VERSION;

    // Local web server
    cfg.webServerPort       = 80;
    cfg.authEnabled         = true;
    cfg.username            = OTA_USER;
    cfg.password            = OTA_PASS;

    // Remote update
    cfg.versionCheckURL     = VER_CHECK_URL;
    cfg.firmwareDownloadURL = FW_DL_URL;
    cfg.versionFormat       = OTAVersionFormat::JSON;
    cfg.autoCheckInterval   = 7200;    // check every 2 hours

    // Resilience
    cfg.maxRetries          = 3;
    cfg.retryDelayMs        = 5000;
    cfg.httpTimeoutMs       = 20000;
    cfg.minFreeSpaceBytes   = 65536;

    // Behaviour
    cfg.autoReboot          = true;
    cfg.rollbackEnabled     = true;
    cfg.verboseLogging      = false;   // disable for production

    // ── Callbacks ──────────────────────────────────────────────────────────

    OTA.onStart([](OTASource src) {
        const char* srcStr = (src == OTASource::LOCAL_WEB) ? "LOCAL" : "REMOTE";
        Serial.printf("[OTA] ▶ Update STARTED (source: %s)\n", srcStr);
        ledMode = LED_FAST;
    });

    OTA.onProgress([](int pct, size_t written, size_t total) {
        // Print every 10%
        static int lastReport = -10;
        if (pct - lastReport >= 10 || pct == 100) {
            lastReport = pct;
            Serial.printf("[OTA] %3d%%  (%u / %u bytes)\n",
                          pct, (unsigned)written, (unsigned)total);
        }
    });

    OTA.onComplete([](OTASource src) {
        Serial.println(F("[OTA] ✓ Flash complete!"));
        ledMode = LED_SLOW;
    });

    OTA.onError([](const OTAError& err) {
        ledMode = LED_VFAST;
        Serial.printf("[OTA] ✗ Error [%u] %s\n",
                      (unsigned)err.code, err.message.c_str());

        // Handle specific error codes
        switch (err.code) {
            case OTAErrorCode::NO_UPDATE_AVAILABLE:
                // Informational — reset LED and carry on
                ledMode = LED_SLOW;
                break;

            case OTAErrorCode::WIFI_NOT_CONNECTED:
                Serial.println(F("[OTA] WiFi lost. Attempting reconnect..."));
                WiFi.reconnect();
                ledMode = LED_SLOW;
                break;

            case OTAErrorCode::INSUFFICIENT_SPACE:
                Serial.println(F("[OTA] Not enough flash space. "
                                 "Reduce sketch size or re-partition."));
                break;

            case OTAErrorCode::CORRUPT_FIRMWARE:
            case OTAErrorCode::FLASH_VERIFY_FAILED:
                Serial.println(F("[OTA] Corrupt firmware. "
                                 "Triggering rollback in 3 s..."));
                delay(3000);
                OTA.rollback("Corrupt firmware detected after download");
                break;

            case OTAErrorCode::INTERRUPTED_UPDATE:
                Serial.println(F("[OTA] Previous update was interrupted "
                                 "(power loss?). Device is stable."));
                ledMode = LED_SLOW;
                break;

            default:
                // Unknown / unhandled — log and continue
                ledMode = LED_SLOW;
                break;
        }
    });

    OTA.onRollback([](const String& reason) {
        Serial.printf("[OTA] ↩ ROLLBACK: %s\n", reason.c_str());
        ledMode = LED_VFAST;
    });

    // ── Start OTA ──────────────────────────────────────────────────────────
    if (!OTA.begin(cfg)) {
        Serial.printf("[OTA] begin() failed: %s\n",
                      OTA.getLastError().message.c_str());
        // Continue without OTA in degraded mode
    } else {
        Serial.printf("[OTA] Ready. Update page: %s\n",
                      OTA.getUpdateURL().c_str());
        Serial.printf("[OTA] Credentials: %s / %s\n", OTA_USER, OTA_PASS);
        ledMode = LED_SLOW;
    }

    // ── Post-OTA self-test ─────────────────────────────────────────────────
    // This runs on every boot.  If it fails after an OTA update, we roll back.
#if defined(ESP32)
    // Check if we just booted from a new OTA image (pending verify)
    esp_ota_img_states_t otaState;
    const esp_partition_t* part = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(part, &otaState) == ESP_OK &&
        otaState == ESP_OTA_IMG_PENDING_VERIFY) {
        Serial.println(F("[Boot] New OTA image — running self-test..."));
        if (!runSelfTest()) {
            Serial.println(F("[Boot] Self-test FAILED — rolling back!"));
            OTA.rollback("Self-test failed on new firmware");
        } else {
            Serial.println(F("[Boot] Self-test passed."));
            // OTA.begin() already called esp_ota_mark_app_valid_cancel_rollback()
        }
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    OTA.loop();
    updateLED();

    // Boot button: single press = check updates, hold 3 s = rollback
    if (digitalRead(BOOT_BTN) == LOW) {
        unsigned long pressStart = millis();
        while (digitalRead(BOOT_BTN) == LOW) {
            delay(10);
            if (millis() - pressStart > 3000) {
                Serial.println(F("[App] Long press — triggering rollback!"));
                OTA.rollback("User-triggered rollback via BOOT button");
                return;
            }
        }
        // Short press
        Serial.println(F("[App] Short press — checking for updates..."));
        if (OTA.checkForUpdates()) {
            Serial.printf("[App] New version: %s  Downloading...\n",
                          OTA.getLatestVersion().c_str());
            OTA.downloadAndInstall();
        }
    }

    // Your application logic here
    delay(10);
}
