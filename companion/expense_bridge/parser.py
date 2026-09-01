from __future__ import annotations

from datetime import date
from decimal import Decimal, InvalidOperation
import re

from .models import RawExpense


_CURRENCY = re.compile(r"^[A-Z]{3}$")


class PayloadError(ValueError):
    pass


def _parse_date(value: str, line_number: int) -> date:
    try:
        parsed = date.fromisoformat(value)
    except ValueError as exc:
        raise PayloadError(
            f"Line {line_number}: invalid date {value!r}; expected YYYY-MM-DD"
        ) from exc
    if parsed.isoformat() != value:
        raise PayloadError(
            f"Line {line_number}: invalid date {value!r}; expected YYYY-MM-DD"
        )
    return parsed


def _make_record(
    recorded_date: date,
    name: str,
    amount_text: str,
    currency: str,
    line_number: int,
    entry_id: str | None = None,
) -> RawExpense:
    name = name.strip()
    amount_text = amount_text.strip()
    currency = currency.strip().upper()
    if not name:
        raise PayloadError(f"Line {line_number}: expense name cannot be empty")
    try:
        amount = Decimal(amount_text)
    except InvalidOperation as exc:
        raise PayloadError(f"Line {line_number}: invalid amount {amount_text!r}") from exc
    if not amount.is_finite() or amount <= 0:
        raise PayloadError(f"Line {line_number}: amount must be greater than zero")
    if currency == "IDR" and amount != amount.to_integral_value():
        raise PayloadError(f"Line {line_number}: IDR amount must be a whole number")
    if not _CURRENCY.fullmatch(currency):
        raise PayloadError(
            f"Line {line_number}: currency must be a three-letter code"
        )
    return RawExpense(recorded_date, name, amount, amount_text, currency, entry_id)


def parse_payload(payload: str) -> list[RawExpense]:
    """Parse current firmware QR rows and date-header prototype rows."""
    records: list[RawExpense] = []
    active_date: date | None = None
    lines = payload.lstrip("\ufeff").splitlines()

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line:
            continue
        parts = [part.strip() for part in line.split("|")]
        if len(parts) == 1:
            active_date = _parse_date(parts[0], line_number)
            continue
        if len(parts) == 5:
            recorded_date = _parse_date(parts[0], line_number)
            if parts[1].upper() not in {"X", "-"}:
                raise PayloadError(
                    f"Line {line_number}: firmware shared flag must be X or -"
                )
            records.append(
                _make_record(recorded_date, parts[2], parts[3], parts[4], line_number)
            )
            continue
        if len(parts) == 3:
            if active_date is None:
                raise PayloadError(
                    f"Line {line_number}: three-field record needs a date header first"
                )
            records.append(
                _make_record(active_date, parts[0], parts[1], parts[2], line_number)
            )
            continue
        raise PayloadError(
            f"Line {line_number}: expected a date header, name|amount|currency, "
            "or date|flag|name|amount|currency"
        )

    if not records:
        raise PayloadError("Payload contains no expense records")
    return records
