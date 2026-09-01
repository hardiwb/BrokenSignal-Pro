from __future__ import annotations

from abc import ABC, abstractmethod
import json
import os
import sys
import urllib.error
import urllib.request

from .models import CATEGORIES, Classification, RawExpense


class AIProcessor(ABC):
    @abstractmethod
    def classify(self, records: list[RawExpense]) -> list[Classification]:
        raise NotImplementedError

    @property
    def run_info(self) -> dict:
        return {"source": type(self).__name__, "fallback": False}


class OpenAIProcessor(AIProcessor):
    """Classifies titles only. Dates, amounts, and currencies never pass through AI."""

    def __init__(self, api_key: str | None = None, model: str | None = None) -> None:
        self.api_key = api_key or os.environ.get("OPENAI_API_KEY")
        self.model = model or os.environ.get("OPENAI_MODEL", "gpt-5-mini")
        if not self.api_key:
            raise RuntimeError(
                "OPENAI_API_KEY is not set. Configure it or use --offline-preview."
            )

    def classify(self, records: list[RawExpense]) -> list[Classification]:
        schema = {
            "type": "object",
            "properties": {
                "items": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "index": {"type": "integer"},
                            "normalized_title": {"type": "string"},
                            "emoji": {"type": "string"},
                            "category": {"type": "string", "enum": list(CATEGORIES)},
                        },
                        "required": [
                            "index",
                            "normalized_title",
                            "emoji",
                            "category",
                        ],
                        "additionalProperties": False,
                    },
                }
            },
            "required": ["items"],
            "additionalProperties": False,
        }
        names = [
            {"index": index, "raw_title": item.name}
            for index, item in enumerate(records)
        ]
        body = {
            "model": self.model,
            "store": False,
            "instructions": (
                "Normalize expense titles and classify them. Return one item per input, "
                "in the same index mapping. Use exactly one appropriate emoji. The "
                "normalized_title must be concise title case and must not contain an emoji. "
                "Choose only from the category enum supplied by the output schema."
            ),
            "input": json.dumps(names, ensure_ascii=False),
            "text": {
                "format": {
                    "type": "json_schema",
                    "name": "expense_classifications",
                    "strict": True,
                    "schema": schema,
                }
            },
        }
        request = urllib.request.Request(
            "https://api.openai.com/v1/responses",
            data=json.dumps(body).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                response_body = json.load(response)
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"OpenAI API request failed ({exc.code}): {detail}") from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(f"OpenAI API request failed: {exc.reason}") from exc

        output_text = _extract_output_text(response_body)
        parsed = json.loads(output_text)
        items = parsed.get("items", [])
        if len(items) != len(records):
            raise RuntimeError("AI response record count does not match the input")
        by_index = {item["index"]: item for item in items}
        if set(by_index) != set(range(len(records))):
            raise RuntimeError("AI response indexes do not match the input")
        return [
            Classification(
                normalized_title=by_index[index]["normalized_title"],
                emoji=by_index[index]["emoji"],
                category=by_index[index]["category"],
            )
            for index in range(len(records))
        ]

    @property
    def run_info(self) -> dict:
        return {"source": "openai-api", "fallback": False}


def _extract_output_text(response_body: dict) -> str:
    for item in response_body.get("output", []):
        if item.get("type") != "message":
            continue
        for content in item.get("content", []):
            if content.get("type") == "output_text":
                return content["text"]
    raise RuntimeError("OpenAI API response did not contain output text")


class OfflinePreviewProcessor(AIProcessor):
    """Deterministic development preview; intentionally not presented as AI."""

    _RULES = (
        (("nasi rendang",), "🍛", "Food"),
        (("es doger",), "🍧", "Food"),
        (("ayam",), "🍗", "Food"),
        (("nasi", "kopi", "tebu", "makan", "food"), "🍽️", "Food"),
        (("grocery", "groceries", "supermarket", "beras", "sayur"), "🛒", "Groceries"),
        (("grab", "gojek", "taxi", "bus", "train", "bensin"), "🚕", "Transport"),
        (("rent", "kos", "listrik", "water", "internet"), "🏠", "Living"),
        (("shampoo", "skincare", "salon", "barber"), "🧴", "Personal Care"),
        (("stock", "reksa", "investment", "deposit"), "📈", "Investment"),
        (("subscription", "premium", "monthly", "netflix"), "🔁", "Recurring"),
    )

    def classify(self, records: list[RawExpense]) -> list[Classification]:
        output: list[Classification] = []
        for record in records:
            lowered = record.name.casefold()
            emoji, category = "🎨", "Lifestyle"
            for keywords, candidate_emoji, candidate_category in self._RULES:
                if any(keyword in lowered for keyword in keywords):
                    emoji, category = candidate_emoji, candidate_category
                    break
            output.append(
                Classification(
                    normalized_title=record.name.strip().title(),
                    emoji=emoji,
                    category=category,
                )
            )
        return output

    @property
    def run_info(self) -> dict:
        return {"source": "offline-rules", "fallback": False}


class FallbackAIProcessor(AIProcessor):
    """Use deterministic preview rules if the configured classifier fails."""

    def __init__(self, primary: AIProcessor, fallback: AIProcessor) -> None:
        self.primary = primary
        self.fallback = fallback
        self._run_info = primary.run_info.copy()

    def classify(self, records: list[RawExpense]) -> list[Classification]:
        try:
            result = self.primary.classify(records)
        except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
            reason = str(exc).strip() or type(exc).__name__
            print(
                f"warning: primary classifier failed; using offline preview: {reason}",
                file=sys.stderr,
                flush=True,
            )
            result = self.fallback.classify(records)
            self._run_info = {
                "source": self.fallback.run_info["source"],
                "fallback": True,
                "primary": self.primary.run_info["source"],
                "reason": reason[:500],
            }
            return result
        self._run_info = self.primary.run_info.copy()
        return result

    @property
    def run_info(self) -> dict:
        return self._run_info.copy()
