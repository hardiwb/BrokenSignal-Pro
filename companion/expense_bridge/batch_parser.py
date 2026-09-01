from __future__ import annotations

from dataclasses import dataclass
from datetime import date
import re

from .models import RawExpense
from .parser import PayloadError, _make_record


MAX_BATCH_ENTRIES = 50
_ENTRY_ID = re.compile(r"^[A-Za-z0-9._:-]{1,128}$")
_ENTRY_FIELDS = {"id", "name", "value", "currency", "date"}


@dataclass(frozen=True)
class ExpenseBatch:
    device: str
    entries: list[RawExpense]


def parse_batch_json(payload: object) -> ExpenseBatch:
    if not isinstance(payload, dict):
        raise PayloadError("Request body must be a JSON object")
    if set(payload) != {"device", "entries"}:
        raise PayloadError("Request body must contain only 'device' and 'entries'")

    device = payload["device"]
    if not isinstance(device, str) or not device.strip() or len(device) > 64:
        raise PayloadError("'device' must be a non-empty string up to 64 characters")

    items = payload["entries"]
    if not isinstance(items, list) or not items:
        raise PayloadError("'entries' must be a non-empty array")
    if len(items) > MAX_BATCH_ENTRIES:
        raise PayloadError(f"A batch can contain at most {MAX_BATCH_ENTRIES} entries")

    records: list[RawExpense] = []
    seen_ids: set[str] = set()
    for index, item in enumerate(items):
        location = f"Entry {index + 1}"
        if not isinstance(item, dict) or set(item) != _ENTRY_FIELDS:
            raise PayloadError(
                f"{location}: expected exactly id, name, value, currency, and date"
            )
        entry_id = item["id"]
        if not isinstance(entry_id, str) or not _ENTRY_ID.fullmatch(entry_id):
            raise PayloadError(
                f"{location}: id must use 1-128 letters, numbers, '.', '_', ':', or '-'"
            )
        if entry_id in seen_ids:
            raise PayloadError(f"{location}: duplicate id {entry_id!r} in this batch")
        seen_ids.add(entry_id)

        name = item["name"]
        currency = item["currency"]
        date_text = item["date"]
        value = item["value"]
        if not isinstance(name, str):
            raise PayloadError(f"{location}: name must be a string")
        if len(name.strip()) > 200:
            raise PayloadError(f"{location}: name must be at most 200 characters")
        if not isinstance(currency, str):
            raise PayloadError(f"{location}: currency must be a string")
        if not isinstance(date_text, str):
            raise PayloadError(f"{location}: date must be a YYYY-MM-DD string")
        if isinstance(value, bool) or not isinstance(value, (str, int)):
            raise PayloadError(
                f"{location}: value must be a decimal string or integer, not a JSON float"
            )
        try:
            recorded_date = date.fromisoformat(date_text)
        except ValueError as exc:
            raise PayloadError(
                f"{location}: invalid date {date_text!r}; expected YYYY-MM-DD"
            ) from exc
        if recorded_date.isoformat() != date_text:
            raise PayloadError(
                f"{location}: invalid date {date_text!r}; expected YYYY-MM-DD"
            )

        try:
            record = _make_record(
                recorded_date,
                name,
                str(value),
                currency,
                index + 1,
                entry_id=entry_id,
            )
        except PayloadError as exc:
            detail = str(exc).split(": ", 1)[-1]
            raise PayloadError(f"{location}: {detail}") from exc
        records.append(record)

    return ExpenseBatch(device=device.strip(), entries=records)
