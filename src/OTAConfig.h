#pragma once
#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

/**
 * OTAConfig.h — Runtime configuration struct for ESPFlexOTA.
 *
 * Only structs live here.  All enums (OTASource, OTAVersionFormat, etc.)
 * are defined in OTATypes.h which is included below.
 */

#include <Arduino.h>
#include "OTATypes.h"   // OTAVersionFormat

struct OTAConfig {

    // ── Identity ──────────────────────────────────────────────────────────────
    /** Running firmware version string (semver recommended, e.g. "1.2.3"). */
    String currentVersion = "0.0.0";

    // ── Local web-server ──────────────────────────────────────────────────────
    /** TCP port for the local /update web server. Default: 80 */
    uint16_t webServerPort = 80;

    /**
     * Enable HTTP Basic-Auth on the /update endpoint.
     * When false the page is publicly accessible on the local network.
     */
    bool authEnabled = false;

    /** HTTP Basic-Auth username. Default: "admin" */
    String username = "admin";

    /** HTTP Basic-Auth password. Default: "admin" */
    String password = "admin";

    // ── Remote update ─────────────────────────────────────────────────────────
    /**
     * URL that returns the latest available firmware version.
     * Leave empty to disable remote-update checking.
     *
     * Plain-text example: "http://ota.example.com/firmware/version.txt"
     * JSON example:       "http://ota.example.com/firmware/version.json"
     *   expected JSON:    { "version": "1.3.0" }
     *   optional field:   { "version": "1.3.0", "url": "http://..." }
     */
    String versionCheckURL = "";

    /**
     * URL from which to download the firmware binary (.bin).
     * May be overridden at runtime if the version-check response
     * includes a "url" key (JSON format only).
     */
    String firmwareDownloadURL = "";

    /** How the version-check URL returns its version. Default: PLAIN_TEXT */
    OTAVersionFormat versionFormat = OTAVersionFormat::PLAIN_TEXT;

    /**
     * Interval in seconds between automatic version checks.
     * Set to 0 to disable (manual via checkForUpdates()).
     * Default: 0 (disabled)
     */
    uint32_t autoCheckInterval = 0;

    // ── Behaviour ─────────────────────────────────────────────────────────────
    /**
     * Automatically reboot after a successful flash.
     * When false the sketch must call ESP.restart() itself.
     * Default: true
     */
    bool autoReboot = true;

    /**
     * Enable partition rollback on ESP32.
     * Has no effect on ESP8266.
     * Default: true
     */
    bool rollbackEnabled = true;

    // ── Resilience ────────────────────────────────────────────────────────────
    /** Number of retries for remote HTTP operations. Default: 3 */
    uint8_t maxRetries = 3;

    /** Delay between retries in milliseconds. Default: 2000 */
    uint32_t retryDelayMs = 2000;

    /** HTTP request / download timeout in milliseconds. Default: 15000 */
    uint32_t httpTimeoutMs = 15000;

    /**
     * Minimum free flash space required before attempting an update.
     * Default: 65536 (64 KB safety margin)
     */
    size_t minFreeSpaceBytes = 65536;

    // ── Developer helpers ─────────────────────────────────────────────────────
    /**
     * Print verbose progress messages to Serial.
     * Useful during development; disable in production.
     * Default: false
     */
    bool verboseLogging = false;
};

#endif // OTA_CONFIG_H
