#pragma once
#ifndef OTA_TYPES_H
#define OTA_TYPES_H

/**
 * OTATypes.h — All enums and primitive type aliases for ESPFlexOTA.
 *
 * This is the single source of truth for every enum in the library.
 * Every other header includes this file; nothing here depends on anything
 * except <Arduino.h>.
 *
 *  Enums defined here:
 *    - OTASource          where a firmware update originated
 *    - OTAVersionFormat   how the version-check URL returns its payload
 *    - OTAErrorCode       every error the library can produce
 *    - OTAEventType       internal event queue discriminator
 */

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// OTASource — where a firmware update originated
// ─────────────────────────────────────────────────────────────────────────────
enum class OTASource : uint8_t {
    LOCAL_WEB  = 0,   ///< Browser-upload via /update endpoint
    REMOTE_URL = 1,   ///< Downloaded from a remote HTTP(S) URL
    UNKNOWN    = 0xFF
};

// ─────────────────────────────────────────────────────────────────────────────
// OTAVersionFormat — how the version-check URL returns its payload
// ─────────────────────────────────────────────────────────────────────────────
enum class OTAVersionFormat : uint8_t {
    PLAIN_TEXT = 0,   ///< Response body is a bare version string e.g. "1.2.3"
    JSON       = 1    ///< Response is JSON; library reads the "version" key
};

// ─────────────────────────────────────────────────────────────────────────────
// OTAErrorCode — every error the library can produce
//
//  0        → no error
//  100–199  → configuration / initialisation
//  200–299  → network / connectivity
//  300–399  → version-check / parsing
//  400–499  → flash / partition
//  500–599  → authentication
//  900+     → unexpected / internal
// ─────────────────────────────────────────────────────────────────────────────
enum class OTAErrorCode : uint16_t {
    // No error
    NONE                    = 0,

    // Configuration
    NOT_INITIALISED         = 100,  ///< begin() was not called
    INVALID_CONFIG          = 101,  ///< Supplied OTAConfig is invalid
    SERVER_START_FAILED     = 102,  ///< Could not start the web server

    // Network
    WIFI_NOT_CONNECTED      = 200,  ///< WiFi link is down
    HTTP_REQUEST_FAILED     = 201,  ///< HTTP GET/POST returned an error
    HTTP_TIMEOUT            = 202,  ///< HTTP operation timed out
    DOWNLOAD_FAILED         = 203,  ///< Firmware binary download failed
    WIFI_LOST_DURING_UPDATE = 204,  ///< WiFi disconnected mid-flash
    HTTP_REDIRECT_LIMIT     = 205,  ///< Too many HTTP redirects

    // Version check
    VERSION_CHECK_FAILED    = 300,  ///< Could not reach the version-check URL
    VERSION_PARSE_FAILED    = 301,  ///< Could not parse the version response
    NO_UPDATE_AVAILABLE     = 302,  ///< Firmware is already up-to-date
    VERSION_URL_NOT_SET     = 303,  ///< versionCheckURL is empty

    // Flash / partition
    INSUFFICIENT_SPACE      = 400,  ///< Not enough flash space for the new image
    FLASH_WRITE_FAILED      = 401,  ///< Write to flash failed
    FLASH_VERIFY_FAILED     = 402,  ///< Post-write integrity check failed
    UPDATE_BEGIN_FAILED     = 403,  ///< Update.begin() returned false
    UPDATE_END_FAILED       = 404,  ///< Update.end() returned false
    CORRUPT_FIRMWARE        = 405,  ///< Firmware binary is corrupt
    PARTITION_NOT_FOUND     = 406,  ///< No suitable OTA partition found
    ROLLBACK_FAILED         = 407,  ///< Rollback could not be initiated
    INTERRUPTED_UPDATE      = 408,  ///< Previous update was interrupted (power loss)
    FIRMWARE_TOO_LARGE      = 409,  ///< Binary exceeds partition size

    // Authentication
    AUTH_FAILED             = 500,  ///< Wrong username / password

    // Internal
    ALREADY_UPDATING        = 900,  ///< An update is already in progress
    UNKNOWN                 = 999   ///< Catch-all unknown error
};

// ─────────────────────────────────────────────────────────────────────────────
// OTAEventType — internal event queue discriminator
// ─────────────────────────────────────────────────────────────────────────────
enum class OTAEventType : uint8_t {
    NONE     = 0,
    START    = 1,
    PROGRESS = 2,
    COMPLETE = 3,
    ERROR    = 4,
    ROLLBACK = 5
};

#endif // OTA_TYPES_H
