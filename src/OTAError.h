#pragma once
#ifndef OTA_ERROR_H
#define OTA_ERROR_H

/**
 * OTAError.h — Structured error type for ESPFlexOTA.
 *
 * Only the OTAError struct lives here.
 * OTAErrorCode enum is defined in OTATypes.h.
 *
 * Usage:
 * @code
 *   OTA.onError([](const OTAError& e) {
 *       Serial.printf("[OTA] Error %d: %s\n", (int)e.code, e.message.c_str());
 *   });
 * @endcode
 */

#include <Arduino.h>
#include "OTATypes.h"   // OTAErrorCode

struct OTAError {

    OTAErrorCode code     = OTAErrorCode::NONE;
    String       message;           ///< Human-readable description
    int          httpCode = 0;      ///< Raw HTTP status code when applicable
    uint32_t     timestamp = 0;     ///< millis() when the error occurred

    OTAError() = default;

    OTAError(OTAErrorCode c, const String& msg, int http = 0)
        : code(c), message(msg), httpCode(http), timestamp(millis()) {}

    /** Returns true when this object represents a real error. */
    bool isError() const { return code != OTAErrorCode::NONE; }

    /** Clear the error state. */
    void clear() {
        code      = OTAErrorCode::NONE;
        message   = "";
        httpCode  = 0;
        timestamp = 0;
    }

    /**
     * Serialize to a human-readable string for logging.
     * e.g. "[OTAError 401] Flash write failed (HTTP: 0)"
     */
    String toString() const {
        String s = "[OTAError ";
        s += String((uint16_t)code);
        s += "] ";
        s += message;
        if (httpCode != 0) {
            s += " (HTTP: ";
            s += String(httpCode);
            s += ")";
        }
        return s;
    }

    /**
     * Map an OTAErrorCode to a default short message.
     * Useful for producing readable output without custom messages.
     */
    static String defaultMessage(OTAErrorCode c) {
        switch (c) {
            case OTAErrorCode::NONE:                    return "No error";
            case OTAErrorCode::NOT_INITIALISED:         return "OTA not initialised";
            case OTAErrorCode::INVALID_CONFIG:          return "Invalid configuration";
            case OTAErrorCode::SERVER_START_FAILED:     return "Web server failed to start";
            case OTAErrorCode::WIFI_NOT_CONNECTED:      return "WiFi not connected";
            case OTAErrorCode::HTTP_REQUEST_FAILED:     return "HTTP request failed";
            case OTAErrorCode::HTTP_TIMEOUT:            return "HTTP request timed out";
            case OTAErrorCode::DOWNLOAD_FAILED:         return "Firmware download failed";
            case OTAErrorCode::WIFI_LOST_DURING_UPDATE: return "WiFi lost during update";
            case OTAErrorCode::HTTP_REDIRECT_LIMIT:     return "Too many HTTP redirects";
            case OTAErrorCode::VERSION_CHECK_FAILED:    return "Version check failed";
            case OTAErrorCode::VERSION_PARSE_FAILED:    return "Version parse failed";
            case OTAErrorCode::NO_UPDATE_AVAILABLE:     return "Firmware is up-to-date";
            case OTAErrorCode::VERSION_URL_NOT_SET:     return "Version check URL not configured";
            case OTAErrorCode::INSUFFICIENT_SPACE:      return "Insufficient flash space";
            case OTAErrorCode::FLASH_WRITE_FAILED:      return "Flash write failed";
            case OTAErrorCode::FLASH_VERIFY_FAILED:     return "Flash verify failed";
            case OTAErrorCode::UPDATE_BEGIN_FAILED:     return "Update begin failed";
            case OTAErrorCode::UPDATE_END_FAILED:       return "Update end failed";
            case OTAErrorCode::CORRUPT_FIRMWARE:        return "Firmware binary is corrupt";
            case OTAErrorCode::PARTITION_NOT_FOUND:     return "OTA partition not found";
            case OTAErrorCode::ROLLBACK_FAILED:         return "Rollback failed";
            case OTAErrorCode::INTERRUPTED_UPDATE:      return "Previous update was interrupted";
            case OTAErrorCode::FIRMWARE_TOO_LARGE:      return "Firmware binary too large";
            case OTAErrorCode::AUTH_FAILED:             return "Authentication failed";
            case OTAErrorCode::ALREADY_UPDATING:        return "Update already in progress";
            default:                                    return "Unknown error";
        }
    }
};

#endif // OTA_ERROR_H
