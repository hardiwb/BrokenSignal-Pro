#include "core/Utils.h"

String folderName(const String &p, int maxCh)
{
    int sl = p.lastIndexOf('/');
    String n = (sl >= 0) ? p.substring(sl + 1) : p;
    n.replace('_', ' ');
    if (maxCh > 0 && (int)n.length() > maxCh)
        n = n.substring(0, maxCh - 1) + ">";
    return n;
}

String shortName(const String &p, int maxCh)
{
    int sl = p.lastIndexOf('/');
    String n = (sl >= 0) ? p.substring(sl + 1) : p;
    int dot = n.lastIndexOf('.');
    if (dot > 0)
        n = n.substring(0, dot);
    n.replace('_', ' ');
    if (maxCh > 0 && (int)n.length() > maxCh)
        n = n.substring(0, maxCh - 1) + ">";
    return n;
}

String formatTime(unsigned long ms)
{
    unsigned long s = ms / 1000;
    char b[8];
    snprintf(b, sizeof(b), "%02lu:%02lu", s / 60, s % 60);
    return String(b);
}
