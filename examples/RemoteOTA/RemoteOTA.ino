/**
 * RemoteOTA.ino — ESPFlexOTA Example 2
 *
 * Demonstrates remote OTA:
 *   • Version-check URL (plain-text or JSON)
 *   • Firmware download URL
 *   • Automatic periodic checking
 *   • Manual check via button on GPIO0 (BOOT button on most dev boards)
 *   • Full event callbacks
 *
 * Server-side setup (any web/file host, e.g. GitHub Releases, S3, your own server):
 *
 *   Plain-text version endpoint (version.txt):
 *       1.2.0
 *
 *   — OR —
 *
 *   JSON version endpoint (version.json):
 *       {
 *         "version": "1.2.0",
 *         "url": "http://your-server.com/firmware/firmware_1.2.0.bin"
 *       }
 *
 * Hardware: ESP32 or ESP8266
 */

#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include <ESPFlexOTA.h>

// ── Configuration ─────────────────────────────────────────────────────────────
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// ── Remote OTA server ─────────────────────────────────────────────────────────
// Option A — plain text version file
const char* VERSION_CHECK_URL  = "http://your-server.com/firmware/version.txt";
const char* FIRMWARE_DL_URL    = "http://your-server.com/firmware/firmware.bin";

// Option B — JSON (comment out Option A and uncomment these):
// const char* VERSION_CHECK_URL  = "http://your-server.com/firmware/version.json";
// const char* FIRMWARE_DL_URL    = "";  // URL comes from the JSON "url" field

// ── Hardware ──────────────────────────────────────────────────────────────────
#define BOOT_BUTTON_PIN 0   // GPIO0 — BOOT on most ESP32/ESP8266 boards
#define STATUS_LED_PIN  2   // built-in LED (active LOW on most boards)

// ─────────────────────────────────────────────────────────────────────────────
static bool ledState = false;
static unsigned long lastBlink = 0;

void blinkLED() {
    if (millis() - lastBlink > 200) {
        lastBlink = millis();
        ledState  = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState ? LOW : HIGH);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== ESPFlexOTA – Remote OTA Example ==="));

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);  // off
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // WiFi
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print('.');
    }
    Serial.println();
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());

    // OTA config
    OTAConfig config;
    config.currentVersion      = "1.0.0";             // bump this in new builds
    config.webServerPort       = 80;
    config.authEnabled         = true;                 // protect the /update page
    config.username            = "admin";
    config.password            = "admin";

    // Remote update settings
    config.versionCheckURL     = VERSION_CHECK_URL;
    config.firmwareDownloadURL = FIRMWARE_DL_URL;
    config.versionFormat       = OTAVersionFormat::PLAIN_TEXT;  // or JSON
    config.autoCheckInterval   = 3600;                // check every hour (0 = off)

    // Resilience
    config.maxRetries          = 3;
    config.retryDelayMs        = 3000;
    config.httpTimeoutMs       = 20000;
    config.autoReboot          = true;
    config.rollbackEnabled     = true;

    config.verboseLogging      = true;

    // ── Callbacks ──────────────────────────────────────────────────────────
    OTA.onStart([](OTASource src) {
        const char* s = (src == OTASource::LOCAL_WEB) ? "Local Web" : "Remote URL";
        Serial.printf("[App] ★ Update starting (source: %s)\n", s);
        digitalWrite(STATUS_LED_PIN, LOW);  // LED on solid during flash
    });

    OTA.onProgress([](int percent, size_t written, size_t total) {
        Serial.printf("[App] ▶ %3d%%  %u / %u bytes\n",
                      percent, (unsigned)written, (unsigned)total);
    });

    OTA.onComplete([](OTASource src) {
        Serial.println(F("[App] ✓ Update complete! Rebooting in 1 s..."));
        digitalWrite(STATUS_LED_PIN, HIGH);
    });

    OTA.onError([](const OTAError& err) {
        // Structured error — log code, message, and optional HTTP code
        Serial.printf("[App] ✗ OTA Error [%u]: %s",
                      (unsigned)err.code, err.message.c_str());
        if (err.httpCode != 0) {
            Serial.printf(" (HTTP %d)", err.httpCode);
        }
        Serial.println();

        // Non-fatal errors (e.g. NO_UPDATE_AVAILABLE) — just log
        // Fatal errors — could trigger rollback or alert
        if (err.code == OTAErrorCode::CORRUPT_FIRMWARE ||
            err.code == OTAErrorCode::FLASH_VERIFY_FAILED) {
            Serial.println(F("[App] Fatal flash error — consider rollback."));
            // OTA.rollback("Corrupt firmware detected");
        }
    });

    OTA.onRollback([](const String& reason) {
        Serial.printf("[App] ↩ Rollback triggered: %s\n", reason.c_str());
    });

    // ── Start OTA ──────────────────────────────────────────────────────────
    if (!OTA.begin(config)) {
        Serial.printf("OTA begin() failed: %s\n", OTA.getLastError().message);
        return;
    }

    Serial.printf("[App] OTA ready! Update page: %s\n",
                  OTA.getUpdateURL().c_str());
    Serial.printf("[App] Version check every %u seconds.\n",
                  config.autoCheckInterval);
    Serial.println(F("[App] Press BOOT button for manual version check."));
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    OTA.loop();

    // Blink LED gently while idle to show device is alive
    if (!OTA.isUpdating()) {
        static unsigned long lastIdleBlink = 0;
        static bool idleLed = false;
        if (millis() - lastIdleBlink > 2000) {
            lastIdleBlink = millis();
            idleLed = !idleLed;
            digitalWrite(STATUS_LED_PIN, idleLed ? LOW : HIGH);
        }
    }

    // Manual check: press BOOT button
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        delay(50);  // debounce
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            Serial.println(F("[App] Button pressed — checking for updates..."));

            if (OTA.checkForUpdates()) {
                Serial.printf("[App] New version available: %s (current: %s)\n",
                              OTA.getLatestVersion().c_str(),
                              OTA.getCurrentVersion().c_str());
                Serial.println(F("[App] Downloading and installing..."));
                OTA.downloadAndInstall();
            } else {
                Serial.println(F("[App] No update available."));
            }

            // Wait for button release
            while (digitalRead(BOOT_BUTTON_PIN) == LOW) delay(10);
        }
    }

    delay(10);
}
