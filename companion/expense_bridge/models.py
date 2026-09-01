from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from decimal import Decimal
from hashlib import sha256


CATEGORIES = (
    "Food",
    "Groceries",
    "Lifestyle",
    "Living",
    "Personal Care",
    "Transport",
    "Investment",
    "Recurring",
)


@dataclass(frozen=True)
class RawExpense:
    recorded_date: date
    name: str
    amount: Decimal
    amount_text: str
    currency: str
    entry_id: str | None = None

    @property
    def source_id(self) -> str:
        if self.entry_id is not None:
            return self.entry_id
        canonical = (
            f"{self.recorded_date.isoformat()}|{self.name}|"
            f"{self.amount_text}|{self.currency}"
        )
        return sha256(canonical.encode("utf-8")).hexdigest()

    @property
    def fingerprint(self) -> str:
        canonical = (
            f"{self.recorded_date.isoformat()}|{self.name}|"
            f"{self.amount_text}|{self.currency}"
        )
        return sha256(canonical.encode("utf-8")).hexdigest()


@dataclass(frozen=True)
class Classification:
    normalized_title: str
    emoji: str
    category: str

    def __post_init__(self) -> None:
        if self.category not in CATEGORIES:
            raise ValueError(f"Unsupported expense category: {self.category}")
        if not self.normalized_title.strip():
            raise ValueError("Normalized title cannot be empty")
        if not self.emoji.strip() or any(ch.isspace() for ch in self.emoji):
            raise ValueError("Emoji must be a non-empty token")

    @property
    def title(self) -> str:
        return f"{self.emoji} {self.normalized_title.strip()}"


@dataclass(frozen=True)
class CurrencyResult:
    status: str
    idr_amount: int | None
    original_amount: str
    original_currency: str
    rate: str | None = None
    rate_source: str | None = None


@dataclass(frozen=True)
class NormalizedExpense:
    title: str
    nominal: int
    category: str
    recorded_date: date

    def __post_init__(self) -> None:
        if not self.title.strip():
            raise ValueError("Normalized expense title cannot be empty")
        if self.nominal <= 0:
            raise ValueError("Normalized IDR nominal must be greater than zero")
        if self.category not in CATEGORIES:
            raise ValueError(f"Unsupported expense category: {self.category}")

    def to_dict(self) -> dict:
        return {
            "Title": self.title,
            "Nominal": self.nominal,
            "Jenis pengeluaran": self.category,
            "Tanggal": self.recorded_date.isoformat(),
        }

    @classmethod
    def from_dict(cls, value: dict) -> "NormalizedExpense":
        if set(value) != {"Title", "Nominal", "Jenis pengeluaran", "Tanggal"}:
            raise ValueError("Stored normalized expense has an invalid schema")
        if isinstance(value["Nominal"], bool) or not isinstance(value["Nominal"], int):
            raise ValueError("Stored normalized nominal must be an integer")
        return cls(
            title=str(value["Title"]),
            nominal=value["Nominal"],
            category=str(value["Jenis pengeluaran"]),
            recorded_date=date.fromisoformat(str(value["Tanggal"])),
        )
