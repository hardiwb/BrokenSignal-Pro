#pragma once

#include <Arduino.h>

struct NotesNotionConfig
{
    String apiToken;
    String databaseId;
    String dataSourceId;
};

bool beginNotesNotionSetup();
void tickNotesNotionSetup();
void stopNotesNotionSetup();
bool notesNotionSetupActive();
void drawNotesNotionSetupScreen();

bool loadNotesNotionConfig(NotesNotionConfig &config);
bool saveNotesNotionDataSourceId(const String &dataSourceId);

