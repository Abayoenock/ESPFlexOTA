/**
 * BasicOTA.ino — ESPFlexOTA Example 1
 *
 * Demonstrates the simplest possible usage:
 *   • Local /update web page (no auth)
 *   • Progress + error events printed to Serial
 *
 * Hardware: ESP32 or ESP8266
 *
 * Steps:
 *   1. Set WIFI_SSID and WIFI_PASS below.
 *   2. Flash this sketch via USB.
 *   3. Open Serial Monitor (115200 baud).
 *   4. Note the IP address printed (e.g. 192.168.1.42).
 *   5. Open http://192.168.1.42/update in a browser.
 *   6. Select a compiled .bin file and click "Upload & Install".
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

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== ESPFlexOTA – Basic Example ==="));

    // Connect to WiFi
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    Serial.print(F("Connected! IP: "));
    Serial.println(WiFi.localIP());

    // Build OTA configuration
    OTAConfig config;
    config.currentVersion = "1.0.0";   // your firmware version
    config.webServerPort  = 80;
    config.authEnabled    = false;      // no password — open on local network
    config.autoReboot     = true;       // reboot automatically after flashing
    config.verboseLogging = true;       // print progress to Serial

    // Register callbacks BEFORE calling begin()
    OTA.onStart([](OTASource src) {
        Serial.println(F("[App] OTA update started!"));
    });

    OTA.onProgress([](int percent, size_t written, size_t total) {
        // Simple progress bar on Serial
        static int lastPct = -1;
        if (percent != lastPct) {
            lastPct = percent;
            Serial.printf("[App] Progress: [");
            int filled = percent / 5;
            for (int i = 0; i < 20; i++) Serial.print(i < filled ? '#' : '-');
            Serial.printf("] %d%%\n", percent);
        }
    });

    OTA.onComplete([](OTASource src) {
        Serial.println(F("[App] OTA complete! Rebooting..."));
    });

    OTA.onError([](const OTAError& err) {
        Serial.print(F("[App] OTA ERROR: "));
        Serial.println(err.toString());
    });

    // Start OTA
    if (!OTA.begin(config)) {
        Serial.print(F("OTA init failed: "));
        Serial.println(OTA.getLastError().toString());
        return;
    }

    Serial.printf("OTA ready! Open: http://%s/update\n",
                  WiFi.localIP().toString().c_str());
}

void loop() {
    OTA.loop();   // <-- must be called every loop iteration

    // Your application code here
    delay(10);
}
