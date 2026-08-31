// ESP8266Audio/libmad exports this macro in the real Cardputer build.
#define VERSION "unrelated audio library version"
#include "StickyNoteProtocol.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace sticky_note;
using Packet = std::vector<uint8_t>;
const uint8_t SOURCE[] = {1, 2, 3, 4, 5, 6};
const uint8_t OTHER[] = {6, 5, 4, 3, 2, 1};

Note makeNote(const std::string& text, uint32_t sequence = 123) {
    Note note;
    note.sequence = sequence; note.year = 2026; note.month = 9; note.day = 20;
    note.messageLength = static_cast<uint16_t>(text.size());
    assert(text.size() <= MAX_MESSAGE_BYTES);
    memcpy(note.message.data(), text.data(), text.size());
    return note;
}

std::vector<Packet> packets(const Note& note) {
    std::vector<Packet> result;
    const uint32_t checksum = crc32(reinterpret_cast<const uint8_t*>(note.message.data()), note.messageLength);
    for (uint8_t index = 0; index < chunkCount(note.messageLength); ++index) {
        Packet packet(MAX_PACKET_BYTES);
        const size_t size = encodePacket(note, index, checksum, packet.data());
        assert(size > 0 && size <= 250);
        packet.resize(size); result.push_back(packet);
    }
    return result;
}

ReceiveResult feed(Reassembler& receiver, Note& out, const Packet& packet, uint32_t now = 100,
                   const uint8_t* source = SOURCE) {
    return receiver.accept(source, packet.data(), packet.size(), now, out);
}

void verify(const Note& input, const Note& output) {
    assert(input.sequence == output.sequence && input.year == output.year);
    assert(input.month == output.month && input.day == output.day);
    assert(input.messageLength == output.messageLength);
    assert(memcmp(input.message.data(), output.message.data(), input.messageLength + 1) == 0);
}

int main() {
    assert(crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926U);
    for (size_t length : {1U, 220U, 221U, 440U, 441U, 2048U}) {
        Note input = makeNote(std::string(length, 'x')), output;
        const auto chunks = packets(input);
        Reassembler receiver;
        for (size_t i = chunks.size(); i-- > 0;) {
            assert(feed(receiver, output, chunks[i]) == (i == 0 ? ReceiveResult::Complete : ReceiveResult::Partial));
            // Identical duplicates neither erase data nor inflate completion.
            assert(feed(receiver, output, chunks[i]) == (i == 0 ? ReceiveResult::Complete : ReceiveResult::Partial));
        }
        verify(input, output);
        const auto ack = makeAck(output.sequence, receiver.version());
        assert(validAck(ack.data(), ack.size(), input.sequence, wireVersion(input)));
        assert(!validAck(ack.data(), ack.size(), input.sequence + 1, wireVersion(input)));
        assert(!validAck(ack.data(), ack.size(), input.sequence, 99));
        // Legacy encoding is byte-for-byte the original 16-byte header format.
        if (length <= 220) {
            assert(chunks[0].size() == 16 + length && chunks[0][4] == 1 && chunks[0][6] == 0);
            assert(readU32Le(chunks[0].data() + 8) == 123 && readU16Le(chunks[0].data() + 12) == 2026);
        }
    }

    Note input = makeNote(std::string(2048, 'a')), output;
    auto chunks = packets(input);
    Reassembler receiver;
    // Loss on the first round, then recovery via repeated whole-message rounds.
    for (size_t i = 0; i < chunks.size(); ++i) {
        if (i == 3) continue;
        assert(feed(receiver, output, chunks[i]) == ReceiveResult::Partial);
    }
    assert(!receiver.complete());
    for (size_t i = 0; i < chunks.size(); ++i) feed(receiver, output, chunks[i]);
    assert(receiver.complete()); verify(input, output);

    receiver.reset();
    assert(feed(receiver, output, chunks[0]) == ReceiveResult::Partial);
    Packet conflict = chunks[0]; conflict.back() ^= 1;
    assert(feed(receiver, output, conflict) == ReceiveResult::Rejected);
    assert(feed(receiver, output, chunks[1], 101, OTHER) == ReceiveResult::Rejected);
    auto another = packets(makeNote(std::string(2048, 'b'), 999));
    assert(feed(receiver, output, another[1], 102) == ReceiveResult::Rejected);
    Packet badDate = chunks[1]; badDate[15] = 19;
    assert(feed(receiver, output, badDate, 103) == ReceiveResult::Rejected);
    // Inactivity releases the incomplete sender lock. Millisecond wrap is safe.
    assert(feed(receiver, output, another[0], 5100, OTHER) == ReceiveResult::Partial);
    receiver.reset();
    assert(feed(receiver, output, chunks[0], 0xffffff00U) == ReceiveResult::Partial);
    assert(feed(receiver, output, another[0], 0x1300U, OTHER) == ReceiveResult::Partial);

    receiver.reset();
    Packet corrupted = chunks[0]; corrupted.back() ^= 1;
    feed(receiver, output, corrupted);
    ReceiveResult last = ReceiveResult::Partial;
    for (size_t i = 1; i < chunks.size(); ++i) last = feed(receiver, output, chunks[i]);
    assert(last == ReceiveResult::Rejected && !receiver.complete());
    for (const auto& p : chunks) feed(receiver, output, p);
    assert(receiver.complete()); verify(input, output);
    // A completed transfer cannot be overwritten during save/ACK grace time.
    assert(feed(receiver, output, another[0], 999999, OTHER) == ReceiveResult::Rejected);
    verify(input, output);

    // UTF-8 codepoints may straddle chunk boundaries; validate only after assembly.
    input = makeNote(std::string(219, 'a') + "\xe2\x82\xac" + std::string(300, 'b'));
    receiver.reset();
    for (const auto& p : packets(input)) last = feed(receiver, output, p);
    assert(last == ReceiveResult::Complete); verify(input, output);
    input = makeNote(std::string(219, 'a') + "\xe2\x82" + std::string(300, 'b'));
    receiver.reset();
    for (const auto& p : packets(input)) last = feed(receiver, output, p);
    assert(last == ReceiveResult::Rejected);

    // Malformed headers, lengths, offsets, totals and dates are rejected safely.
    for (size_t shortLength = 0; shortLength < chunks[0].size(); ++shortLength) {
        receiver.reset();
        assert(receiver.accept(SOURCE, chunks[0].data(), shortLength, 0, output) == ReceiveResult::Rejected);
    }
    for (size_t offset : {4U, 5U, 6U, 7U, 14U, 15U, 18U, 19U}) {
        receiver.reset(); Packet malformed = chunks[0]; malformed[offset] = 255;
        assert(feed(receiver, output, malformed) == ReceiveResult::Rejected);
    }
    receiver.reset(); Packet oversized = chunks[0]; writeU16Le(oversized.data() + 16, 2049);
    assert(feed(receiver, output, oversized) == ReceiveResult::Rejected);
    auto legacy = packets(makeNote("[ ] test"))[0];
    legacy[6] = 1;
    assert(feed(receiver, output, legacy) == ReceiveResult::Rejected);
    assert(receiver.accept(nullptr, nullptr, 0, 0, output) == ReceiveResult::Rejected);
    Note empty; uint8_t packet[MAX_PACKET_BYTES];
    assert(encodePacket(empty, 0, 0, packet) == 0);

    std::cout << "PASS: v1 compatibility, 2 KB roundtrip, loss/reorder/duplicates, CRC, UTF-8, "
                 "sender isolation, timeouts, malformed packets, ACK validation\n";
    std::cout << "sizeof(Note)=" << sizeof(Note) << ", sizeof(Reassembler)=" << sizeof(Reassembler) << '\n';
}
