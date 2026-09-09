#include "module/service/EspNowNotes.h"

#include <WiFi.h>
#include <esp_idf_version.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include <cstring>

#include "core/State.h"
#include "module/service/WiFi.h"

namespace
{
constexpr uint8_t ESPNOW_CHANNEL = 1;
constexpr unsigned long RETRY_INTERVAL_MS = 350;
constexpr unsigned long CHUNK_INTERVAL_MS = 100;
constexpr unsigned long SEND_TIMEOUT_MS = 30000;
constexpr unsigned long WIFI_RESTORE_TIMEOUT_MS = 8000;
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

enum class ServiceState : uint8_t
{
    Idle,
    Sending,
    RadioReady,
    RestoringWifi
};

enum class TransferKind : uint8_t
{
    Note,
    SnapshotControl
};

ServiceState state = ServiceState::Idle;
EspNowNotesResult pendingResult = EspNowNotesResult::None;
uint8_t packet[sticky_note::MAX_PACKET_BYTES] = {};
// Reused across transfers, never on the loop/task stack.
sticky_note::Note outgoingNote;
uint32_t outgoingChecksum = 0;
TransferKind transferKind = TransferKind::Note;
uint8_t snapshotControlType = 0;
uint16_t snapshotEntryCount = 0;
uint32_t snapshotDigest = 0;
uint8_t nextChunk = 0;
size_t packetLength = 0;
uint32_t sequence = 0;
uint8_t expectedAckVersion = sticky_note::LEGACY_VERSION;
bool keepRadioAfterAck = false;
unsigned long sendStartedMs = 0;
unsigned long lastSendMs = 0;
unsigned long wifiRestoreStartedMs = 0;
wifi_mode_t previousWifiMode = WIFI_MODE_NULL;
bool previousWifiConnected = false;
bool peerAdded = false;
bool espNowInitialized = false;

portMUX_TYPE ackMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t receivedAckSequence = 0;

#if ESP_IDF_VERSION_MAJOR >= 5
void onEspNowReceive(const esp_now_recv_info_t *, const uint8_t *data, int length)
#else
void onEspNowReceive(const uint8_t *, const uint8_t *data, int length)
#endif
{
    if (length < 0 || !sticky_note::validAck(data, static_cast<size_t>(length),
                                           sequence, expectedAckVersion))
    {
        return;
    }

    portENTER_CRITICAL(&ackMux);
    receivedAckSequence = sequence;
    portEXIT_CRITICAL(&ackMux);
}

uint32_t consumeAckSequence()
{
    portENTER_CRITICAL(&ackMux);
    const uint32_t ackSequence = receivedAckSequence;
    receivedAckSequence = 0;
    portEXIT_CRITICAL(&ackMux);
    return ackSequence;
}

void stopEspNow()
{
    if (!espNowInitialized)
        return;

    esp_now_unregister_recv_cb();
    if (peerAdded)
    {
        esp_now_del_peer(BROADCAST_MAC);
        peerAdded = false;
    }
    esp_now_deinit();
    espNowInitialized = false;
}

void finishWifiRestore()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        wifiConnected = true;
        wifiSSID = WiFi.SSID();
        applyWifiPowerSave();
    }
    else
    {
        wifiConnected = false;
    }
    state = ServiceState::Idle;
}

void beginWifiRestore(EspNowNotesResult result)
{
    stopEspNow();
    pendingResult = result;

    if (previousWifiConnected)
    {
        WiFi.mode(previousWifiMode == WIFI_MODE_NULL ? WIFI_MODE_STA : previousWifiMode);
        WiFi.reconnect();
        wifiRestoreStartedMs = millis();
        state = ServiceState::RestoringWifi;
        return;
    }

    WiFi.mode(previousWifiMode);
    wifiConnected = false;
    state = ServiceState::Idle;
}

bool queuePacket()
{
    lastSendMs = millis();
    if (transferKind == TransferKind::SnapshotControl)
    {
        packetLength = sticky_note::encodeSnapshotControl(
            snapshotControlType, sequence, snapshotEntryCount, snapshotDigest, packet);
    }
    else
    {
        packetLength = sticky_note::encodePacket(outgoingNote, nextChunk, outgoingChecksum, packet);
    }
    if (packetLength == 0 || esp_now_send(BROADCAST_MAC, packet, packetLength) != ESP_OK)
        return false;
    // Repeat the whole sequence until the receiver confirms render + save.
    // Missing, reordered, or duplicated chunks are safe at the receiver.
    if (transferKind == TransferKind::Note)
        nextChunk = (nextChunk + 1) % sticky_note::chunkCount(outgoingNote.messageLength);
    return true;
}

bool beginRadioSession()
{
    if (state == ServiceState::RadioReady)
        return true;

    previousWifiMode = WiFi.getMode();
    previousWifiConnected = WiFi.status() == WL_CONNECTED;
    if (previousWifiConnected)
    {
        WiFi.disconnect(false, false);
        wifiConnected = false;
        delay(20);
    }
    if (!WiFi.mode(WIFI_STA) || esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
        esp_now_init() != ESP_OK)
        return false;
    espNowInitialized = true;

    if (esp_now_register_recv_cb(onEspNowReceive) != ESP_OK)
        return false;

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK)
        return false;
    peerAdded = true;
    return true;
}

EspNowNotesStartResult beginTransfer()
{
    pendingResult = EspNowNotesResult::None;
    portENTER_CRITICAL(&ackMux);
    receivedAckSequence = 0;
    portEXIT_CRITICAL(&ackMux);

    if (!beginRadioSession())
    {
        beginWifiRestore(EspNowNotesResult::None);
        return EspNowNotesStartResult::RadioError;
    }

    state = ServiceState::Sending;
    sendStartedMs = millis();
    lastSendMs = sendStartedMs - RETRY_INTERVAL_MS;
    if (!queuePacket())
    {
        beginWifiRestore(EspNowNotesResult::None);
        return EspNowNotesStartResult::RadioError;
    }
    return EspNowNotesStartResult::Started;
}
} // namespace

uint32_t createEspNowNotesSequence()
{
    const uint32_t value = esp_random();
    return value == 0 ? 1 : value;
}

EspNowNotesStartResult startEspNowNoteSend(
    const char *message,
    size_t messageLength,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    bool keepRadioActive)
{
    if (state != ServiceState::Idle && state != ServiceState::RadioReady)
        return EspNowNotesStartResult::Busy;
    if (radioIsPlaying)
        return EspNowNotesStartResult::RadioPlaying;
    if (message == nullptr || messageLength == 0 || messageLength > sticky_note::MAX_MESSAGE_BYTES ||
        !sticky_note::validDate(year, month, day) ||
        !sticky_note::validUtf8(reinterpret_cast<const uint8_t *>(message), messageLength))
    {
        return EspNowNotesStartResult::InvalidNote;
    }

    transferKind = TransferKind::Note;
    keepRadioAfterAck = keepRadioActive;
    sequence = createEspNowNotesSequence();
    outgoingNote.sequence = sequence;
    outgoingNote.year = year;
    outgoingNote.month = month;
    outgoingNote.day = day;
    outgoingNote.messageLength = static_cast<uint16_t>(messageLength);
    memcpy(outgoingNote.message.data(), message, messageLength);
    outgoingNote.message[messageLength] = '\0';
    outgoingChecksum = sticky_note::crc32(reinterpret_cast<const uint8_t *>(message), messageLength);
    expectedAckVersion = sticky_note::wireVersion(outgoingNote);
    nextChunk = 0;
    Serial.printf("[NOTES] Sending %u bytes in %u packet(s), protocol v%u\n",
                  static_cast<unsigned>(messageLength),
                  static_cast<unsigned>(sticky_note::chunkCount(messageLength)),
                  static_cast<unsigned>(sticky_note::wireVersion(outgoingNote)));

    return beginTransfer();
}

EspNowNotesStartResult startEspNowSnapshotControl(
    uint8_t type,
    uint32_t snapshotSequence,
    uint16_t entryCount,
    uint32_t digest,
    bool finishSession)
{
    if (state != ServiceState::Idle && state != ServiceState::RadioReady)
        return EspNowNotesStartResult::Busy;
    if (radioIsPlaying)
        return EspNowNotesStartResult::RadioPlaying;
    if (snapshotSequence == 0 ||
        (type != sticky_note::TYPE_SNAPSHOT_BEGIN && type != sticky_note::TYPE_SNAPSHOT_COMMIT))
        return EspNowNotesStartResult::InvalidNote;

    transferKind = TransferKind::SnapshotControl;
    snapshotControlType = type;
    snapshotEntryCount = entryCount;
    snapshotDigest = digest;
    sequence = snapshotSequence;
    expectedAckVersion = sticky_note::SNAPSHOT_VERSION;
    keepRadioAfterAck = !finishSession;
    nextChunk = 0;
    Serial.printf("[NOTES] Sending snapshot %s: %u entries, digest %08lx\n",
                  type == sticky_note::TYPE_SNAPSHOT_BEGIN ? "begin" : "commit",
                  static_cast<unsigned>(entryCount), static_cast<unsigned long>(digest));
    return beginTransfer();
}

void tickEspNowNotes()
{
    const unsigned long now = millis();
    if (state == ServiceState::Sending)
    {
        if (consumeAckSequence() == sequence)
        {
            if (keepRadioAfterAck)
            {
                pendingResult = EspNowNotesResult::Sent;
                state = ServiceState::RadioReady;
            }
            else
            {
                beginWifiRestore(EspNowNotesResult::Sent);
            }
            return;
        }
        if (now - sendStartedMs >= SEND_TIMEOUT_MS)
        {
            beginWifiRestore(EspNowNotesResult::Timeout);
            return;
        }
        const unsigned long interval = transferKind == TransferKind::Note &&
                                               expectedAckVersion == sticky_note::CHUNK_VERSION
                                           ? CHUNK_INTERVAL_MS : RETRY_INTERVAL_MS;
        if (now - lastSendMs >= interval)
            queuePacket();
        return;
    }

    if (state == ServiceState::RestoringWifi &&
        (WiFi.status() == WL_CONNECTED || now - wifiRestoreStartedMs >= WIFI_RESTORE_TIMEOUT_MS))
    {
        finishWifiRestore();
    }
}

bool espNowNotesBusy()
{
    return state == ServiceState::Sending || state == ServiceState::RestoringWifi;
}

EspNowNotesResult takeEspNowNotesResult()
{
    const EspNowNotesResult result = pendingResult;
    pendingResult = EspNowNotesResult::None;
    return result;
}

void cancelEspNowNotes()
{
    if (state == ServiceState::Sending || state == ServiceState::RadioReady)
        beginWifiRestore(EspNowNotesResult::None);
}
