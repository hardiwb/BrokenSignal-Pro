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
2. Wait for the setup address and one-time code.
3. On another device on the same network, open
   `http://brokensignal.local/`. Use the numeric address shown on the Cardputer
   if mDNS is unavailable.
4. Sign in as `admin` with the displayed six-digit one-time code.
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

Press `W` on a selected Cardputer note to toggle its category between Personal
and Work. Work notes display a `<w>` prefix; the next sync writes `Work` or
`Personal` to Notion's `Category` property.

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
pages of 25 to control memory use.

Monthly note files are replaced transactionally through `.tmp` and `.bak`
files only after all required Notion requests succeed. A failed API request
therefore leaves the original SD records untouched.
