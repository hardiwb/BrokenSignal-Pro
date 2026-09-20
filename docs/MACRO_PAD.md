# Macro Pad JSON files

Macro Pad loads one game definition per JSON file from `/Macros` on the SD
card. The repository includes `sdcard/Macros/helldivers2.json` as a complete
example. Copy the `Macros` folder to the root of the Cardputer SD card, open
Macro Pad, and press `R` to rescan it.

## Format

```json
{
  "version": 1,
  "name": "My Game",
  "activation": {"mouse_button": 5, "mode": "press"},
  "press_ms": 55,
  "gap_ms": 40,
  "macros": [
    {
      "name": "Example action",
      "hotkey": "1",
      "sequence": ["W", "S", "ENTER"]
    },
    {
      "name": "Different modifier",
      "hotkey": "q",
      "hold": ["LEFT_ALT"],
      "press_ms": 80,
      "gap_ms": 50,
      "sequence": ["F1", "RIGHT", "SPACE"]
    }
  ]
}
```

`version`, `name`, and `macros` are required. Each macro requires a unique
single-letter or number `hotkey`, a display `name`, and a non-empty `sequence`.
`default_hold`, `press_ms`, and `gap_ms` provide game-wide defaults. A macro can
override them with `hold`, `press_ms`, or `gap_ms`.

`activation` optionally sends a mouse button before every macro. Buttons 1-5
are supported. Use `"mode": "press"` to click and release the button before
the sequence, or `"mode": "hold"` to hold it until the sequence finishes. An
individual macro can override the game-wide `activation` object.

Supported modifiers are `LEFT_CTRL`, `LEFT_SHIFT`, `LEFT_ALT`, `LEFT_GUI`,
`RIGHT_CTRL`, `RIGHT_SHIFT`, `RIGHT_ALT`, and `RIGHT_GUI`. The shorter names
`CTRL`, `SHIFT`, `ALT`, `GUI`, and `WIN` select their left-side variants.

Supported sequence keys are `A`-`Z`, `0`-`9`, `F1`-`F12`, `ENTER`, `ESC`,
`BACKSPACE`, `TAB`, `SPACE`, `MINUS`, `EQUAL`, `LEFT_BRACKET`, `RIGHT_BRACKET`,
`BACKSLASH`, `SEMICOLON`, `APOSTROPHE`, `GRAVE`, `COMMA`, `PERIOD`, `SLASH`,
`CAPS_LOCK`, `UP`, `DOWN`, `LEFT`, `RIGHT`, and `DELETE`.

Limits protect the Cardputer from oversized or malformed files: each JSON file
may be at most 32 KB and contain up to 36 macros, with up to 32 sequence keys per
macro. Timings are clamped to 10-1000 milliseconds.
