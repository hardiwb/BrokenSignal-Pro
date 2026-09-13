#include "module/service/Infrared.h"

#define RAW_BUFFER_LENGTH 1024
#define RECORD_GAP_MICROS 12000
#include <IRremote.hpp>
#include <SD.h>

namespace
{
constexpr const char *SETTINGS_PATH = "/Infrared/settings.cfg";
bool captureRunning = false;
uint32_t captureFrequency = 38000;
uint8_t captureDutyPercent = 33;

bool externalPin(uint8_t pin)
{
    return pin == 1 || pin == 2;
}

uint8_t activeTxPin(const InfraredSettings &settings)
{
    return settings.txSource == InfraredTxSource::BuiltIn
               ? INFRARED_BUILTIN_TX_PIN
               : settings.externalTxPin;
}

String valueAfterColon(const String &line)
{
    const int separator = line.indexOf(':');
    if (separator < 0)
        return "";
    String value = line.substring(separator + 1);
    value.trim();
    return value;
}
} // namespace

bool loadInfraredSettings(InfraredSettings &settings)
{
    settings = {};
    File file = SD.open(SETTINGS_PATH, FILE_READ);
    if (!file)
        return true;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("tx_source:"))
        {
            String value = valueAfterColon(line);
            value.toLowerCase();
            if (value == "external") settings.txSource = InfraredTxSource::External;
            else if (value == "disabled") settings.txSource = InfraredTxSource::Disabled;
            else settings.txSource = InfraredTxSource::BuiltIn;
        }
        else if (line.startsWith("tx_pin:")) settings.externalTxPin = valueAfterColon(line).toInt();
        else if (line.startsWith("rx_enabled:")) settings.rxEnabled = valueAfterColon(line) == "1";
        else if (line.startsWith("rx_pin:")) settings.rxPin = valueAfterColon(line).toInt();
        else if (line.startsWith("raw_frequency:")) settings.rawFrequency = valueAfterColon(line).toInt();
        else if (line.startsWith("raw_duty_percent:")) settings.rawDutyPercent = valueAfterColon(line).toInt();
    }
    file.close();

    String error;
    if (!validateInfraredSettings(settings, error))
        settings = {};
    return true;
}

bool saveInfraredSettings(const InfraredSettings &settings)
{
    String error;
    if (!validateInfraredSettings(settings, error))
        return false;

    SD.mkdir("/Infrared");
    const char *temporary = "/Infrared/settings.tmp";
    SD.remove(temporary);
    File file = SD.open(temporary, FILE_WRITE);
    if (!file)
        return false;
    const char *source = settings.txSource == InfraredTxSource::BuiltIn
                             ? "built-in"
                             : settings.txSource == InfraredTxSource::External ? "external" : "disabled";
    file.printf("tx_source: %s\n", source);
    file.printf("tx_pin: %u\n", static_cast<unsigned>(settings.externalTxPin));
    file.printf("rx_enabled: %u\n", settings.rxEnabled ? 1U : 0U);
    file.printf("rx_pin: %u\n", static_cast<unsigned>(settings.rxPin));
    file.printf("raw_frequency: %lu\n", static_cast<unsigned long>(settings.rawFrequency));
    file.printf("raw_duty_percent: %u\n", static_cast<unsigned>(settings.rawDutyPercent));
    file.close();

    const char *backup = "/Infrared/settings.bak";
    SD.remove(backup);
    const bool existed = SD.exists(SETTINGS_PATH);
    if (existed && !SD.rename(SETTINGS_PATH, backup))
    {
        SD.remove(temporary);
        return false;
    }
    if (!SD.rename(temporary, SETTINGS_PATH))
    {
        if (existed) SD.rename(backup, SETTINGS_PATH);
        return false;
    }
    SD.remove(backup);
    return true;
}

bool validateInfraredSettings(const InfraredSettings &settings, String &error)
{
    if (settings.txSource == InfraredTxSource::External && !externalPin(settings.externalTxPin))
    {
        error = "Invalid TX pin";
        return false;
    }
    if (settings.rxEnabled && !externalPin(settings.rxPin))
    {
        error = "Invalid RX pin";
        return false;
    }
    if (settings.rxEnabled && settings.txSource == InfraredTxSource::External &&
        settings.rxPin == settings.externalTxPin)
    {
        error = "RX and TX pins conflict";
        return false;
    }
    if (settings.rawFrequency < 30000 || settings.rawFrequency > 60000)
    {
        error = "Invalid raw frequency";
        return false;
    }
    if (settings.rawDutyPercent < 10 || settings.rawDutyPercent > 90)
    {
        error = "Invalid raw duty cycle";
        return false;
    }
    return true;
}

bool sendInfraredSignal(const InfraredSettings &settings,
                        const InfraredSignal &signal, String &error)
{
    if (settings.txSource == InfraredTxSource::Disabled)
    {
        error = "Configure IR transmitter first";
        return false;
    }
    if (!validateInfraredSettings(settings, error))
        return false;

    const uint8_t pin = activeTxPin(settings);
    IrSender.begin(pin);
    if (signal.type == InfraredSignalType::Raw)
    {
        if (signal.timings.empty() || signal.timings.size() > 1024 ||
            signal.frequency < 30000 || signal.frequency > 60000 ||
            signal.dutyPercent < 10 || signal.dutyPercent > 90)
        {
            error = signal.timings.size() > 1024 ? "Raw data too long" : "Invalid IR file";
            return false;
        }
        IrSender.enableIROut(static_cast<uint_fast8_t>((signal.frequency + 500) / 1000));
        const uint32_t duty = (signal.dutyPercent * 255U + 50U) / 100U;
        for (size_t i = 0; i < signal.timings.size(); ++i)
        {
            if ((i & 1U) == 0)
            {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
                ledcWrite(pin, duty);
#else
                ledcWrite(SEND_LEDC_CHANNEL, duty);
#endif
            }
            else
            {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
                ledcWrite(pin, 0);
#else
                ledcWrite(SEND_LEDC_CHANNEL, 0);
#endif
            }
            IRsend::customDelayMicroseconds(signal.timings[i]);
        }
        IrSender.IRLedOff();
        return true;
    }

    if (signal.address > 0xffff || signal.command > 0xffff)
    {
        error = "Unsupported protocol";
        return false;
    }
    if (signal.protocol == "NEC")
        IrSender.sendNEC(signal.address, signal.command, 0);
    else if (signal.protocol == "NECext")
        IrSender.sendOnkyo(signal.address, signal.command, 0);
    else if (signal.protocol == "Samsung32")
        IrSender.sendSamsung(signal.address, signal.command, 0);
    else
    {
        error = "Unsupported protocol";
        return false;
    }
    IrSender.IRLedOff();
    return true;
}

bool startInfraredCapture(const InfraredSettings &settings, String &error)
{
    if (!settings.rxEnabled)
    {
        error = "Enable IR receiver first";
        return false;
    }
    if (!validateInfraredSettings(settings, error)) return false;
    stopInfraredCapture();
    captureFrequency = settings.rawFrequency;
    captureDutyPercent = settings.rawDutyPercent;
    IrReceiver.begin(settings.rxPin, false);
    captureRunning = true;
    return true;
}

bool pollInfraredCapture(InfraredSignal &signal, bool &received, String &error)
{
    received = false;
    if (!captureRunning || !IrReceiver.decode()) return true;
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW)
    {
        stopInfraredCapture(); error = "IR signal too long"; return false;
    }
    const uint16_t rawLength = IrReceiver.decodedIRData.rawlen;
    if (rawLength < 3 || rawLength > RAW_BUFFER_LENGTH)
    {
        stopInfraredCapture(); error = "Invalid IR signal"; return false;
    }
    signal = {};
    signal.type = InfraredSignalType::Raw;
    signal.frequency = captureFrequency;
    signal.dutyPercent = captureDutyPercent;
    signal.timings.reserve(rawLength - 1);
    for (uint16_t i = 1; i < rawLength; ++i)
    {
        uint32_t duration = static_cast<uint32_t>(IrReceiver.irparams.rawbuf[i]) * MICROS_PER_TICK;
        if (i & 1U)
        {
            if (duration > MARK_EXCESS_MICROS) duration -= MARK_EXCESS_MICROS;
        }
        else duration += MARK_EXCESS_MICROS;
        signal.timings.push_back(duration ? duration : 1);
    }
    stopInfraredCapture(); received = true; return true;
}

void stopInfraredCapture()
{
    if (!captureRunning) return;
    IrReceiver.end();
    captureRunning = false;
}

String infraredTxSourceLabel(const InfraredSettings &settings)
{
    if (settings.txSource == InfraredTxSource::BuiltIn)
        return "Built-in G44";
    if (settings.txSource == InfraredTxSource::External)
        return "External G" + String(settings.externalTxPin);
    return "Disabled";
}

String infraredRxLabel(const InfraredSettings &settings)
{
    return settings.rxEnabled ? "External G" + String(settings.rxPin) : "Disabled";
}
