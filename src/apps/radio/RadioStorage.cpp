#include "apps/radio/Radio.h"

#include <SD.h>

#include "core/State.h"

void loadRadioList()
{
    radioCount = 0;
    File file = SD.open("/Music/_radio/webradio.cfg", FILE_READ);
    if (!file)
        return;

    while (file.available() && radioCount < RADIO_MAX)
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        const int separator = line.indexOf('|');
        if (separator < 0)
        {
            radioList[radioCount].name = "Radio " + String(radioCount + 1);
            radioList[radioCount].url = line;
        }
        else
        {
            radioList[radioCount].name = line.substring(0, separator);
            radioList[radioCount].url = line.substring(separator + 1);
        }
        if (radioList[radioCount].url.length() > 0)
            ++radioCount;
    }
    file.close();
}

void saveRadioList()
{
    SD.mkdir("/Music/_radio");
    File file = SD.open("/Music/_radio/webradio.cfg", FILE_WRITE);
    if (!file)
        return;
    for (int i = 0; i < radioCount; ++i)
        file.printf("%s|%s\n", radioList[i].name.c_str(), radioList[i].url.c_str());
    file.close();
}

String generateRadioName(const String &url, int number)
{
    int start = url.indexOf("://");
    if (start >= 0)
    {
        start += 3;
        const int end = url.indexOf('/', start);
        String domain = end > 0 ? url.substring(start, end) : url.substring(start);
        if (domain.startsWith("www."))
            domain = domain.substring(4);
        const int dot = domain.lastIndexOf('.');
        if (dot > 0)
            domain = domain.substring(0, dot);
        domain.replace('-', ' ');
        if (domain.length() > 0 && domain.length() <= 20)
        {
            domain.toUpperCase();
            return domain;
        }
    }
    return "Radio " + String(number);
}
