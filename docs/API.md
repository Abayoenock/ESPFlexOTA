# ESPFlexOTA — Documentation

> Robust, developer-friendly OTA update library for **ESP32** and **ESP8266**.

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [API Reference](#api-reference)
   - [OTAConfig](#otaconfig)
   - [Lifecycle](#lifecycle)
   - [Remote Update](#remote-update)
   - [Rollback](#rollback)
   - [Callbacks](#callbacks)
   - [Status & Introspection](#status--introspection)
5. [Error Reference](#error-reference)
6. [Event Callbacks](#event-callbacks)
7. [Remote Server Setup](#remote-server-setup)
8. [Resilience & Edge Cases](#resilience--edge-cases)
9. [Rollback Deep-Dive](#rollback-deep-dive)
10. [FAQ](#faq)

---

## Overview

ESPFlexOTA provides two update paths that can be used independently or together:

| Path | How it works |
|---|---|
| **Local Web OTA** | Visit `http://<device-ip>/update` in a browser on the same network; drag-and-drop a `.bin` file. |
| **Remote URL OTA** | Device contacts a version-check URL, compares versions, and downloads + flashes firmware automatically. |

Both paths share the same event callbacks, error codes, and resilience features.

---

## Installation

### Arduino Library Manager
1. Open Arduino IDE → *Sketch → Include Library → Manage Libraries…*
2. Search for **ESPFlexOTA** and click *Install*.

### Manual (ZIP)
1. Download the repository as a ZIP file.
2. Arduino IDE → *Sketch → Include Library → Add .ZIP Library…*
3. Select the downloaded ZIP.

### PlatformIO
```ini
; platformio.ini
lib_deps =
    https://github.com/your-org/ESPFlexOTA.git
```

### Partition scheme (ESP32)
For rollback support you **must** use a partition scheme with two OTA slots:

- In Arduino IDE: *Tools → Partition Scheme → **Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)*** or any `ota` scheme.
- In PlatformIO: `board_build.partitions = min_spiffs.csv`

---

## Quick Start

```cpp
#include <WiFi.h>          // or ESP8266WiFi.h
#include <ESPFlexOTA.h>

void setup() {
    Serial.begin(115200);
    WiFi.begin("YOUR_SSID", "YOUR_PASS");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    OTAConfig config;
    config.currentVersion = "1.0.0";

    OTA.onProgress([](int pct, size_t written, size_t total) {
        Serial.printf("Progress: %d%%\n", pct);
    });
    OTA.onError([](const OTAError& err) {
        Serial.println(err.toString());
    });

    OTA.begin(config);
    Serial.println(OTA.getUpdateURL());   // → http://192.168.x.x/update
}

void loop() {
    OTA.loop();   // required
}
```

---

## API Reference

### OTAConfig

All configuration is passed to `OTA.begin()` via an `OTAConfig` struct.

| Field | Type | Default | Description |
|---|---|---|---|
| `currentVersion` | `String` | `"0.0.0"` | **Required.** Running firmware version (semver recommended). |
| `webServerPort` | `uint16_t` | `80` | TCP port for the local `/update` web server. |
| `authEnabled` | `bool` | `false` | Require HTTP Basic-Auth on `/update`. |
| `username` | `String` | `"admin"` | Basic-Auth username. |
| `password` | `String` | `"admin"` | Basic-Auth password. |
| `versionCheckURL` | `String` | `""` | URL returning the latest firmware version. Leave empty to disable remote OTA. |
| `firmwareDownloadURL` | `String` | `""` | URL of the firmware `.bin` to download. |
| `versionFormat` | `OTAVersionFormat` | `PLAIN_TEXT` | `PLAIN_TEXT` or `JSON` (see [Remote Server Setup](#remote-server-setup)). |
| `autoCheckInterval` | `uint32_t` | `0` | Seconds between automatic version checks. `0` = disabled. |
| `autoReboot` | `bool` | `true` | Reboot automatically after a successful flash. |
| `rollbackEnabled` | `bool` | `true` | Enable ESP32 partition rollback (no effect on ESP8266). |
| `maxRetries` | `uint8_t` | `3` | HTTP retry count for remote downloads. |
| `retryDelayMs` | `uint32_t` | `2000` | Milliseconds between retries. |
| `httpTimeoutMs` | `uint32_t` | `15000` | HTTP request/download timeout in ms. |
| `minFreeSpaceBytes` | `size_t` | `65536` | Minimum free flash required before update (safety margin). |
| `verboseLogging` | `bool` | `false` | Print detailed progress to `Serial`. |

---

### Lifecycle

#### `bool OTA.begin(const OTAConfig& config)`

Initialise the OTA system.

- Must be called **after** WiFi is connected.
- Starts the local web server.
- On ESP32, marks the running partition as valid (cancels rollback window).
- On first boot after an interrupted update, emits an `INTERRUPTED_UPDATE` error.

Returns `true` on success. On failure, call `OTA.getLastError()` for details.

```cpp
if (!OTA.begin(config)) {
    Serial.println(OTA.getLastError().toString());
}
```

---

#### `void OTA.loop()`

Pump the web server and auto-check timer. **Must be called from `loop()`.**

```cpp
void loop() {
    OTA.loop();
    // your code...
}
```

---

#### `void OTA.end()`

Stop the web server and free resources. Call before deep sleep or WiFi shutdown.

---

### Remote Update

#### `bool OTA.checkForUpdates()`

Contacts `versionCheckURL`, parses the response, and compares with `currentVersion`.

- Returns `true` if a newer version is available.
- Returns `false` if already up-to-date (error code `NO_UPDATE_AVAILABLE`) or if the check failed.
- Populates `OTA.getLatestVersion()` on success.

```cpp
if (OTA.checkForUpdates()) {
    Serial.println("New version: " + OTA.getLatestVersion());
}
```

---

#### `bool OTA.downloadAndInstall()`

Downloads firmware from `firmwareDownloadURL` (or the `url` field in a JSON version response) and flashes it.

- Automatically calls `checkForUpdates()` if not previously called.
- Retries up to `maxRetries` times on failure.
- Emits `onProgress`, `onError`, and `onComplete` callbacks.
- Calls `ESP.restart()` automatically if `autoReboot = true`.

```cpp
if (OTA.checkForUpdates()) {
    OTA.downloadAndInstall();
}
```

---

### Rollback

#### `bool OTA.rollback(const String& reason = "Manual rollback")`

Reboot into the previously valid firmware partition.

- **ESP32 only.** Requires `rollbackEnabled = true` and a two-OTA-slot partition scheme.
- Triggers `onRollback` callback before rebooting.
- ESP8266 returns `false` with error code `ROLLBACK_FAILED`.

```cpp
OTA.onError([](const OTAError& err) {
    if (err.code == OTAErrorCode::CORRUPT_FIRMWARE) {
        OTA.rollback("Corrupt firmware");
    }
});
```

**Automatic rollback** — if your application fails a self-test after booting new firmware:

```cpp
// In setup(), after OTA.begin():
esp_ota_img_states_t state;
const esp_partition_t* part = esp_ota_get_running_partition();
if (esp_ota_get_state_partition(part, &state) == ESP_OK &&
    state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (!runSelfTest()) {
        OTA.rollback("Self-test failed");
    }
}
```

---

### Callbacks

Register callbacks **before** calling `OTA.begin()`.

#### `OTA.onStart(callback)`

```cpp
OTA.onStart([](OTASource source) {
    // source = OTASource::LOCAL_WEB or OTASource::REMOTE_URL
});
```

#### `OTA.onProgress(callback)`

```cpp
OTA.onProgress([](int percent, size_t bytesWritten, size_t totalBytes) {
    Serial.printf("%d%%  %u/%u bytes\n", percent, bytesWritten, totalBytes);
});
```

#### `OTA.onComplete(callback)`

```cpp
OTA.onComplete([](OTASource source) {
    // Flash succeeded; device will reboot if autoReboot=true
});
```

#### `OTA.onError(callback)`

```cpp
OTA.onError([](const OTAError& err) {
    Serial.printf("Error [%u]: %s\n", (unsigned)err.code, err.message.c_str());
    // err.httpCode  — raw HTTP status (0 if not applicable)
    // err.timestamp — millis() when the error occurred
});
```

#### `OTA.onRollback(callback)`

```cpp
OTA.onRollback([](const String& reason) {
    Serial.println("Rollback: " + reason);
});
```

---

### Status & Introspection

| Method | Returns | Description |
|---|---|---|
| `OTA.isUpdating()` | `bool` | `true` while a flash operation is in progress. |
| `OTA.getLastError()` | `const OTAError&` | Last structured error. |
| `OTA.clearError()` | `void` | Reset the last error. |
| `OTA.getCurrentVersion()` | `const String&` | Version string from `OTAConfig`. |
| `OTA.getLatestVersion()` | `const String&` | Version from last `checkForUpdates()` call. |
| `OTA.getServerIP()` | `String` | IP address the web server is listening on. |
| `OTA.getServerPort()` | `uint16_t` | Port number. |
| `OTA.getUpdateURL()` | `String` | Full `http://ip:port/update` URL. |
| `ESPFlexOTAClass::version()` | `const char*` | Library version string. |
| `ESPFlexOTAClass::platform()` | `const char*` | `"ESP32"` or `"ESP8266"`. |

---

## Error Reference

| Code | Value | Meaning |
|---|---|---|
| `NONE` | 0 | No error. |
| `NOT_INITIALISED` | 100 | `begin()` was not called. |
| `INVALID_CONFIG` | 101 | Config validation failed. |
| `SERVER_START_FAILED` | 102 | Web server could not bind to port. |
| `WIFI_NOT_CONNECTED` | 200 | WiFi link is down. |
| `HTTP_REQUEST_FAILED` | 201 | HTTP GET/POST returned an error. |
| `HTTP_TIMEOUT` | 202 | Request timed out. |
| `DOWNLOAD_FAILED` | 203 | Firmware download failed. |
| `WIFI_LOST_DURING_UPDATE` | 204 | WiFi dropped mid-flash. |
| `VERSION_CHECK_FAILED` | 300 | Could not reach version URL. |
| `VERSION_PARSE_FAILED` | 301 | Version response could not be parsed. |
| `NO_UPDATE_AVAILABLE` | 302 | Already running latest firmware. |
| `VERSION_URL_NOT_SET` | 303 | `versionCheckURL` is empty. |
| `INSUFFICIENT_SPACE` | 400 | Not enough flash for the new image. |
| `FLASH_WRITE_FAILED` | 401 | Write to flash failed. |
| `FLASH_VERIFY_FAILED` | 402 | Post-write integrity check failed. |
| `UPDATE_BEGIN_FAILED` | 403 | `Update.begin()` returned false. |
| `UPDATE_END_FAILED` | 404 | `Update.end()` returned false. |
| `CORRUPT_FIRMWARE` | 405 | Binary is corrupt (bad size/checksum). |
| `PARTITION_NOT_FOUND` | 406 | No suitable OTA partition. |
| `ROLLBACK_FAILED` | 407 | Rollback could not be initiated. |
| `INTERRUPTED_UPDATE` | 408 | Previous update was cut short (power loss). |
| `FIRMWARE_TOO_LARGE` | 409 | Binary exceeds partition size. |
| `AUTH_FAILED` | 500 | Wrong username or password. |
| `ALREADY_UPDATING` | 900 | Concurrent update attempt. |

---

## Remote Server Setup

### Plain-text endpoint

`GET https://your-server.com/firmware/version.txt`

Response body:
```
1.3.0
```

Config:
```cpp
config.versionFormat       = OTAVersionFormat::PLAIN_TEXT;
config.versionCheckURL     = "https://your-server.com/firmware/version.txt";
config.firmwareDownloadURL = "https://your-server.com/firmware/firmware.bin";
```

---

### JSON endpoint

`GET https://your-server.com/firmware/version.json`

Response body:
```json
{
  "version": "1.3.0",
  "url": "https://your-server.com/firmware/firmware_1.3.0.bin"
}
```

The `url` field is optional; if present it overrides `firmwareDownloadURL`.

Config:
```cpp
config.versionFormat   = OTAVersionFormat::JSON;
config.versionCheckURL = "https://your-server.com/firmware/version.json";
```

---

### Hosting options

- **GitHub Releases** — upload `.bin` as a release asset; use the raw download URL.
- **Amazon S3 / Cloudflare R2** — serve files as public objects.
- **Nginx / Apache** — serve from any directory; no special config needed.
- **GitHub Pages** — commit `version.txt` and `.bin` to a `gh-pages` branch.

---

## Resilience & Edge Cases

| Scenario | Library behaviour |
|---|---|
| **Power loss mid-flash** | On next boot, `begin()` detects the interrupted flag and emits `INTERRUPTED_UPDATE`. Device boots the old (untouched) firmware. |
| **WiFi drops during upload** | Upload handler detects `WL_CONNECTED == false`, aborts `Update`, emits `WIFI_LOST_DURING_UPDATE`. |
| **WiFi drops during download** | Checked every 4 KB; download aborted, retried up to `maxRetries`. |
| **Corrupt `.bin` file** | `Update.end()` verifies MD5; if bad, emits `CORRUPT_FIRMWARE` or `FLASH_VERIFY_FAILED`. |
| **Insufficient partition space** | Checked before `Update.begin()`; emits `INSUFFICIENT_SPACE`. |
| **Flash write failure** | Byte-level mismatch detected; emits `FLASH_WRITE_FAILED`. |
| **HTTP timeout** | Request times out after `httpTimeoutMs`; emits `HTTP_TIMEOUT`, retried. |
| **Server returns wrong version format** | Parser returns `VERSION_PARSE_FAILED`. |
| **Concurrent update attempt** | Second call returns `ALREADY_UPDATING`. |
| **WDT during long flash** | Watchdog fed every `OTA_WATCHDOG_FEED_INTERVAL_MS` (1 s) via `yield()` / `ESP.wdtFeed()`. |

---

## Rollback Deep-Dive

### ESP32

ESP32's IDF bootloader supports native two-slot OTA rollback:

1. After `Update.end()` the new partition is marked **PENDING_VERIFY**.
2. On the next boot the bootloader runs the new firmware.
3. `OTA.begin()` calls `esp_ota_mark_app_valid_cancel_rollback()` — confirming the new firmware is healthy.
4. If the device reboots **before** `OTA.begin()` runs (power loss, crash), the bootloader automatically boots the **previous** partition.
5. To manually roll back: call `OTA.rollback(reason)` — it calls `esp_ota_mark_app_invalid_rollback_and_reboot()`.

### ESP8266

ESP8266 does not have hardware-level two-slot rollback in the same way. `OTA.rollback()` returns `false` with `ROLLBACK_FAILED`. For rollback-like behaviour on ESP8266 you must manage a secondary firmware slot yourself.

---

## FAQ

**Q: My ESP8266 runs out of heap during download. What can I do?**  
A: The library uses a 1 KB stack buffer for streaming. Reduce other heap usage or increase `httpTimeoutMs` to allow slower connections.

**Q: Can I change the `/update` path?**  
A: Not in this version. The path is fixed to `/update`. You can host additional routes using a separate `WebServer` instance on a different port.

**Q: How do I disable the web server and only use remote OTA?**  
A: Set `webServerPort` to an unused port (e.g. `8888`) or wrap `_server->begin()` — the server still needs to start for `OTA.loop()` to work, but nothing will connect if you don't share the URL.

**Q: Does this work with HTTPS download URLs?**  
A: Yes. On ESP8266 the library uses `setInsecure()` (accepts any certificate). For production, use a pinned fingerprint. On ESP32 the HTTPS client is configured similarly.

**Q: Can I use this without the singleton `OTA` object?**  
A: Yes. Instantiate `ESPFlexOTAClass myOTA;` directly and use `myOTA.begin(cfg)` etc.

**Q: The update page asks for a password but I set `authEnabled = false`.**  
A: If your router or browser cached a previous `WWW-Authenticate` challenge, do a hard refresh (Ctrl+Shift+R) or clear browser cache.
