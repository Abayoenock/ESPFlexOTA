/**
 * ESPFlexOTA.cpp
 * Implementation — every method signature matches ESPFlexOTA.h exactly.
 */

#include "ESPFlexOTA.h"
#include "OTAWebPages.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

ESPFlexOTAClass::ESPFlexOTAClass() {}

ESPFlexOTAClass::~ESPFlexOTAClass() {
    end();
}

// ─────────────────────────────────────────────────────────────────────────────
// begin()
// ─────────────────────────────────────────────────────────────────────────────

bool ESPFlexOTAClass::begin(const OTAConfig& config) {
    if (_initialised) end();

    _config = config;
    _lastError.clear();

    if (_config.currentVersion.isEmpty()) {
        _emitError(OTAErrorCode::INVALID_CONFIG, "currentVersion must not be empty");
        return false;
    }

    if (!_checkWiFi()) {
        _emitError(OTAErrorCode::WIFI_NOT_CONNECTED,
                   "WiFi is not connected. Call begin() after WiFi is up.");
        return false;
    }

    // Power-loss recovery check
    if (_wasInterrupted()) {
        if (_config.verboseLogging)
            Serial.println(F("[ESPFlexOTA] WARNING: previous update was interrupted."));
        _emitError(OTAErrorCode::INTERRUPTED_UPDATE,
                   "Previous update was interrupted (possible power loss).");
        _clearInterruptedFlag();
    }

#if defined(ESP32)
    if (_config.rollbackEnabled)
        esp_ota_mark_app_valid_cancel_rollback();
    _server = new WebServer(_config.webServerPort);
#elif defined(ESP8266)
    _server = new ESP8266WebServer(_config.webServerPort);
#endif

    _setupWebServer();
    _initialised = true;

    if (_config.verboseLogging) {
        Serial.printf("[ESPFlexOTA] v%s ready on %s  update page: %s\n",
                      ESPFLEXOTA_VERSION,
                      OTA_PLATFORM,
                      getUpdateURL().c_str());
        if (_config.authEnabled)
            Serial.println(F("[ESPFlexOTA] Auth: ENABLED"));
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────

void ESPFlexOTAClass::loop() {
    if (!_initialised || _server == nullptr) return;
    _server->handleClient();

    if (_config.autoCheckInterval > 0 && !_config.versionCheckURL.isEmpty()) {
        static unsigned long _lastAutoCheck = 0;
        unsigned long now = millis();
        if (now - _lastAutoCheck >= (_config.autoCheckInterval * 1000UL)) {
            _lastAutoCheck = now;
            checkForUpdates();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// end()
// ─────────────────────────────────────────────────────────────────────────────

void ESPFlexOTAClass::end() {
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    _initialised  = false;
    _isUpdating   = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Status helpers
// ─────────────────────────────────────────────────────────────────────────────

String ESPFlexOTAClass::getServerIP() const {
    return WiFi.localIP().toString();
}

String ESPFlexOTAClass::getUpdateURL() const {
    return "http://" + getServerIP() + ":" +
           String(_config.webServerPort) + "/update";
}

// ─────────────────────────────────────────────────────────────────────────────
// Web server setup & handlers
// ─────────────────────────────────────────────────────────────────────────────

void ESPFlexOTAClass::_setupWebServer() {
    _server->on("/update", HTTP_GET, [this]() {
        _handleUpdateGet();
    });

    _server->on("/update", HTTP_POST,
        [this]() {
            // Response sent after upload completes
            if (Update.hasError()) {
                _server->send(200, "text/html",
                              _buildErrorPage(Update.errorString()));
            } else {
                _server->send(200, "text/html", _buildSuccessPage());
                if (_config.autoReboot) {
                    delay(1500);
                    ESP.restart();
                }
            }
        },
        [this]() {
            _handleUpload();
        }
    );

    _server->onNotFound([this]() {
        _server->send(404, "text/plain",
                      "Not found. OTA update page is at /update");
    });

    _server->begin();
}

void ESPFlexOTAClass::_handleUpdateGet() {
    if (!_checkAuth()) return;
    _server->send(200, "text/html", _buildUpdatePage());
}

void ESPFlexOTAClass::_handleUpload() {
    if (!_checkAuth()) return;

    HTTPUpload& upload = _server->upload();

    if (upload.status == UPLOAD_FILE_START) {
        if (_isUpdating) {
            _emitError(OTAErrorCode::ALREADY_UPDATING, "Upload already in progress");
            return;
        }
        _isUpdating = true;
        _lastError.clear();
        _emitStart(OTASource::LOCAL_WEB);
        _persistUpdateState(true);

        if (_config.verboseLogging)
            Serial.printf("[ESPFlexOTA] Upload started: %s\n",
                          upload.filename.c_str());

        size_t freeSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (freeSpace < _config.minFreeSpaceBytes) {
            _emitError(OTAErrorCode::INSUFFICIENT_SPACE,
                       "Not enough free flash space");
            _isUpdating = false;
            return;
        }

        if (!Update.begin(freeSpace)) {
            _emitError(OTAErrorCode::UPDATE_BEGIN_FAILED,
                       "Update.begin() failed: " + String(Update.errorString()));
            _isUpdating = false;
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!_isUpdating) return;

        if (!_checkWiFi()) {
            Update.abort();
            _emitError(OTAErrorCode::WIFI_LOST_DURING_UPDATE,
                       "WiFi lost during upload");
            _isUpdating = false;
            return;
        }

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.abort();
            _emitError(OTAErrorCode::FLASH_WRITE_FAILED,
                       "Flash write error: " + String(Update.errorString()));
            _isUpdating = false;
            return;
        }

        if (upload.totalSize > 0) {
            int pct = (int)((upload.currentSize * 100UL) / upload.totalSize);
            _emitProgress(pct, upload.currentSize, upload.totalSize);
        }
        _feedWatchdog();

    } else if (upload.status == UPLOAD_FILE_END) {
        if (!_isUpdating) return;

        if (!Update.end(true)) {
            _emitError(OTAErrorCode::UPDATE_END_FAILED,
                       "Update.end() failed: " + String(Update.errorString()));
            _isUpdating = false;
            _persistUpdateState(false);
            return;
        }

        _emitProgress(100, upload.totalSize, upload.totalSize);
        _emitComplete(OTASource::LOCAL_WEB);
        _persistUpdateState(false);
        _isUpdating = false;

        if (_config.verboseLogging)
            Serial.println(F("[ESPFlexOTA] Upload complete."));

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        _emitError(OTAErrorCode::FLASH_WRITE_FAILED, "Upload aborted by client");
        _persistUpdateState(false);
        _isUpdating = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// checkForUpdates()
// ─────────────────────────────────────────────────────────────────────────────

bool ESPFlexOTAClass::checkForUpdates() {
    if (!_initialised) {
        _emitError(OTAErrorCode::NOT_INITIALISED,
                   OTAError::defaultMessage(OTAErrorCode::NOT_INITIALISED));
        return false;
    }
    if (_config.versionCheckURL.isEmpty()) {
        _emitError(OTAErrorCode::VERSION_URL_NOT_SET,
                   OTAError::defaultMessage(OTAErrorCode::VERSION_URL_NOT_SET));
        return false;
    }
    if (!_checkWiFi()) {
        _emitError(OTAErrorCode::WIFI_NOT_CONNECTED,
                   OTAError::defaultMessage(OTAErrorCode::WIFI_NOT_CONNECTED));
        return false;
    }

    if (_config.verboseLogging)
        Serial.printf("[ESPFlexOTA] Checking for updates at: %s\n",
                      _config.versionCheckURL.c_str());

    String body = _httpGet(_config.versionCheckURL, (int)_config.httpTimeoutMs);
    if (body.isEmpty()) return false;  // error emitted by _httpGet

    if (!_parseVersionResponse(body)) return false;

    int cmp = _compareVersions(_latestVersion, _config.currentVersion);
    _updateAvailable = (cmp > 0);

    if (_config.verboseLogging)
        Serial.printf("[ESPFlexOTA] Current: %s  Latest: %s  → %s\n",
                      _config.currentVersion.c_str(),
                      _latestVersion.c_str(),
                      _updateAvailable ? "UPDATE AVAILABLE" : "up-to-date");

    if (!_updateAvailable) {
        _emitError(OTAErrorCode::NO_UPDATE_AVAILABLE,
                   "Already on latest firmware (" + _config.currentVersion + ")");
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// downloadAndInstall()
// ─────────────────────────────────────────────────────────────────────────────

bool ESPFlexOTAClass::downloadAndInstall() {
    if (!_initialised) {
        _emitError(OTAErrorCode::NOT_INITIALISED,
                   OTAError::defaultMessage(OTAErrorCode::NOT_INITIALISED));
        return false;
    }
    if (_isUpdating) {
        _emitError(OTAErrorCode::ALREADY_UPDATING,
                   OTAError::defaultMessage(OTAErrorCode::ALREADY_UPDATING));
        return false;
    }

    if (!_updateAvailable) {
        if (!checkForUpdates()) return false;
    }

    String url = _firmwareDownloadURL.isEmpty()
                 ? _config.firmwareDownloadURL
                 : _firmwareDownloadURL;

    if (url.isEmpty()) {
        _emitError(OTAErrorCode::DOWNLOAD_FAILED,
                   "No firmware download URL configured");
        return false;
    }

    if (_config.verboseLogging)
        Serial.printf("[ESPFlexOTA] Downloading from: %s\n", url.c_str());

    bool success = false;
    for (uint8_t attempt = 1; attempt <= _config.maxRetries; attempt++) {
        if (attempt > 1) {
            if (_config.verboseLogging)
                Serial.printf("[ESPFlexOTA] Retry %d/%d in %u ms...\n",
                              attempt, _config.maxRetries, _config.retryDelayMs);
            delay(_config.retryDelayMs);
        }
        success = _performRemoteDownload(url);
        if (success) break;
    }
    return success;
}

// ─────────────────────────────────────────────────────────────────────────────
// _performRemoteDownload()
// ─────────────────────────────────────────────────────────────────────────────

bool ESPFlexOTAClass::_performRemoteDownload(const String& url) {
    if (!_checkWiFi()) {
        _emitError(OTAErrorCode::WIFI_NOT_CONNECTED,
                   OTAError::defaultMessage(OTAErrorCode::WIFI_NOT_CONNECTED));
        return false;
    }

    _isUpdating = true;
    _lastError.clear();
    _emitStart(OTASource::REMOTE_URL);
    _persistUpdateState(true);

#if defined(ESP32)
    WiFiClient client;
    HTTPClient http;
    http.setTimeout((int)_config.httpTimeoutMs);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        _emitError(OTAErrorCode::HTTP_REQUEST_FAILED,
                   "HTTPClient.begin() failed for: " + url);
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }
#elif defined(ESP8266)
    WiFiClient client;
    HTTPClient http;
    http.setTimeout((int)_config.httpTimeoutMs);
    http.begin(client, url);
#endif

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        _emitError(OTAErrorCode::DOWNLOAD_FAILED,
                   "HTTP GET returned " + String(httpCode), httpCode);
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength == 0) {
        http.end();
        _emitError(OTAErrorCode::CORRUPT_FIRMWARE, "Server returned empty body");
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }

    if (contentLength > 0 && !_validatePartitionSpace((size_t)contentLength)) {
        http.end();
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }

    bool canBegin = (contentLength > 0)
                    ? Update.begin((size_t)contentLength)
                    : Update.begin(UPDATE_SIZE_UNKNOWN);

    if (!canBegin) {
        http.end();
        _emitError(OTAErrorCode::UPDATE_BEGIN_FAILED,
                   "Update.begin() failed: " + String(Update.errorString()));
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t  buf[1024];
    size_t   written  = 0;
    int      lastPct  = -1;

    while (http.connected() &&
           (contentLength < 0 || written < (size_t)contentLength)) {
        size_t avail = stream->available();
        if (avail == 0) { delay(1); continue; }

        size_t toRead = (avail < sizeof(buf)) ? avail : sizeof(buf);
        size_t rd     = stream->readBytes(buf, toRead);
        if (rd == 0) break;

        size_t wr = Update.write(buf, rd);
        if (wr != rd) {
            http.end();
            Update.abort();
            _emitError(OTAErrorCode::FLASH_WRITE_FAILED,
                       "Flash write mismatch (" + String(wr) +
                       " of " + String(rd) + " bytes)");
            _isUpdating = false;
            _persistUpdateState(false);
            return false;
        }
        written += wr;

        // WiFi check every 4 KB
        if ((written % 4096) == 0 && !_checkWiFi()) {
            http.end();
            Update.abort();
            _emitError(OTAErrorCode::WIFI_LOST_DURING_UPDATE,
                       "WiFi lost during download");
            _isUpdating = false;
            _persistUpdateState(false);
            return false;
        }

        if (contentLength > 0) {
            int pct = (int)((written * 100UL) / (size_t)contentLength);
            if (pct != lastPct) {
                _emitProgress(pct, written, (size_t)contentLength);
                lastPct = pct;
            }
        }
        _feedWatchdog();
    }

    http.end();

    if (!Update.end(true)) {
        _emitError(OTAErrorCode::UPDATE_END_FAILED,
                   "Update.end() failed: " + String(Update.errorString()));
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }

    if (Update.hasError()) {
        _emitError(OTAErrorCode::FLASH_VERIFY_FAILED,
                   "Post-flash verify failed: " + String(Update.errorString()));
        _isUpdating = false;
        _persistUpdateState(false);
        return false;
    }

    _emitProgress(100, written, written);
    _emitComplete(OTASource::REMOTE_URL);
    _persistUpdateState(false);
    _updateAvailable = false;
    _isUpdating      = false;

    if (_config.verboseLogging)
        Serial.println(F("[ESPFlexOTA] Remote flash complete."));

    if (_config.autoReboot) {
        delay(500);
        ESP.restart();
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// rollback()
// ─────────────────────────────────────────────────────────────────────────────

bool ESPFlexOTAClass::rollback(const String& reason) {
#if defined(ESP32)
    if (!_config.rollbackEnabled) {
        _emitError(OTAErrorCode::ROLLBACK_FAILED,
                   "Rollback is disabled in configuration");
        return false;
    }

    esp_ota_img_states_t state;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        _emitError(OTAErrorCode::ROLLBACK_FAILED,
                   "Could not read OTA partition state");
        return false;
    }

    _emitRollback(reason);

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_invalid_rollback_and_reboot();
    } else {
        // Find the other OTA partition and boot it
        const esp_partition_t* other = nullptr;
        esp_partition_iterator_t it =
            esp_partition_find(ESP_PARTITION_TYPE_APP,
                               ESP_PARTITION_SUBTYPE_ANY, nullptr);
        while (it) {
            const esp_partition_t* p = esp_partition_get(it);
            if (p != running &&
                p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
                p->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_15) {
                other = p;
                break;
            }
            it = esp_partition_next(it);
        }
        esp_partition_iterator_release(it);

        if (!other) {
            _emitError(OTAErrorCode::ROLLBACK_FAILED,
                       "No alternate OTA partition found");
            return false;
        }
        if (esp_ota_set_boot_partition(other) != ESP_OK) {
            _emitError(OTAErrorCode::ROLLBACK_FAILED,
                       "esp_ota_set_boot_partition() failed");
            return false;
        }
        delay(500);
        ESP.restart();
    }
    return true;

#elif defined(ESP8266)
    _emitError(OTAErrorCode::ROLLBACK_FAILED,
               "Hardware rollback is not supported on ESP8266");
    return false;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

bool ESPFlexOTAClass::_checkWiFi() const {
    return (WiFi.status() == WL_CONNECTED);
}

bool ESPFlexOTAClass::_isAuthRequired() const {
    return _config.authEnabled;
}

bool ESPFlexOTAClass::_checkAuth() {
    if (!_isAuthRequired()) return true;
    if (!_server->authenticate(_config.username.c_str(),
                               _config.password.c_str())) {
        _server->requestAuthentication(DIGEST_AUTH, "OTA Update",
                                       "Authentication required");
        return false;
    }
    return true;
}

void ESPFlexOTAClass::_feedWatchdog() {
    unsigned long now = millis();
    if (now - _lastWatchdogFeed >= OTA_WATCHDOG_FEED_INTERVAL_MS) {
        _lastWatchdogFeed = now;
#if defined(ESP8266)
        ESP.wdtFeed();
#endif
        yield();
    }
}

bool ESPFlexOTAClass::_validatePartitionSpace(size_t requiredBytes) {
    size_t freeSpace = ESP.getFreeSketchSpace();
    if (freeSpace < requiredBytes + _config.minFreeSpaceBytes) {
        _emitError(OTAErrorCode::INSUFFICIENT_SPACE,
                   "Need " + String(requiredBytes) +
                   " bytes, only " + String(freeSpace) + " available");
        return false;
    }
    return true;
}

String ESPFlexOTAClass::_httpGet(const String& url, int timeoutMs) {
#if defined(ESP32)
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        _emitError(OTAErrorCode::HTTP_REQUEST_FAILED,
                   "HTTPClient.begin() failed");
        return "";
    }
#elif defined(ESP8266)
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.begin(client, url);
#endif

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        OTAErrorCode ec = (code <= 0) ? OTAErrorCode::HTTP_TIMEOUT
                                      : OTAErrorCode::HTTP_REQUEST_FAILED;
        _emitError(ec, "HTTP GET returned " + String(code), code);
        return "";
    }

    String body = http.getString();
    http.end();
    body.trim();
    return body;
}

bool ESPFlexOTAClass::_parseVersionResponse(const String& body) {
    if (_config.versionFormat == OTAVersionFormat::PLAIN_TEXT) {
        _latestVersion = body;
        if (_latestVersion.startsWith("v") || _latestVersion.startsWith("V"))
            _latestVersion = _latestVersion.substring(1);
        if (_latestVersion.isEmpty()) {
            _emitError(OTAErrorCode::VERSION_PARSE_FAILED,
                       "Version response was empty");
            return false;
        }
        return true;
    }

    // JSON — lightweight hand-rolled parser (no ArduinoJson dependency)
    auto extractJSON = [](const String& json, const String& key) -> String {
        String search = "\"" + key + "\"";
        int idx = json.indexOf(search);
        if (idx < 0) return "";
        idx = json.indexOf(":", idx + search.length());
        if (idx < 0) return "";
        idx++;
        while (idx < (int)json.length() && isspace((unsigned char)json[idx])) idx++;
        if (json[idx] == '"') {
            idx++;
            int end = json.indexOf('"', idx);
            if (end < 0) return "";
            return json.substring(idx, end);
        } else {
            int end = idx;
            while (end < (int)json.length() &&
                   json[end] != ',' && json[end] != '}' && json[end] != '\n')
                end++;
            String val = json.substring(idx, end);
            val.trim();
            return val;
        }
    };

    String ver = extractJSON(body, "version");
    if (ver.isEmpty()) {
        _emitError(OTAErrorCode::VERSION_PARSE_FAILED,
                   "JSON missing 'version' key");
        return false;
    }
    if (ver.startsWith("v") || ver.startsWith("V")) ver = ver.substring(1);
    _latestVersion = ver;

    String urlField = extractJSON(body, "url");
    if (!urlField.isEmpty()) _firmwareDownloadURL = urlField;

    return true;
}

int ESPFlexOTAClass::_compareVersions(const String& a, const String& b) {
    auto nextPart = [](const String& s, int& pos) -> int {
        int end = s.indexOf('.', pos);
        int val;
        if (end < 0) {
            val = s.substring(pos).toInt();
            pos = s.length();
        } else {
            val = s.substring(pos, end).toInt();
            pos = end + 1;
        }
        return val;
    };

    int pa = 0, pb = 0;
    for (int i = 0; i < 3; i++) {
        int va = (pa < (int)a.length()) ? nextPart(a, pa) : 0;
        int vb = (pb < (int)b.length()) ? nextPart(b, pb) : 0;
        if (va > vb) return  1;
        if (va < vb) return -1;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event emitters
// ─────────────────────────────────────────────────────────────────────────────

void ESPFlexOTAClass::_emitProgress(int pct, size_t written, size_t total) {
    if (_progressCb) _progressCb(pct, written, total);
    if (_config.verboseLogging)
        Serial.printf("[ESPFlexOTA] Progress: %d%%  (%u / %u bytes)\n",
                      pct, (unsigned)written, (unsigned)total);
}

void ESPFlexOTAClass::_emitStart(OTASource src) {
    if (_startCb) _startCb(src);
    if (_config.verboseLogging)
        Serial.printf("[ESPFlexOTA] Update started (source: %s)\n",
                      src == OTASource::LOCAL_WEB ? "LOCAL_WEB" : "REMOTE_URL");
}

void ESPFlexOTAClass::_emitComplete(OTASource src) {
    if (_completeCb) _completeCb(src);
    if (_config.verboseLogging)
        Serial.println(F("[ESPFlexOTA] Update complete!"));
}

void ESPFlexOTAClass::_emitError(OTAErrorCode code, const String& msg, int httpCode) {
    _lastError = OTAError(code, msg, httpCode);
    if (_errorCb) _errorCb(_lastError);
    if (_config.verboseLogging)
        Serial.println("[ESPFlexOTA] " + _lastError.toString());
}

void ESPFlexOTAClass::_emitRollback(const String& reason) {
    if (_rollbackCb) _rollbackCb(reason);
    if (_config.verboseLogging)
        Serial.printf("[ESPFlexOTA] Rollback: %s\n", reason.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Power-loss persistence
// ─────────────────────────────────────────────────────────────────────────────

void ESPFlexOTAClass::_persistUpdateState(bool inProgress) {
#if defined(ESP32)
    _prefs.begin("flexota", false);
    _prefs.putBool("updating", inProgress);
    _prefs.end();
#elif defined(ESP8266)
    uint32_t flag = inProgress ? 0xDEADBEEF : 0x00000000;
    ESP.rtcUserMemoryWrite(0, &flag, sizeof(flag));
#endif
}

bool ESPFlexOTAClass::_wasInterrupted() {
#if defined(ESP32)
    _prefs.begin("flexota", true);
    bool v = _prefs.getBool("updating", false);
    _prefs.end();
    return v;
#elif defined(ESP8266)
    uint32_t flag = 0;
    ESP.rtcUserMemoryRead(0, &flag, sizeof(flag));
    return (flag == 0xDEADBEEF);
#endif
}

void ESPFlexOTAClass::_clearInterruptedFlag() {
    _persistUpdateState(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

ESPFlexOTAClass OTA;
