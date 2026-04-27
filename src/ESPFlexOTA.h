#pragma once
#ifndef ESP_FLEX_OTA_H
#define ESP_FLEX_OTA_H

/**
 * ESPFlexOTA.h — Main header for the ESPFlexOTA library.
 *
 * Include this single header in your sketch:
 *   #include <ESPFlexOTA.h>
 *
 * Include chain (all pulled in automatically):
 *   ESPFlexOTA.h
 *     └── OTATypes.h   ← ALL enums  (OTASource, OTAVersionFormat,
 *           │                         OTAErrorCode, OTAEventType)
 *           ├── OTAConfig.h  ← OTAConfig struct only
 *           ├── OTAError.h   ← OTAError  struct only
 *           └── OTAEvents.h  ← OTAEvent  struct only
 */

#include <Arduino.h>

// ── Platform detection & platform-specific includes ──────────────────────────
#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <HTTPClient.h>
  #include <Update.h>
  #include <esp_ota_ops.h>
  #include <Preferences.h>
  #define OTA_PLATFORM "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPClient.h>
  #include <Updater.h>
  #define OTA_PLATFORM "ESP8266"
#else
  #error "ESPFlexOTA: Unsupported platform. Only ESP32 and ESP8266 are supported."
#endif

// ── Library sub-headers (order matters: Types first) ─────────────────────────
#include "OTATypes.h"   // ← ALL enums; must come before the struct headers
#include "OTAConfig.h"  // ← OTAConfig struct  (includes OTATypes.h)
#include "OTAError.h"   // ← OTAError  struct  (includes OTATypes.h)
#include "OTAEvents.h"  // ← OTAEvent  struct  (includes OTATypes.h + OTAError.h)

// ── Version ──────────────────────────────────────────────────────────────────
#define ESPFLEXOTA_VERSION       "1.0.0"
#define ESPFLEXOTA_VERSION_MAJOR 1
#define ESPFLEXOTA_VERSION_MINOR 0
#define ESPFLEXOTA_VERSION_PATCH 0

// ── Timing constants ─────────────────────────────────────────────────────────
#define OTA_DEFAULT_HTTP_TIMEOUT_MS     15000
#define OTA_DEFAULT_CONNECT_TIMEOUT_MS   5000
#define OTA_DEFAULT_RETRY_COUNT             3
#define OTA_DEFAULT_RETRY_DELAY_MS       2000
#define OTA_WATCHDOG_FEED_INTERVAL_MS    1000
#define OTA_MIN_FREE_SPACE_BYTES        65536

// ── Callback type aliases ─────────────────────────────────────────────────────
using OTAProgressCallback = std::function<void(int percent, size_t bytesWritten, size_t totalBytes)>;
using OTAStartCallback    = std::function<void(OTASource source)>;
using OTACompleteCallback = std::function<void(OTASource source)>;
using OTAErrorCallback    = std::function<void(const OTAError& error)>;
using OTARollbackCallback = std::function<void(const String& reason)>;

// ─────────────────────────────────────────────────────────────────────────────
// ESPFlexOTAClass
// ─────────────────────────────────────────────────────────────────────────────
class ESPFlexOTAClass {
public:
    ESPFlexOTAClass();
    ~ESPFlexOTAClass();

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * Initialise the OTA system. Must be called after WiFi is connected.
     * @param  config  OTAConfig describing all options.
     * @return true on success; false on failure (inspect getLastError()).
     */
    bool begin(const OTAConfig& config);

    /**
     * Pump the internal web-server and background tasks.
     * Call this every loop() iteration.
     */
    void loop();

    /** Stop the OTA web server and free resources. */
    void end();

    // ── Remote update API ─────────────────────────────────────────────────────

    /**
     * Contact versionCheckURL and compare with running firmware version.
     * @return true if a newer version is available.
     */
    bool checkForUpdates();

    /**
     * Download firmware from firmwareDownloadURL and flash it.
     * Calls checkForUpdates() automatically if not already called.
     * @return true on successful flash (reboot scheduled if autoReboot=true).
     */
    bool downloadAndInstall();

    /**
     * Reboot into the previously valid firmware partition (ESP32 only).
     * @param reason  Human-readable reason forwarded to the rollback callback.
     * @return true if rollback was initiated.
     */
    bool rollback(const String& reason = "Manual rollback");

    // ── Event callbacks ───────────────────────────────────────────────────────

    /** Called periodically during flash with % complete, bytes written, total bytes. */
    void onProgress(OTAProgressCallback cb)   { _progressCb = cb; }

    /** Called once when a flash operation starts. */
    void onStart(OTAStartCallback cb)         { _startCb    = cb; }

    /** Called once when a flash operation completes successfully. */
    void onComplete(OTACompleteCallback cb)   { _completeCb = cb; }

    /** Called whenever an error occurs; supplies a structured OTAError. */
    void onError(OTAErrorCallback cb)         { _errorCb    = cb; }

    /** Called when a rollback is triggered. */
    void onRollback(OTARollbackCallback cb)   { _rollbackCb = cb; }

    // ── Status / introspection ────────────────────────────────────────────────

    /** Returns true while a flash operation is in progress. */
    bool isUpdating() const { return _isUpdating; }

    /** Returns the last structured error. */
    const OTAError& getLastError() const { return _lastError; }

    /** Returns the running firmware version string (set via OTAConfig). */
    const String& getCurrentVersion() const { return _config.currentVersion; }

    /** Returns the latest version string from the last checkForUpdates() call. */
    const String& getLatestVersion() const { return _latestVersion; }

    /** Returns the IP address the web server is listening on. */
    String getServerIP() const;

    /** Returns the port the web server is listening on. */
    uint16_t getServerPort() const { return _config.webServerPort; }

    /** Returns the full http://ip:port/update URL. */
    String getUpdateURL() const;

    /** Clear the last error. */
    void clearError() { _lastError.clear(); }

    // ── Compile-time info ─────────────────────────────────────────────────────
    static constexpr const char* version()  { return ESPFLEXOTA_VERSION; }
    static constexpr const char* platform() { return OTA_PLATFORM; }

private:
    // ── Web server ────────────────────────────────────────────────────────────
    void  _setupWebServer();
    void  _handleUpdateGet();
    void  _handleUpload();

    // ── Remote download ───────────────────────────────────────────────────────
    bool  _performRemoteDownload(const String& url);

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool    _validatePartitionSpace(size_t requiredBytes);
    bool    _parseVersionResponse(const String& body);
    int     _compareVersions(const String& a, const String& b);
    String  _httpGet(const String& url, int timeoutMs);
    bool    _isAuthRequired() const;
    bool    _checkAuth();
    bool    _checkWiFi() const;
    void    _feedWatchdog();

    // ── Persistence (power-loss detection) ───────────────────────────────────
    void    _persistUpdateState(bool inProgress);
    bool    _wasInterrupted();
    void    _clearInterruptedFlag();

    // ── Event emitters ────────────────────────────────────────────────────────
    void    _emitProgress(int pct, size_t written, size_t total);
    void    _emitStart(OTASource src);
    void    _emitComplete(OTASource src);
    void    _emitError(OTAErrorCode code, const String& msg, int httpCode = 0);
    void    _emitRollback(const String& reason);

    // ── HTML page builders (implemented in OTAWebPages.h) ────────────────────
    String  _buildUpdatePage();
    String  _buildSuccessPage();
    String  _buildErrorPage(const String& msg);

    // ── Member variables ──────────────────────────────────────────────────────
    OTAConfig _config;
    OTAError  _lastError;

    bool      _isUpdating      = false;
    bool      _updateAvailable = false;
    bool      _initialised     = false;

    String    _latestVersion;
    String    _firmwareDownloadURL;   // runtime override from JSON "url" field

    OTAProgressCallback _progressCb = nullptr;
    OTAStartCallback    _startCb    = nullptr;
    OTACompleteCallback _completeCb = nullptr;
    OTAErrorCallback    _errorCb    = nullptr;
    OTARollbackCallback _rollbackCb = nullptr;

    unsigned long _lastWatchdogFeed = 0;

#if defined(ESP32)
    WebServer*  _server = nullptr;
    Preferences _prefs;
#elif defined(ESP8266)
    ESP8266WebServer* _server = nullptr;
#endif
};

// ── Global singleton ──────────────────────────────────────────────────────────
extern ESPFlexOTAClass OTA;

#endif // ESP_FLEX_OTA_H
