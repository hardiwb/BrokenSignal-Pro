# Macro Pad JSON files

Macro Pad loads one game definition per JSON file from `/Macros` on the SD
card. The repository includes `sdcard/Macros/helldivers2.json` as a complete
example. Copy the `Macros` folder to the root of the Cardputer SD card, open
Macro Pad, and press `R` to rescan it.

## Catalog and key bindings (version 2)

Version 2 keeps the complete macro catalog separate from the Cardputer key
assignments. Change only `bindings` when changing a loadout. Every key from
`0`-`9` and `A`-`Z` can be assigned to one catalog entry.

```json
{
  "version": 2,
  "name": "My Game",
  "activation": {"mouse_button": 5, "mode": "press"},
  "press_ms": 55,
  "gap_ms": 40,
  "macros": [
    {
      "id": "first_action",
      "name": "First action",
      "sequence": ["UP", "DOWN", "RIGHT"]
    },
    {
      "id": "ctrl_action",
      "name": "Action with Ctrl",
      "hold": ["CTRL"],
      "sequence": ["UP", "LEFT", "DOWN"]
    }
  ],
  "bindings": {
    "1": "first_action",
    "Q": "ctrl_action"
  }
}
```

`version`, `name`, `macros`, and `bindings` are required. Each catalog entry
requires a unique `id`, a display `name`, and a non-empty `sequence`. IDs may
contain lowercase letters, numbers, underscores, and hyphens. Binding values
must match catalog IDs. Binding keys are case-insensitive.

## Editing bindings on the Cardputer

After selecting a game and Bluetooth device, the main list contains only the
configured bindings. Open the Macro Pad Options menu to edit a version 2
profile:

- **Add Macro** browses unassigned catalog entries. Select one, then press the
  desired free hotkey.
- **Change Key** assigns a different `0`-`9` or `A`-`Z` key to the selected
  macro.
- **Delete Macro** removes the selected binding. The macro remains in the
  catalog and can be added again later.
- **Move Up** and **Move Down** change the displayed binding order and save the
  new order.
- **Starter Input** selects None, left or right Ctrl held through the sequence,
  or Mouse Button 1-5 clicked before the sequence.
- Press a configured hotkey or `Enter` to run a macro.

Every confirmed edit is saved immediately to the selected JSON file. Macro Pad
writes a temporary file, backs up the original, and restores the backup if the
replacement fails. Version 1 profiles remain runnable but are read-only.

The included Helldivers 2 profile uses Mouse Button 5 in `press` mode, so each
macro clicks and releases Mouse Button 5 before sending its arrow sequence.
It does not hold the mouse button during the sequence.

Its catalog contains the current loadout stratagem reference plus common
mission actions. Entries are alphabetized and use family-first display names
such as `Pack - Jump`, `Pack - Warp`, `Sentry - Gatling`, and
`Sentry - Machine Gun`. Existing IDs remain stable so catalog renaming and
reordering do not invalidate saved bindings.

Version 1 remains supported. In that format, each macro has its own `hotkey`
instead of using a separate `bindings` object.

`activation` optionally sends a starter input before every macro. Mouse buttons
1-5 are supported with `{"mouse_button": 5, "mode": "press"}`. Keyboard
modifiers use forms such as `{"key": "CTRL", "mode": "hold"}`. Press mode
releases the starter before the sequence; hold mode keeps it active through the
sequence. An individual macro can override the game-wide `activation` object.

Supported modifiers are `LEFT_CTRL`, `LEFT_SHIFT`, `LEFT_ALT`, `LEFT_GUI`,
`RIGHT_CTRL`, `RIGHT_SHIFT`, `RIGHT_ALT`, and `RIGHT_GUI`. The shorter names
`CTRL`, `SHIFT`, `ALT`, `GUI`, and `WIN` select their left-side variants.

Supported sequence keys are `A`-`Z`, `0`-`9`, `F1`-`F12`, `ENTER`, `ESC`,
`BACKSPACE`, `TAB`, `SPACE`, `MINUS`, `EQUAL`, `LEFT_BRACKET`, `RIGHT_BRACKET`,
`BACKSLASH`, `SEMICOLON`, `APOSTROPHE`, `GRAVE`, `COMMA`, `PERIOD`, `SLASH`,
`CAPS_LOCK`, `UP`, `DOWN`, `LEFT`, `RIGHT`, and `DELETE`.

Limits protect the Cardputer from oversized or malformed files: each JSON file
may be at most 32 KB. Version 2 supports up to 128 catalog entries and 36 active
bindings. A sequence may contain up to 32 keys. Timings are clamped to 10-1000
milliseconds.
