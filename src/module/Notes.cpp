#include "core/Config.h"
#include "core/Types.h"
#include "UI/Themes.h"
#include "UI/UI.h"
#include "core/State.h"
#include "module/Clock.h"
#include "module/Notes.h"
#include <SD.h>
#include <vector>
#include <M5Cardputer.h>

namespace
{
    M5Canvas notesCanvas(&M5Cardputer.Display);
    M5Canvas notesHeaderCanvas(&M5Cardputer.Display);
    M5Canvas noteRowCanvas(&M5Cardputer.Display);
    M5Canvas notesStatusCanvas(&M5Cardputer.Display);
    std::vector<String> noteLines;

    String currentDate;
    String notePath;

    const int NOTES_HEADER_H = HEADER_H;
    const int NOTES_STATUS_H = 18;

    const int NOTES_FIRST_Y = 40;
    const int NOTES_LINE_H = 15;

    const int NOTES_X = 18;
    const int NOTES_TEXT_W = 210;

    bool initialized = false;
    bool notesNeedsFullRedraw = true;
    int lastNotesMinute = -1;
    unsigned long lastNotesMarqueeDraw = 0;

    String getTodayString()
    {
        struct tm now{};

        if (!getCurrentTime(now))
        {
            Serial.println("Notes: clock unavailable");
            return "2000-01-01";
        }

        char buf[16];

        snprintf(
            buf,
            sizeof(buf),
            "%04d-%02d-%02d",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday);

        return String(buf);
    }

    //====================
    // Logic
    //====================

    void loadNote()
    {
        noteLines.clear();

        currentDate = getTodayString();
        notePath = "/Notes/" + currentDate + ".txt";

        Serial.print("Notes: ");
        Serial.println(notePath);

        File f = SD.open(notePath, FILE_READ);

        if (!f)
        {
            Serial.println("Notes: no note for today");
            return;
        }

        while (f.available())
        {
            String line = f.readStringUntil('\n');

            // Remove CR from Windows-style CRLF
            line.trim();

            noteLines.push_back(line);
        }

        f.close();

        Serial.print("Notes lines: ");
        Serial.println(noteLines.size());

        if (notesSelected >= (int)noteLines.size())
            notesSelected = max(0, (int)noteLines.size() - 1);

        if (notesScrollTop >= (int)noteLines.size())
            notesScrollTop = max(0, (int)noteLines.size() - 1);
    }

    //====================
    // UI
    //====================
    void drawNotesHeader()
    {
        notesHeaderCanvas.fillRect(
            0,
            0,
            SCREEN_W,
            NOTES_HEADER_H,
            T->hdrBg);

        //==================================================
        // HEADER TITLE
        //==================================================

        notesHeaderCanvas.setTextDatum(middle_left);
        notesHeaderCanvas.setTextColor(T->textDim);

        notesHeaderCanvas.drawString(
            "BRKN_SIGNAL // NOTES",
            4,
            7,
            1);

        //==================================================
        // CLOCK
        //==================================================

        char clockBuf[6];
        formatClock(clockBuf, sizeof(clockBuf));

        notesHeaderCanvas.setTextDatum(middle_right);
        notesHeaderCanvas.setTextColor(T->accent2);

        notesHeaderCanvas.setTextDatum(middle_right);
        notesHeaderCanvas.setTextColor(T->accent2);

        notesHeaderCanvas.drawString(
            clockBuf,
            SCREEN_W - 4,
            7,
            1);

        //==================================================
        // DATE
        //==================================================
        struct tm now{};
        char dateText[32];

        if (getCurrentTime(now))
        {
            strftime(
                dateText,
                sizeof(dateText),
                "%A, %b %d %Y",
                &now);
        }
        else
        {
            strcpy(dateText, "Date unavailable");
        }

        notesHeaderCanvas.setTextDatum(middle_left);
        notesHeaderCanvas.setTextColor(T->accent1);

        notesHeaderCanvas.drawString(
            dateText,
            8,
            21,
            2);

        //==================================================
        // HEADER LINE
        //==================================================

        notesHeaderCanvas.drawFastHLine(
            0,
            NOTES_HEADER_H - 1,
            SCREEN_W,
            T->textDim);
    }

    void drawNoteRow(int index)
    {
        const int visibleLines =
            (SCREEN_H - NOTES_HEADER_H - NOTES_STATUS_H) /
            NOTES_LINE_H;

        if (index < notesScrollTop ||
            index >= notesScrollTop + visibleLines)
            return;

        int y =
            NOTES_FIRST_Y +
            (index - notesScrollTop) * NOTES_LINE_H;

        bool selected =
            (index == notesSelected);

        noteRowCanvas.fillSprite(T->bg);

        if (selected)
        {
            noteRowCanvas.fillRect(
                0,
                0,
                SCREEN_W,
                NOTES_LINE_H,
                T->hdrBg);

            noteRowCanvas.fillRect(
                2,
                2,
                5,
                10,
                T->accent1);

            noteRowCanvas.setTextColor(T->accent1);

            drawMarquee(
                noteRowCanvas,
                noteLines[index],
                NOTES_X,
                7,
                NOTES_TEXT_W,
                T->accent1,
                notesMarqueeStartMs);
        }
        else
        {
            noteRowCanvas.setTextColor(T->textDim);

            noteRowCanvas.drawString(
                noteLines[index],
                NOTES_X,
                7,
                1);
        }

        noteRowCanvas.pushSprite(
            &M5Cardputer.Display,
            0,
            y - 7);
    }

    void drawNotesList()
    {
        const int visibleLines =
            (SCREEN_H - NOTES_HEADER_H - NOTES_STATUS_H) /
            NOTES_LINE_H;

        for (int i = 0; i < visibleLines; i++)
        {
            int index = notesScrollTop + i;

            if (index >= (int)noteLines.size())
                break;

            drawNoteRow(index);
        }
    }

    void drawNotesStatus()
    {
        const int y = NOTES_STATUS_H / 2;

        notesStatusCanvas.fillSprite(T->hdrBg);

        notesStatusCanvas.drawFastHLine(
            0,
            0,
            SCREEN_W,
            T->textDim);

        notesStatusCanvas.setTextDatum(middle_left);
        notesStatusCanvas.setTextColor(T->textDim);

        notesStatusCanvas.drawString(
            "UP/DN",
            4,
            y,
            1);

        notesStatusCanvas.setTextColor(T->accent1);

        notesStatusCanvas.drawString(
            "ENTER EDIT",
            48,
            y,
            1);

        notesStatusCanvas.setTextColor(T->textDim);
        notesStatusCanvas.setTextDatum(middle_right);

        char pos[16];

        snprintf(
            pos,
            sizeof(pos),
            "%d/%d",
            noteLines.empty() ? 0 : notesSelected + 1,
            noteLines.size());

        notesStatusCanvas.drawString(
            pos,
            SCREEN_W - 4,
            y,
            1);
    }

    void drawNotesFull()
    {
        // Header
        drawNotesHeader();

        notesHeaderCanvas.pushSprite(
            &M5Cardputer.Display,
            0,
            0);

        // List
        drawNotesList();

        // Status
        drawNotesStatus();

        notesStatusCanvas.pushSprite(
            &M5Cardputer.Display,
            0,
            SCREEN_H - NOTES_STATUS_H);
    }
}

//============================
// State
//============================

void notesBegin()
{

    if (initialized)
        return;

    Serial.println("Notes: begin");

    notesHeaderCanvas.setColorDepth(16);
    notesHeaderCanvas.createSprite(
        SCREEN_W,
        NOTES_HEADER_H);

    noteRowCanvas.setColorDepth(16);
    noteRowCanvas.createSprite(
        SCREEN_W,
        NOTES_LINE_H);

    notesStatusCanvas.setColorDepth(16);
    notesStatusCanvas.createSprite(
        SCREEN_W,
        NOTES_STATUS_H);

    initialized = true;

    loadNote();
}

void notesOpen()
{
    notesBegin();
    loadNote();

    notesMode = true;
    notesSelected = 0;
    notesScrollTop = 0;
    notesMarqueeStartMs = millis();

    notesNeedsFullRedraw = true;

    Serial.println("Notes: OPEN");
}

void notesClose()
{
    notesMode = false;

    Serial.println("Notes: CLOSE");
}

void notesDraw()
{
    if (!notesMode)
        return;

    if (notesNeedsFullRedraw)
    {
        drawNotesFull();
        notesNeedsFullRedraw = false;
        return;
    }

    struct tm now{};

    if (getCurrentTime(now))
    {
        if (now.tm_min != lastNotesMinute)
        {
            lastNotesMinute = now.tm_min;

            drawNotesHeader();

            notesHeaderCanvas.pushSprite(
                &M5Cardputer.Display,
                0,
                0);
        }
    }
}

//============================
// Navigation
//============================
void handleNotesInput(Keyboard_Class::KeysState &ks)
{
    if (!notesMode)
        return;

    // ESC / DEL to close notes
    if (ks.del)
    {
        notesClose();
        return;
    }

    // ENTER editor
    if (ks.enter)
    {
        // temporary
        return;
    }

    for (auto c : ks.word)
    {
        switch (c)
        {
        // ';' = UP
        case ';':
            if (notesSelected > 0)
            {
                int oldSelected = notesSelected;
                int oldScroll = notesScrollTop;

                notesSelected--;

                if (notesSelected < notesScrollTop)
                {
                    notesScrollTop = notesSelected;
                }

                notesMarqueeStartMs = millis();

                // Scroll changed → redraw entire visible list
                if (notesScrollTop != oldScroll)
                {
                    drawNotesList();
                }
                else
                {
                    drawNoteRow(oldSelected);
                    drawNoteRow(notesSelected);
                }

                drawNotesStatus();

                notesStatusCanvas.pushSprite(
                    &M5Cardputer.Display,
                    0,
                    SCREEN_H - NOTES_STATUS_H);
            }
            break;

        // '.' = DOWN
        case '.':
            if (!noteLines.empty() &&
                notesSelected < (int)noteLines.size() - 1)
            {
                int oldSelected = notesSelected;
                int oldScroll = notesScrollTop;

                notesSelected++;

                const int visibleLines =
                    (SCREEN_H - NOTES_HEADER_H - NOTES_STATUS_H) /
                    NOTES_LINE_H;

                if (notesSelected >=
                    notesScrollTop + visibleLines)
                {
                    notesScrollTop =
                        notesSelected - visibleLines + 1;
                }

                notesMarqueeStartMs = millis();

                // Scroll changed → redraw entire visible list
                if (notesScrollTop != oldScroll)
                {
                    drawNotesList();
                }
                else
                {
                    drawNoteRow(oldSelected);
                    drawNoteRow(notesSelected);
                }

                drawNotesStatus();

                notesStatusCanvas.pushSprite(
                    &M5Cardputer.Display,
                    0,
                    SCREEN_H - NOTES_STATUS_H);
            }
            break;
        }
    }
}

//============================
// Loop
//============================
void notesLoop()
{
    if (!notesMode)
        return;

    static char lastClock[6] = "";

    char clockBuf[6];
    formatClock(clockBuf, sizeof(clockBuf));

    if (strcmp(clockBuf, lastClock) != 0)
    {
        strcpy(lastClock, clockBuf);

        drawNotesHeader();

        notesHeaderCanvas.pushSprite(
            &M5Cardputer.Display,
            0,
            0);
    }
}