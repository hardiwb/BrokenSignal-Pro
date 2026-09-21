# Notes Notion sync

BrokenSignal Pro can synchronize its SD-card Notes directly with a Notion data
source. The Cardputer uses Notion API version `2026-03-11` and validates the
Notion HTTPS certificate with the ESP32 certificate bundle.

## Required Notion schema

The property names and types are exact:

| Property | Notion type |
| --- | --- |
| `Checkbox` | Checkbox |
| `Entry` | Title |
| `Date` | Date |
| `Category` | Select |
| `ID` | Text (rich text), not Notion's generated Unique ID type |

Connect the Agenda database to the same Notion integration used for Expenses.
The token belongs to the integration and can access every database explicitly
shared with that integration.

## Configure from the Cardputer

1. Open **Control Panel > Notes & Notion** while near a trusted saved WiFi
   network, or select one when prompted.
2. Wait for the setup address. If **Web authentication** is On in Control
   Panel, the Cardputer also displays a one-time code.
3. On another device on the same network, open
   `http://brokensignal.local/`. Use the numeric address shown on the Cardputer
   if mDNS is unavailable.
4. When Web authentication is On, sign in as `admin` with the displayed
   six-digit one-time code. When it is Off, no sign-in is required.
5. Enter the Notion integration token and either the Agenda database ID or its
   Notion URL, then save.

The setup server closes after five minutes. It never returns the saved token to
the browser; leaving the token field blank keeps the current token. The token
and database ID are stored in the ESP32 Preferences partition, not in Git or on
the SD card. **The local setup page uses HTTP, so configure it only on a trusted
network.** Device flash is not encrypted by this project, so anyone with
physical flash access may be able to recover the token.

When WiFi is disconnected, opening Notes setup or starting a Notes sync first
tries the saved WiFi credentials. If that fails or no network is saved, the
normal WiFi picker opens. After a successful connection, the requested setup or
sync operation resumes automatically; cancelling the picker cancels only that
pending operation.

The known Agenda database ID is:

```text
3d66286d-2db7-8014-af3f-d5e4de4a6c17
```

The `v=` value in a Notion URL is a view ID and is not used. On the first sync,
the Cardputer retrieves the database and caches its single data-source ID. If
the database ID is changed in the setup page, that cached ID is cleared.

## Synchronize

Open Notes and choose **Options > Sync Notion**. Playback stops to leave enough
memory for HTTPS and JSON processing. The first sync:

Press `W` on a selected Cardputer note to toggle between Personal and Work, or
press `A` to toggle between Personal and Art. Work notes display `#w` and Art
notes display `#A`; the next sync writes `Personal`, `Work`, or `Art` to
Notion's `Category` property.

- assigns stable IDs to legacy Cardputer notes;
- assigns an ID to Notion rows whose `ID` property is blank;
- imports Notion-only rows to the monthly SD files;
- uploads Cardputer-only rows;
- establishes a content fingerprint for later conflict detection.

On later syncs, a change made on only one side is copied to the other side. If
both versions changed since the previous successful sync, the Cardputer copy
wins and the serial log reports a conflict count. Deleting a row is not yet
propagated: a row missing on one side is restored from the other side. Up to 200
local and 200 remote notes are accepted, and Notion results are fetched in
pages of 5 and parsed directly from the HTTPS stream to control memory use.

Monthly note files are replaced transactionally through `.tmp` and `.bak`
files only after all required Notion requests succeed. A failed API request
therefore leaves the original SD records untouched.

## Transport and memory behavior

Notion query responses are deliberately limited to five rows per request. The
firmware requests HTTP/1.0 identity framing and passes the HTTPS response stream
directly to ArduinoJson. Do not replace this with `HTTPClient::getString()` or
another full-body `String` without re-evaluating peak heap use.

This design addresses a failure observed with a 61-line monthly Notes file. A
chunked Notion response grew to 7,182 bytes before `HTTPClient::getString()`
silently stopped extending its buffer on a fragmented, no-PSRAM ESP32-S3 heap.
ArduinoJson then correctly reported `IncompleteInput` because it received only
the beginning of an otherwise valid JSON document. The note file itself was not
malformed, and reducing or rewriting its lines was not the appropriate repair.

Streaming avoids holding both the complete encoded response and the parsed JSON
document in memory at the same time. This applies to database discovery, query,
page creation, page updates, and Notion error responses.

## Troubleshooting

### `Notion JSON IncompleteInput`

First install firmware containing the streaming response implementation in
`NotesNotionSync.cpp`, then retry the sync on a stable WiFi connection. Older
firmware may append a byte count such as `(7182 bytes)`; that number describes
the truncated HTTP response held in memory, not the size or validity of the
monthly Notes file.

If current firmware still reports `IncompleteInput`, capture the full serial
message and verify whether the network disconnected or timed out during the
request. Keep the query page size at five while diagnosing the transport. Do
not delete Notes files, reset the Notion database, or increase the page size as
a first response.

### Other Notion errors

Errors beginning with `Notion 400`, `401`, `403`, or `404` are complete API
responses rather than JSON truncation. Check the required property names and
types, integration token, database sharing permissions, and configured database
ID. A connection error with a negative status indicates HTTPS or WiFi transport
failure before a valid Notion response was received.
