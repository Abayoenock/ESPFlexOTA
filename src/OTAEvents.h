#pragma once
#ifndef OTA_EVENTS_H
#define OTA_EVENTS_H

/**
 * OTAEvents.h — Internal event record for ESPFlexOTA.
 *
 * Only the OTAEvent struct lives here.
 * OTAEventType enum is defined in OTATypes.h.
 * OTASource     enum is defined in OTATypes.h.
 * OTAError      struct is defined in OTAError.h.
 */

#include "OTATypes.h"   // OTAEventType, OTASource
#include "OTAError.h"   // OTAError

/**
 * OTAEvent — lightweight record queued during upload/flash and
 * flushed to user callbacks in loop().
 */
struct OTAEvent {
    OTAEventType type     = OTAEventType::NONE;
    OTASource    source   = OTASource::UNKNOWN;
    int          progress = 0;      ///< percent (0–100) for PROGRESS events
    size_t       written  = 0;      ///< bytes written for PROGRESS events
    size_t       total    = 0;      ///< total bytes for PROGRESS events
    OTAError     error;             ///< populated for ERROR events
    String       rollbackReason;    ///< populated for ROLLBACK events
};

#endif // OTA_EVENTS_H
