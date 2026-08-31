# CrossInk Notes Sender

BrokenSignal Pro sends the day currently shown in Notes (or the selected entry's
day in Month view) to a CrossInk Xteink running the Sticky Notes receiver.

## Use

1. On Xteink, open **Menu > Sticky Notes > Receive Note**.
2. On Cardputer ADV, open Notes and navigate to the required day.
3. In **Day** view, use **`,`** / **`/`** (left/right) to browse to a past or future
   day. In **Month** view, select an entry on the date you want to send.
   Press **S** to send unchecked entries for that day. Use **Shift+S** to
   include crossed entries. Other dates in Month view are not included.
4. Wait for **SENT**. A timeout means no matching acknowledgement
   arrived.

Each included entry is sent on its own newline with a `[ ]` or `[x]` status
marker. CrossInk renders those entries as checklist rows. The final UTF-8
message must be at most 2048 bytes. Oversized days are rejected instead of being
truncated.

Update **both** devices for larger transfers. Notes up to 220 bytes still use
the legacy format and work with older receivers. Larger notes use automatic
chunking; an old receiver will ignore them and the sender will time out.
CrossInk still produces one sleep-screen image, not multiple pages. Larger
notes use tighter spacing; remaining rows are indicated by **More**, and long
wrapped text can be ellipsized. The 2 KB transfer limit is not a guarantee that
all text fits visibly at the selected font size. Original notes remain on the
Cardputer SD card.

Header feedback uses short labels so the whole message fits: **SENDING**,
**SENT**, **TIMEOUT**, **TOO LONG** (over 2048 bytes including checkbox markers
and newlines), **ALL DONE** (use Shift+S to include completed entries), and
**RADIO ON** (stop web radio before sending).

Notes uses the configured timezone, matching the header clock. Set the timezone
to **+7** for WIB; system/NTP and RTC time remain UTC. The Notes date and loaded
month refresh automatically at local midnight. Future dates are sent as-is;
they display immediately on Xteink, rather than scheduling a later delivery.

Web radio must be stopped before sending because it owns the Wi-Fi channel. If
Wi-Fi was connected for another purpose, the service reconnects it after the
ESP-NOW exchange. Deep sleep is suppressed only while sending or restoring
Wi-Fi.

## Protocol

The sender uses unencrypted ESP-NOW broadcast on channel 1. Multi-byte values
are little-endian.

| Offset | Bytes | Value |
| ---: | ---: | --- |
| 0 | 4 | ASCII `CINT` |
| 4 | 1 | Protocol version `1` |
| 5 | 1 | Packet type `1` (note) |
| 6 | 1 | Reserved `0` |
| 7 | 1 | UTF-8 message length, 1-220 bytes |
| 8 | 4 | Non-zero random sequence number |
| 12 | 2 | Year, 2024-2099 |
| 14 | 1 | Month, 1-12 |
| 15 | 1 | Day |
| 16 | N | Newline-delimited UTF-8 checklist entries without a trailing NUL |

The sender repeats a legacy packet every 350 ms for up to 30 seconds. The Xteink
returns a 16-byte `CINT` packet with type `2` and the matching sequence only
after it has rendered and saved the sleep image.

### Version 2: chunked messages

The first 16 bytes retain the date and sequence positions above. Version is
`2`; byte 6 is the zero-based chunk index and byte 7 is this chunk's length.

| Offset | Bytes | Value |
| ---: | ---: | --- |
| 16 | 2 | Total UTF-8 message bytes, at most 2048 |
| 18 | 1 | Chunk count: ceil(total / 220), at most 10 |
| 19 | 1 | Reserved `0` |
| 20 | 4 | CRC-32/ISO-HDLC of the full unmodified message |
| 24 | N | Chunk payload, 220 bytes except the final chunk |

Packets are at most 244 bytes, so both current radio stacks can carry them.
CRC uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and a
final XOR of `0xFFFFFFFF`. A UTF-8 character may cross a chunk boundary.

The sender cycles through all chunks at 100 ms intervals for up to 30 seconds.
The receiver accepts reordered and identical duplicate chunks, locks a partial
transfer to one source MAC and sequence, and releases that lock after five
seconds without an accepted packet. It checks total length, date, CRC, and full
UTF-8 before rendering or writing storage. Partial/corrupt transfers do not
replace the previous image. No per-chunk ACK is required.

The final ACK has the same 16-byte layout as v1 but version `2`. Reserved bytes
are zero. Only this final render/save ACK counts as success. Xteink repeats it
every 350 ms during a two-second grace period before leaving the receiver.

## Pairing and Privacy

There is no user pairing in this version. The service registers a temporary
broadcast peer internally. Any compatible receiver listening on channel 1 can
read and save the unencrypted message, and multiple listening receivers may all
save it.
CRC detects corruption; it does not authenticate the sender or prevent forged
ACKs. Source/sequence locking is for transfer integrity, not security pairing.

## Source Layout

- `src/module/service/EspNowNotes.*`: packet, channel, retry, ACK, teardown,
  and Wi-Fi restoration state machine.
- `src/module/service/StickyNoteProtocol.h`: shared wire encoder, validation,
  CRC and receiver assembly contract (mirrored in CrossInk).
- `src/apps/notes/NotesModel.cpp`: visible-day composition and status feedback.
- `src/apps/notes/NotesInput.cpp`: the `S` shortcut.

## Verification

Run `./tools/test_notes_dates.ps1` with a host `g++` compiler on PATH. The harness
compiles the actual clock, date-navigation, and note-composition functions with
fake time and transport. It checks GMT+7 Monday at 05:00, date boundaries,
future-day selection, completed-note filtering, and payload limits.

Run `./tools/test_sticky_chunks.ps1` to check both protocol copies, including
2 KB roundtrips, legacy compatibility, losses, duplicates, reordering, CRC,
UTF-8 split boundaries, malformed packets, sender isolation and timeout wrap.

After flashing Cardputer ADV, set timezone to +7 and verify Notes and the clock
agree before 07:00. Leave Notes open across midnight to verify its date updates.
Send tomorrow's notes from Day view, then select a future entry in Month view
and send again. Start **Receive Note** on Xteink for each transfer and check its
displayed date, checklist, and the Cardputer's **SENT** acknowledgement.
Also test a day above 220 bytes, then interrupt a larger transfer: the previous
Xteink sleep image must remain intact until a whole transfer is validated.
