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
    uint8_t day);
void tickEspNowNotes();
bool espNowNotesBusy();
EspNowNotesResult takeEspNowNotesResult();
void cancelEspNowNotes();
