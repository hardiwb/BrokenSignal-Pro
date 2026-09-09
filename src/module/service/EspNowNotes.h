#pragma once

#include <Arduino.h>
#include "module/service/StickyNoteProtocol.h"

enum class EspNowNotesStartResult : uint8_t
{
    Started,
    Busy,
    RadioPlaying,
    InvalidNote,
    RadioError
};

enum class EspNowNotesResult : uint8_t
{
    None,
    Sent,
    Timeout,
    RadioError
};

EspNowNotesStartResult startEspNowNoteSend(
    const char *message,
    size_t messageLength,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    bool keepRadioActive = false);
EspNowNotesStartResult startEspNowSnapshotControl(
    uint8_t type,
    uint32_t snapshotSequence,
    uint16_t entryCount,
    uint32_t digest,
    bool finishSession = false);
uint32_t createEspNowNotesSequence();
void tickEspNowNotes();
bool espNowNotesBusy();
EspNowNotesResult takeEspNowNotesResult();
void cancelEspNowNotes();
