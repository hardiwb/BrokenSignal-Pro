from __future__ import annotations

from .ai_processor import AIProcessor
from .currency import CurrencyConverter
from .models import Classification, NormalizedExpense, RawExpense
from .parser import parse_payload


class ExpensePipeline:
    def __init__(self, ai: AIProcessor, currency: CurrencyConverter) -> None:
        self.ai = ai
        self.currency = currency

    def process(self, payload: str) -> list[dict]:
        raw_records = parse_payload(payload)
        return [record.to_dict() for record in self.process_records(raw_records)]

    def classify(self, raw_records: list[RawExpense]) -> list[Classification]:
        classifications = self.ai.classify(raw_records)
        if len(classifications) != len(raw_records):
            raise RuntimeError("Classifier record count does not match the input")
        return classifications

    def normalize(
        self, raw: RawExpense, classification: Classification
    ) -> NormalizedExpense:
        converted = self.currency.convert(raw)
        if converted.idr_amount is None:
            raise RuntimeError(
                "Currency conversion required before normalization: "
                f"{raw.amount_text} {raw.currency} for {raw.name!r}"
            )
        return NormalizedExpense(
            title=classification.title,
            nominal=converted.idr_amount,
            category=classification.category,
            recorded_date=raw.recorded_date,
        )

    def process_records(self, raw_records: list[RawExpense]) -> list[NormalizedExpense]:
        classifications = self.classify(raw_records)
        output: list[NormalizedExpense] = []
        for raw, classification in zip(raw_records, classifications, strict=True):
            output.append(self.normalize(raw, classification))
        return output
