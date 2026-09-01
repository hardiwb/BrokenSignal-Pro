from __future__ import annotations

from abc import ABC, abstractmethod

from .models import CurrencyResult, RawExpense


class CurrencyConverter(ABC):
    @abstractmethod
    def convert(self, record: RawExpense) -> CurrencyResult:
        raise NotImplementedError


class PrototypeCurrencyConverter(CurrencyConverter):
    """Pass IDR through and leave every other currency pending a real rate source."""

    def convert(self, record: RawExpense) -> CurrencyResult:
        if record.currency == "IDR":
            return CurrencyResult(
                status="ready",
                idr_amount=int(record.amount),
                original_amount=record.amount_text,
                original_currency=record.currency,
            )
        return CurrencyResult(
            status="conversion_required",
            idr_amount=None,
            original_amount=record.amount_text,
            original_currency=record.currency,
        )
