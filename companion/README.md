# Cardputer Expense Companion

The companion receives raw Cardputer expenses, validates them, asks Codex to
normalize only the title/emoji/category, and either previews the result locally
or creates one Notion page per expense.

```text
Cardputer / pasted QR payload
              |
       input transport
              |
           parser
              |
       Codex classifier
              |
      currency converter
              |
     SQLite sync ledger
        /           \
 local preview    Notion (opt-in)
```

Notion is never called by the AI. Authentication, database selection, property
names, and the Account relation are controlled by trusted PC configuration.

## Current capabilities

- Paste a QR payload into a local CLI for inspection.
- Receive a selected Cardputer expense over trusted local Wi-Fi.
- Authenticate Cardputer requests with a private shared token.
- Classify titles through the logged-in Codex CLI without an OpenAI API key.
- Preserve recorded dates and IDR amounts outside the model.
- Preview locally or submit one page per expense to Notion.
- Record confirmed Notion page IDs in SQLite to prevent repeat uploads.
- Retry Codex fallback and failed Notion requests.
- Preserve non-IDR records without guessing an exchange rate; these remain
  pending until a real currency adapter is implemented.

## Requirements

- Windows PowerShell.
- Python 3.11+; the existing PlatformIO Python is supported.
- Codex CLI installed and authenticated with `codex login`.
- Cardputer and PC connected to the same trusted private network for LAN sync.
- A Notion internal integration with Insert Content access for real submission.

Verify Codex before starting the bridge:

```powershell
codex login status
```

## Recommended Windows setup

Open PowerShell in the repository and enter the companion directory:

```powershell
cd .\companion
```

### 1. Configure Notion

Run the interactive generator:

```powershell
.\configure-notion.bat
```

It prompts for three values:

| Prompt | Required value |
| --- | --- |
| Notion integration token | The secret token, commonly beginning with `ntn_` |
| Notion expense database ID | The database receiving new expense pages |
| Related Account page ID | Optional specific Account record attached to every expense |

Press Enter at the relation prompt if the expense database has no `Account`
relation property. In that case the generated template and every page request
omit `Account` completely.

When used, the related Account value is a page ID, not the ID of the database
containing Account records. In a page request it appears here:

```json
{
  "Account": {
    "relation": [
      {
        "id": "RELATED_ACCOUNT_PAGE_ID"
      }
    ]
  }
}
```

Every user who enables the relation must enter their own related page ID. No
personal relation ID is compiled into the shared companion.

The generator creates:

```text
data/notion.env
data/notion-request-template.json
```

Both files are under the Git-ignored `companion/data/` directory. The token is
hidden while typing, but it is stored as plaintext in `notion.env`; keep the PC
account and file private.

To replace an existing configuration:

```powershell
.\configure-notion.bat -Force
```

The server validates the generated JSON before enabling Notion. It rejects
changed property names, missing placeholders, a missing trailing space in
`"Date "`, or an enabled relation ID that differs from `notion.env`.

### 2. Start the bridge

The main BAT launcher currently starts the intended end-to-end mode: LAN,
Codex, and Notion submission enabled.

```powershell
.\start-expense-server.bat
```

This command can create real Notion pages. Startup must show:

```text
LAN mode: enabled (trusted private networks only)
Notion submission: ENABLED
Classifier: codex
```

The launcher generates `data/cardputer-token.txt` on first run and prints the
PC's candidate LAN addresses. Choose the address on the same subnet as the
Cardputer. If Windows Firewall prompts, allow Python on private networks only.

To use another port:

```powershell
.\start-expense-server.bat -Port 9000
```

### 3. Configure the Cardputer SD card

Create `/Expenses/sync.cfg` on the microSD card:

```ini
server=http://192.168.0.65:8765
token=PASTE_CARDPUTER_TOKEN_HERE
```

Replace the address with the LAN address printed by the launcher. Read the
Cardputer token on the PC with:

```powershell
Get-Content .\data\cardputer-token.txt
```

The Cardputer token authenticates only the local bridge. It is not the Notion
token and must never be replaced with the Notion token.

### 4. Upload an expense

On the Cardputer:

1. Open Expenses and select an entry.
2. Open Options.
3. Choose `Sync Pending`.

Every unmarked expense on the displayed date is sent in one batch. The
Cardputer marks only entries explicitly accepted by the PC, leaving failures
available for retry.

If Wi-Fi is disconnected, the Cardputer shows its connection screen and first
tries the saved network. If network selection or a password is needed, it opens
the Wi-Fi menu. After a successful connection, the pending expense upload
resumes automatically. Leaving the Wi-Fi menu cancels that pending attempt.

`Synced to Notion` appears only after Notion returns a created page ID. A failed
request remains pending and can be retried by uploading the same entry again.

## Safe preview modes

The BAT launcher above enables real Notion writes. Use the PowerShell launcher
directly when you want a preview:

| Command | Network | Classifier | Notion |
| --- | --- | --- | --- |
| `.\start-expense-server.bat` | LAN | Codex | Enabled |
| `.\start-expense-server.ps1 -Lan` | LAN | Codex | Disabled |
| `.\start-expense-server.ps1` | Localhost | Codex | Disabled |
| `.\start-expense-server.ps1 -Lan -Offline` | LAN | Deterministic preview | Disabled |

`-Offline` cannot be combined with `-EnableNotion`. If Codex fails while
running normally, deterministic rules produce only a fallback preview; that
record is not considered Notion-ready and is retried later.

Check the active mode:

```powershell
Invoke-RestMethod http://127.0.0.1:8765/health | ConvertTo-Json -Depth 4
```

A real-sync server reports:

```json
{
  "status": "ok",
  "mode": "notion-sync",
  "notion_enabled": true
}
```

## Notion database contract

The normalized record contains only:

```json
{
  "Title": "🍗 Ayam Goreng",
  "Nominal": 20000,
  "Jenis pengeluaran": "Food",
  "Tanggal": "2026-09-01"
}
```

The trusted Notion layer maps it as follows:

| Normalized field | Notion property path |
| --- | --- |
| `Title` | `properties.Name.title[0].text.content` |
| `Nominal` | `properties.Expense.number` |
| `Jenis pengeluaran` | `properties["Type of Expense"].select.name` |
| `Tanggal` | `properties["Date "].date.start` |

`"Date "` intentionally has a trailing space. When configured, the generated
template also sets `properties.Account.relation[0].id` from the related Account
page ID. Without a relation ID, the `Account` property is omitted. The model
cannot generate or alter any of these values.

The integration uses `POST https://api.notion.com/v1/pages`, Bearer
authentication, and Notion API version `2026-03-11`.

The Notion integration must have:

- Insert Content capability;
- access to the expense database;
- access to the configured Account relation target, if that optional relation
  is enabled.

## Classification contract

Codex receives only each raw title and a temporary batch index. It does not
receive amounts, currencies, dates, stable entry IDs, Notion credentials,
database IDs, property names, or relation IDs.

`Jenis pengeluaran` is always exactly one of:

```text
Food
Groceries
Lifestyle
Living
Personal Care
Transport
Investment
Recurring
```

IDR values pass through unchanged. The Cardputer's recorded date is always
preserved. Non-IDR records stop before Notion until a real exchange-rate source
is implemented; neither Codex nor offline rules may guess a rate.

## Duplicate and retry behavior

The SQLite ledger is stored at `data/sync-state.sqlite3`.

- `processed`: a valid normalized preview exists locally.
- `synced`: Notion returned a page ID, which is stored as the receipt.
- `failed`: parsing, classification, or currency processing failed.
- A Notion failure keeps the local preview retryable.
- Reusing the same entry ID with different expense data is rejected.
- A confirmed `synced` entry is not uploaded again.

Existing monthly Cardputer expense files are migrated automatically when
opened so each entry has a stable private ID. QR sharing remains available.

## Manual QR-payload preview

The standalone CLI is separate from the LAN server. For a no-network preview:

```powershell
python -m expense_bridge `
  --offline-preview `
  --input sample-payload.txt
```

For direct OpenAI API classification instead of Codex CLI, set
`OPENAI_API_KEY` and omit `--offline-preview`. This API key is unrelated to the
Notion integration token and is not required by the normal Codex server path.

Accepted pasted format:

```text
2026-09-01
Ayam Goreng|20000|IDR
Es Doger|6000|IDR
```

## HTTP batch contract

The Cardputer sends:

```json
{
  "device": "cardputer",
  "entries": [
    {
      "id": "20260901-001",
      "name": "Ayam Goreng",
      "value": "20000",
      "currency": "IDR",
      "date": "2026-09-01"
    }
  ]
}
```

Requests use `POST /expenses/sync`, `Content-Type: application/json`, and the
`X-Cardputer-Token` header. A batch may contain up to 50 records and 64 KiB.
Decimal values are strings so values such as `3.50` remain exact; JSON floats
are rejected.

## Troubleshooting

### `Missing /Expenses/sync.cfg`

Create `Expenses/sync.cfg` at the root of the Cardputer microSD card, not in the
PC project.

### `PC connection failed -1`

- Start the bridge in LAN mode.
- Use the PC's private LAN address, never `127.0.0.1` or `0.0.0.0`, in
  `sync.cfg`.
- Confirm the Cardputer and PC are on the same router/subnet.
- Allow Python through Windows Firewall on private networks.
- Test `http://PC_LAN_IP:8765/health` from another device on the LAN.

Ethernet on the PC and Wi-Fi on the Cardputer is valid when the router permits
devices to communicate. Guest-network client isolation can block it.

### HTTP 401 or `Invalid or missing token`

Copy the complete value from `data/cardputer-token.txt` into the SD card's
`sync.cfg`. Do not use the Notion token there.

### Notion HTTP 400

Check the exact database property names and types. In particular, this project
expects `Name`, `Expense`, `Type of Expense`, and `Date `. It expects `Account`
only when a related Account page ID was configured.

### Notion HTTP 403 or 404

Confirm the integration has Insert Content capability and access to the expense
database. If Account relation is enabled, it also needs access to that relation
target. Verify that the expense database ID is not the related Account page ID.

### Template validation error

Run the generator again instead of manually repairing the template:

```powershell
.\configure-notion.bat -Force
```

## Files and responsibilities

```text
expense_bridge/
|-- batch_parser.py       Cardputer JSON validation
|-- parser.py             QR/pasted payload parsing
|-- ai_processor.py       Classifier boundary and offline preview
|-- codex_processor.py    Isolated Codex CLI classifier
|-- currency.py           Currency boundary; IDR pass-through only
|-- pipeline.py           Classification and normalization composition
|-- notion_sync.py        Validated Notion request builder/client
|-- sync_state.py         SQLite duplicate/retry ledger
|-- service.py            Preview and sync orchestration
`-- server.py             Authenticated HTTP transport
```

Transport remains independent of classification and Notion, so the current QR,
pasted input, and local Wi-Fi paths use the same processing pipeline.

## Tests

```powershell
python -m unittest discover -s tests -v
```

The tests cover parsing, normalization, classifier isolation, exact Notion
mapping, per-user relation configuration, template validation, SQLite migration,
retry behavior, and duplicate prevention. They do not create a live Notion
page.

## Future work

1. Add a real exchange-rate adapter with stored rate/source metadata.
2. Add optional batch/background upload after selected-entry sync is proven.
