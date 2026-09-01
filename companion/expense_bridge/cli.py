from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from .ai_processor import OfflinePreviewProcessor, OpenAIProcessor
from .currency import PrototypeCurrencyConverter
from .parser import PayloadError
from .pipeline import ExpensePipeline


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Parse and classify a pasted Cardputer expense QR payload."
    )
    parser.add_argument("--input", type=Path, help="Read payload from a UTF-8 file")
    parser.add_argument(
        "--offline-preview",
        action="store_true",
        help="Use deterministic preview rules instead of the OpenAI API",
    )
    return parser.parse_args()


def _read_payload(path: Path | None) -> str:
    if path:
        return path.read_text(encoding="utf-8")
    if sys.stdin.isatty():
        print("Paste the Cardputer payload, then press Ctrl+Z and Enter (Windows):")
    return sys.stdin.read()


def main() -> int:
    args = _arguments()
    try:
        ai = OfflinePreviewProcessor() if args.offline_preview else OpenAIProcessor()
        result = ExpensePipeline(ai, PrototypeCurrencyConverter()).process(
            _read_payload(args.input)
        )
    except (OSError, PayloadError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if hasattr(sys.stdout, "buffer"):
        sys.stdout.buffer.write(rendered.encode("utf-8"))
    else:
        sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
