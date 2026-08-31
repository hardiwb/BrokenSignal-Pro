#pragma once

// Wire contract shared verbatim with CrossInk's Sticky Notes receiver.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sticky_note {
constexpr std::array<uint8_t, 4> MAGIC = {'C', 'I', 'N', 'T'};
constexpr uint8_t LEGACY_VERSION = 1;
constexpr uint8_t CHUNK_VERSION = 2;
constexpr uint8_t TYPE_NOTE = 1;
constexpr uint8_t TYPE_ACK = 2;
constexpr size_t HEADER_BYTES = 16;
constexpr size_t CHUNK_HEADER_BYTES = 24;
constexpr size_t CHUNK_BYTES = 220;
constexpr size_t MAX_MESSAGE_BYTES = 2048;
constexpr size_t MAX_PACKET_BYTES = CHUNK_HEADER_BYTES + CHUNK_BYTES;
constexpr uint32_t ASSEMBLY_TIMEOUT_MS = 5000;
static_assert(MAX_PACKET_BYTES <= 250, "Must fit ESP-NOW v1 radios");

struct Note {
  uint32_t sequence = 0;
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  std::array<char, MAX_MESSAGE_BYTES + 1> message{};
  uint16_t messageLength = 0;
};

inline uint16_t readU16Le(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline uint32_t readU32Le(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline void writeU16Le(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8);
}
inline void writeU32Le(uint8_t* p, uint32_t v) {
  for (uint8_t i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
inline bool validDate(uint16_t year, uint8_t month, uint8_t day) {
  if (year < 2024 || year > 2099 || month == 0 || month > 12 || day == 0) return false;
  static constexpr uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t limit = DAYS[month - 1];
  if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ++limit;
  return day <= limit;
}
inline bool validUtf8(const uint8_t* data, size_t length) {
  size_t i = 0;
  while (i < length) {
    const uint8_t first = data[i++];
    if (first < 0x80) {
      if (first == 0 || (first < 0x20 && first != '\n' && first != '\r' && first != '\t')) return false;
      continue;
    }
    uint8_t count;
    uint32_t cp, minimum;
    if ((first & 0xe0) == 0xc0) { count = 1; cp = first & 0x1f; minimum = 0x80; }
    else if ((first & 0xf0) == 0xe0) { count = 2; cp = first & 0x0f; minimum = 0x800; }
    else if ((first & 0xf8) == 0xf0) { count = 3; cp = first & 7; minimum = 0x10000; }
    else return false;
    if (i + count > length) return false;
    for (uint8_t n = 0; n < count; ++n) {
      const uint8_t next = data[i++];
      if ((next & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (next & 0x3f);
    }
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
  }
  return true;
}
inline uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffU;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
  }
  return ~crc;
}
inline uint8_t chunkCount(size_t length) {
  return static_cast<uint8_t>((length + CHUNK_BYTES - 1) / CHUNK_BYTES);
}
inline uint8_t wireVersion(const Note& note) {
  return note.messageLength <= CHUNK_BYTES ? LEGACY_VERSION : CHUNK_VERSION;
}
inline size_t encodePacket(const Note& note, uint8_t index, uint32_t checksum, uint8_t* packet) {
  if (!packet || note.sequence == 0 || note.messageLength == 0 || note.messageLength > MAX_MESSAGE_BYTES ||
      !validDate(note.year, note.month, note.day) || index >= chunkCount(note.messageLength)) return 0;
  const uint8_t version = wireVersion(note);
  const size_t offset = index * CHUNK_BYTES;
  const size_t size = std::min(CHUNK_BYTES, static_cast<size_t>(note.messageLength) - offset);
  const size_t header = version == LEGACY_VERSION ? HEADER_BYTES : CHUNK_HEADER_BYTES;
  memset(packet, 0, header);
  std::copy(MAGIC.begin(), MAGIC.end(), packet);
  packet[4] = version; packet[5] = TYPE_NOTE; packet[6] = index; packet[7] = static_cast<uint8_t>(size);
  writeU32Le(packet + 8, note.sequence); writeU16Le(packet + 12, note.year);
  packet[14] = note.month; packet[15] = note.day;
  if (version == CHUNK_VERSION) {
    writeU16Le(packet + 16, note.messageLength);
    packet[18] = chunkCount(note.messageLength);
    writeU32Le(packet + 20, checksum);
  }
  memcpy(packet + header, note.message.data() + offset, size);
  return header + size;
}
inline std::array<uint8_t, HEADER_BYTES> makeAck(uint32_t sequence, uint8_t version = LEGACY_VERSION) {
  std::array<uint8_t, HEADER_BYTES> packet{};
  std::copy(MAGIC.begin(), MAGIC.end(), packet.begin());
  packet[4] = version; packet[5] = TYPE_ACK;
  writeU32Le(packet.data() + 8, sequence);
  return packet;
}
inline bool validAck(const uint8_t* data, size_t length, uint32_t sequence, uint8_t version) {
  return data && length == HEADER_BYTES && std::equal(MAGIC.begin(), MAGIC.end(), data) &&
         data[4] == version && data[5] == TYPE_ACK && data[6] == 0 && data[7] == 0 &&
         readU32Le(data + 8) == sequence && readU32Le(data + 12) == 0;
}

enum class ReceiveResult { Rejected, Partial, Complete };

// Small metadata only. The caller owns a single, activity-lifetime Note buffer;
// no 2 KB locals or allocation in the receive callback/render loop.
class Reassembler {
 public:
  void reset() { active_ = false; complete_ = false; mask_ = 0; }
  uint8_t version() const { return version_; }
  const uint8_t* source() const { return source_.data(); }
  bool complete() const { return complete_; }

  ReceiveResult accept(const uint8_t* source, const uint8_t* data, size_t length, uint32_t now, Note& note) {
    if (!source || !data || length < HEADER_BYTES || !std::equal(MAGIC.begin(), MAGIC.end(), data) ||
        data[5] != TYPE_NOTE || (data[4] != LEGACY_VERSION && data[4] != CHUNK_VERSION)) return ReceiveResult::Rejected;
    const uint8_t version = data[4], index = data[6], size = data[7];
    const uint32_t seq = readU32Le(data + 8);
    const uint16_t year = readU16Le(data + 12);
    if (!seq || !size || size > CHUNK_BYTES || !validDate(year, data[14], data[15])) return ReceiveResult::Rejected;
    const size_t header = version == LEGACY_VERSION ? HEADER_BYTES : CHUNK_HEADER_BYTES;
    if (length != header + size) return ReceiveResult::Rejected;
    const uint16_t total = version == LEGACY_VERSION ? size : readU16Le(data + 16);
    const uint8_t count = version == LEGACY_VERSION ? 1 : data[18];
    const uint32_t checksum = version == LEGACY_VERSION ? 0 : readU32Le(data + 20);
    if (!total || total > MAX_MESSAGE_BYTES || count != chunkCount(total) || index >= count ||
        (version == LEGACY_VERSION && index != 0) || (version == CHUNK_VERSION && data[19] != 0) ||
        size != std::min(CHUNK_BYTES, static_cast<size_t>(total) - index * CHUNK_BYTES)) return ReceiveResult::Rejected;

    // Completed transfers are immutable until the activity resets us. Partial
    // transfers release their sender/sequence lock after inactivity (wrap-safe).
    if (active_ && !complete_ && now - lastPacketMs_ >= ASSEMBLY_TIMEOUT_MS) reset();
    if (active_ && (seq != note.sequence || version != version_ ||
        !std::equal(source_.begin(), source_.end(), source))) return ReceiveResult::Rejected;
    if (!active_) {
      note.sequence = seq; note.year = year; note.month = data[14]; note.day = data[15];
      note.messageLength = total;
      version_ = version; checksum_ = checksum; mask_ = 0;
      std::copy(source, source + source_.size(), source_.begin());
      active_ = true;
    } else if (total != note.messageLength || checksum != checksum_ || year != note.year ||
               data[14] != note.month || data[15] != note.day) return ReceiveResult::Rejected;
    if (complete_) return ReceiveResult::Complete;

    const size_t offset = index * CHUNK_BYTES;
    const uint16_t bit = static_cast<uint16_t>(1U << index);
    if ((mask_ & bit) && memcmp(note.message.data() + offset, data + header, size) != 0) return ReceiveResult::Rejected;
    memcpy(note.message.data() + offset, data + header, size);
    mask_ |= bit; lastPacketMs_ = now;
    if (mask_ != (1U << count) - 1) return ReceiveResult::Partial;
    const auto* text = reinterpret_cast<const uint8_t*>(note.message.data());
    if ((version == CHUNK_VERSION && crc32(text, total) != checksum_) || !validUtf8(text, total)) {
      reset();
      return ReceiveResult::Rejected;
    }
    note.message[total] = '\0';
    for (size_t i = 0; i < total; ++i)
      if (note.message[i] == '\r' || note.message[i] == '\t') note.message[i] = ' ';
    complete_ = true;
    return ReceiveResult::Complete;
  }

 private:
  std::array<uint8_t, 6> source_{};
  uint32_t checksum_ = 0, lastPacketMs_ = 0;
  uint16_t mask_ = 0;
  uint8_t version_ = LEGACY_VERSION;
  bool active_ = false, complete_ = false;
};
} // namespace sticky_note
