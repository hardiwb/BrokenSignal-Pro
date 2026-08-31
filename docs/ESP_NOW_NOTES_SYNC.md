# ESP-NOW Notes Sync Plan

This document tracks the plan for sending BrokenSignal Pro daily notes to nearby
ESP32-family devices, especially Xteink X3 e-ink readers.

## Feasibility Summary

ESP-NOW should be technically feasible between BrokenSignal Pro on Cardputer ADV
and Xteink X3-class devices if the X3 firmware exposes a receiver.

Confirmed research:

- Xteink X3 community firmware references identify the device as ESP32-C3 based.
- Xteink X3 product material lists 2.4 GHz WiFi.
- Espressif documents ESP-NOW support for ESP32-C3.

Important caveat: stock Xteink firmware is not enough by itself. The X3 must run
firmware that includes an ESP-NOW receiver and knows how to display or store the
incoming notes.

## Hardware Compatibility

| Device | Role | Notes |
| --- | --- | --- |
| Cardputer ADV | Sender | Existing notes live on SD under `/Notes/YYYY-MM.txt`. |
| Xteink X3 | Receiver | ESP32-C3 and 2.4 GHz WiFi make ESP-NOW plausible. |

ESP-NOW does not need an access point, router, or internet connection. Both
devices do need WiFi radio support and must agree on channel, packet format, and
optional encryption.

## UX Goal

The first useful feature should be small and reliable:

```text
Notes Options
  Send Today via ESP-NOW
```

The sender should package visible notes for the selected day and transmit them
to a paired/broadcast receiver. The receiver can render a simple daily note page
on e-ink.

## Protocol Sketch

Use a small application-level packet on top of ESP-NOW.

```text
magic      "BSPN"
version    1
type       hello | note_chunk | done | ack | error
device     sender id or short name
date       YYYY-MM-DD
seq        chunk number
total      total chunks
payload    UTF-8 note text chunk
crc        lightweight integrity check
```

Keep chunks below the conservative ESP-NOW v1 packet size until both sender and
receiver prove v2 support. Treat 200 bytes of note payload per packet as a safe
starting point.

## Pairing Modes

Start simple:

| Mode | Pros | Cons |
| --- | --- | --- |
| Broadcast send | No setup; good first prototype. | Anyone nearby can receive unencrypted packets. |
| Saved peer MAC | More deliberate and reliable. | Needs pairing UI or config entry. |
| Encrypted peer | Better privacy. | More setup and key management. |

Recommended path:

1. Build unencrypted broadcast prototype with non-sensitive test notes.
2. Add receiver ACK and retry.
3. Add saved peer MAC.
4. Add encryption before recommending real personal notes.

## Firmware Modules

Suggested sender-side layout:

```text
src/module/service/EspNowNotes.*
```

Responsibilities:

- Initialize ESP-NOW only when sending or listening.
- Preserve/restore normal WiFi mode where possible.
- Build note packets from the Notes storage/model API.
- Send packets with sequence numbers.
- Track ACK/retry state outside ESP-NOW callbacks.

Notes app integration:

```text
src/apps/notes/NotesMetadata.cpp
  Send Today via ESP-NOW

src/apps/notes/NotesModel.cpp
  collect selected-day note text
```

## Receiver Requirements

Xteink X3 receiver firmware needs:

- ESP-NOW initialization on ESP32-C3.
- A known channel strategy.
- Packet reassembly.
- Optional ACK response.
- E-ink page renderer for received notes.
- Storage if received notes should survive reboot.

## Risks

- ESP-NOW send callbacks confirm MAC-layer delivery, not application-level save
  or display. Use receiver ACKs for confidence.
- WiFi channel mismatch can break delivery. If the sender is connected to an AP,
  ESP-NOW usually follows that channel.
- Notes can contain personal data. Do not ship broadcast-only note sync as a
  privacy-safe feature.
- E-ink refresh is slow, so receiver UI should update after a full message, not
  on every packet.
- Xteink stock firmware likely cannot receive custom ESP-NOW packets. Custom
  receiver firmware is required.

## Open Questions

- Should BrokenSignal Pro send only today's notes, the selected day, or all
  visible notes under the current Notes filter?
- Should the receiver be broadcast/discoverable, paired by MAC, or configured in
  a file?
- Should this be send-only from Cardputer, or two-way sync later?
- What Xteink firmware target should we support first: CrossPoint/PocketLeaf,
  FreeInk SDK, or a tiny dedicated receiver app?

## First Milestone

Build a throwaway proof of concept:

1. Cardputer sends `hello` and one short note packet by broadcast.
2. Xteink X3 receiver prints packet data to serial.
3. Add an ACK packet from X3 back to Cardputer.
4. Only after ACK works, connect the feature to daily notes and e-ink rendering.
