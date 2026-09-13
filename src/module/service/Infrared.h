#pragma once

#include <Arduino.h>
#include <vector>

enum class InfraredTxSource : uint8_t
{
    BuiltIn,
    External,
    Disabled,
};

enum class InfraredSignalType : uint8_t
{
    Parsed,
    Raw,
};

struct InfraredSettings
{
    InfraredTxSource txSource = InfraredTxSource::BuiltIn;
    uint8_t externalTxPin = 1;
    bool rxEnabled = false;
    uint8_t rxPin = 2;
    uint32_t rawFrequency = 38000;
    uint8_t rawDutyPercent = 33;
};

struct InfraredSignal
{
    InfraredSignalType type = InfraredSignalType::Parsed;
    String protocol;
    uint32_t address = 0;
    uint32_t command = 0;
    uint32_t frequency = 38000;
    uint8_t dutyPercent = 33;
    // Long idle gaps in captured files can exceed 65,535 microseconds.
    std::vector<uint32_t> timings;
};

constexpr uint8_t INFRARED_BUILTIN_TX_PIN = 44;

bool loadInfraredSettings(InfraredSettings &settings);
bool saveInfraredSettings(const InfraredSettings &settings);
bool validateInfraredSettings(const InfraredSettings &settings, String &error);
bool sendInfraredSignal(const InfraredSettings &settings,
                        const InfraredSignal &signal, String &error);
bool startInfraredCapture(const InfraredSettings &settings, String &error);
bool pollInfraredCapture(InfraredSignal &signal, bool &received, String &error);
void stopInfraredCapture();

String infraredTxSourceLabel(const InfraredSettings &settings);
String infraredRxLabel(const InfraredSettings &settings);
