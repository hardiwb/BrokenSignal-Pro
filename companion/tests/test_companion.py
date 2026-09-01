from __future__ import annotations

import unittest
from unittest.mock import patch
from contextlib import closing, redirect_stderr
import io
from pathlib import Path
import subprocess
import sqlite3
import sys
from tempfile import TemporaryDirectory

from expense_bridge.ai_processor import (
    AIProcessor,
    FallbackAIProcessor,
    OfflinePreviewProcessor,
    OpenAIProcessor,
)
from expense_bridge.batch_parser import parse_batch_json
from expense_bridge.codex_processor import CodexExecProcessor
from expense_bridge.currency import PrototypeCurrencyConverter
from expense_bridge.models import NormalizedExpense
from expense_bridge.notion_sync import (
    NotionHTTPClient,
    NotionSync,
    build_page_request,
    default_page_template,
    load_page_template,
)
from expense_bridge.parser import PayloadError, parse_payload
from expense_bridge.pipeline import ExpensePipeline
from expense_bridge.service import ExpenseService
from expense_bridge.sync_state import MemorySyncState, SQLiteSyncState


SAMPLE = """2026-09-01
Ayam Goreng|20000|IDR
Es Doger|6000|IDR
Nasi Rendang|20000|IDR
"""


class ParserTests(unittest.TestCase):
    def test_parses_date_header_format(self) -> None:
        records = parse_payload(SAMPLE)
        self.assertEqual(3, len(records))
        self.assertEqual("2026-09-01", records[0].recorded_date.isoformat())
        self.assertEqual("6000", records[1].amount_text)
        self.assertEqual("IDR", records[2].currency)

    def test_parses_current_firmware_format(self) -> None:
        records = parse_payload(
            "2026-08-31|X|Ayam Goreng|20000|IDR\n"
            "2026-08-30|-|Bus|5000|IDR"
        )
        self.assertEqual("2026-08-30", records[1].recorded_date.isoformat())

    def test_rejects_invalid_calendar_date(self) -> None:
        with self.assertRaises(PayloadError):
            parse_payload("2026-02-30\nLunch|10000|IDR")

    def test_rejects_fractional_idr(self) -> None:
        with self.assertRaises(PayloadError):
            parse_payload("2026-08-31\nLunch|10.50|IDR")


class BatchParserTests(unittest.TestCase):
    def test_parses_server_batch_with_stable_id_and_decimal_text(self) -> None:
        batch = parse_batch_json(
            {
                "device": "cardputer",
                "entries": [
                    {
                        "id": "20260901-001",
                        "name": "Icon Pack",
                        "value": "3.50",
                        "currency": "USD",
                        "date": "2026-09-01",
                    }
                ],
            }
        )
        self.assertEqual("20260901-001", batch.entries[0].source_id)
        self.assertEqual("3.50", batch.entries[0].amount_text)

    def test_rejects_json_float_amount(self) -> None:
        with self.assertRaisesRegex(PayloadError, "not a JSON float"):
            parse_batch_json(
                {
                    "device": "cardputer",
                    "entries": [
                        {
                            "id": "entry-1",
                            "name": "Icon Pack",
                            "value": 3.5,
                            "currency": "USD",
                            "date": "2026-09-01",
                        }
                    ],
                }
            )


class PipelineTests(unittest.TestCase):
    def test_outputs_only_normalized_expense_fields(self) -> None:
        result = ExpensePipeline(
            OfflinePreviewProcessor(), PrototypeCurrencyConverter()
        ).process(SAMPLE)
        self.assertEqual(
            [
                {
                    "Title": "🍗 Ayam Goreng",
                    "Nominal": 20000,
                    "Jenis pengeluaran": "Food",
                    "Tanggal": "2026-09-01",
                },
                {
                    "Title": "🍧 Es Doger",
                    "Nominal": 6000,
                    "Jenis pengeluaran": "Food",
                    "Tanggal": "2026-09-01",
                },
                {
                    "Title": "🍛 Nasi Rendang",
                    "Nominal": 20000,
                    "Jenis pengeluaran": "Food",
                    "Tanggal": "2026-09-01",
                },
            ],
            result,
        )

    def test_non_idr_cannot_be_finalized_without_real_rate(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "Currency conversion required"):
            ExpensePipeline(
                OfflinePreviewProcessor(), PrototypeCurrencyConverter()
            ).process("2026-09-01\nIcon Pack|3.50|USD")


class PreviewServiceTests(unittest.TestCase):
    def _batch(self, value: str = "20000"):
        return parse_batch_json(
            {
                "device": "cardputer",
                "entries": [
                    {
                        "id": "20260901-001",
                        "name": "Ayam Goreng",
                        "value": value,
                        "currency": "IDR",
                        "date": "2026-09-01",
                    }
                ],
            }
        )

    def test_sqlite_state_survives_service_restart_and_returns_preview(self) -> None:
        with TemporaryDirectory() as directory:
            database = Path(directory) / "state.sqlite3"
            pipeline = ExpensePipeline(
                OfflinePreviewProcessor(), PrototypeCurrencyConverter()
            )
            first = ExpenseService(pipeline, SQLiteSyncState(database)).process_preview(
                self._batch()
            )
            second = ExpenseService(pipeline, SQLiteSyncState(database)).process_preview(
                self._batch()
            )

        self.assertTrue(first["dry_run"])
        self.assertEqual(["20260901-001"], first["processed"])
        self.assertEqual([], second["processed"])
        self.assertEqual(["20260901-001"], second["already_processed"])
        self.assertEqual("🍗 Ayam Goreng", second["records"][0]["normalized"]["Title"])

    def test_rejects_reused_id_with_changed_expense(self) -> None:
        with TemporaryDirectory() as directory:
            state = SQLiteSyncState(Path(directory) / "state.sqlite3")
            pipeline = ExpensePipeline(
                OfflinePreviewProcessor(), PrototypeCurrencyConverter()
            )
            service = ExpenseService(pipeline, state)
            service.process_preview(self._batch())
            result = service.process_preview(self._batch(value="25000"))

        self.assertEqual([], result["processed"])
        self.assertRegex(result["failed"][0]["error"], "different expense data")

    def test_fallback_preview_is_not_recorded_as_processed(self) -> None:
        class FailingProcessor(AIProcessor):
            def classify(self, records):
                raise RuntimeError("Codex unavailable")

            @property
            def run_info(self):
                return {"source": "codex-cli", "fallback": False}

        state = MemorySyncState()
        processor = FallbackAIProcessor(
            FailingProcessor(), OfflinePreviewProcessor()
        )
        service = ExpenseService(
            ExpensePipeline(processor, PrototypeCurrencyConverter()), state
        )
        with redirect_stderr(io.StringIO()):
            result = service.process_preview(self._batch())

        self.assertEqual([], result["processed"])
        self.assertEqual(["20260901-001"], result["fallback_previewed"])
        self.assertTrue(result["classification"]["fallback"])
        self.assertEqual("failed", state.get("20260901-001").status)

    def test_existing_preview_syncs_once_when_notion_is_enabled(self) -> None:
        class FakeNotion(NotionSync):
            def __init__(self) -> None:
                self.uploaded = []

            @property
            def enabled(self) -> bool:
                return True

            def upload(self, record: NormalizedExpense) -> str:
                self.uploaded.append(record)
                return "notion-page-1"

        state = MemorySyncState()
        pipeline = ExpensePipeline(
            OfflinePreviewProcessor(), PrototypeCurrencyConverter()
        )
        ExpenseService(pipeline, state).process_preview(self._batch())
        notion = FakeNotion()
        service = ExpenseService(pipeline, state, notion)

        first = service.process_preview(self._batch())
        second = service.process_preview(self._batch())

        self.assertFalse(first["dry_run"])
        self.assertEqual(["20260901-001"], first["synced"])
        self.assertEqual("synced", state.get("20260901-001").status)
        self.assertEqual("notion-page-1", state.get("20260901-001").remote_page_id)
        self.assertEqual(1, len(notion.uploaded))
        self.assertEqual(["20260901-001"], second["already_synced"])

    def test_notion_failure_keeps_preview_retryable(self) -> None:
        class FailingNotion(NotionSync):
            @property
            def enabled(self) -> bool:
                return True

            def upload(self, record: NormalizedExpense) -> str:
                raise RuntimeError("Notion unavailable")

        state = MemorySyncState()
        pipeline = ExpensePipeline(
            OfflinePreviewProcessor(), PrototypeCurrencyConverter()
        )
        ExpenseService(pipeline, state).process_preview(self._batch())
        result = ExpenseService(pipeline, state, FailingNotion()).process_preview(
            self._batch()
        )

        self.assertEqual([], result["synced"])
        self.assertEqual("notion", result["failed"][0]["stage"])
        self.assertEqual("processed", state.get("20260901-001").status)

    def test_existing_sqlite_ledger_migrates_for_notion_receipts(self) -> None:
        with TemporaryDirectory() as directory:
            database = Path(directory) / "legacy.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                with connection:
                    connection.execute(
                        """
                        CREATE TABLE expense_sync_state (
                            entry_id TEXT PRIMARY KEY,
                            fingerprint TEXT NOT NULL,
                            status TEXT NOT NULL CHECK(status IN ('processed', 'failed', 'synced')),
                            normalized_json TEXT,
                            error TEXT,
                            updated_at TEXT NOT NULL
                        )
                        """
                    )
            state = SQLiteSyncState(database)
            with closing(sqlite3.connect(database)) as connection:
                columns = {
                    row[1]
                    for row in connection.execute(
                        "PRAGMA table_info(expense_sync_state)"
                    )
                }

        self.assertIn("remote_page_id", columns)


class CodexExecProcessorTests(unittest.TestCase):
    def test_uses_ephemeral_read_only_schema_constrained_batch(self) -> None:
        calls = []

        def fake_runner(command, **kwargs):
            calls.append((command, kwargs))
            output_path = Path(
                command[command.index("--output-last-message") + 1]
            )
            output_path.write_text(
                '{"items":['
                '{"index":0,"normalized_title":"Ayam Goreng",'
                '"emoji":"🍗","category":"Food"},'
                '{"index":1,"normalized_title":"Monthly Gym",'
                '"emoji":"🏋️","category":"Recurring"}]}',
                encoding="utf-8",
            )
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")

        records = parse_payload(
            "2026-09-01\nAyam Goreng|20000|IDR\nMonthly Gym|100000|IDR"
        )
        processor = CodexExecProcessor(
            executable=sys.executable,
            timeout_seconds=12,
            runner=fake_runner,
        )
        result = processor.classify(records)

        self.assertEqual(1, len(calls))
        command, kwargs = calls[0]
        self.assertIn("--ephemeral", command)
        self.assertEqual("read-only", command[command.index("--sandbox") + 1])
        self.assertIn("--output-schema", command)
        self.assertIn("Ayam Goreng", kwargs["input"])
        self.assertEqual(12, kwargs["timeout"])
        self.assertNotIn("OPENAI_API_KEY", kwargs["env"])
        self.assertNotIn("CODEX_API_KEY", kwargs["env"])
        self.assertEqual("🍗 Ayam Goreng", result[0].title)
        self.assertEqual("Recurring", result[1].category)

    def test_rejects_duplicate_output_indexes(self) -> None:
        def fake_runner(command, **kwargs):
            output_path = Path(
                command[command.index("--output-last-message") + 1]
            )
            output_path.write_text(
                '{"items":['
                '{"index":0,"normalized_title":"One","emoji":"1️⃣",'
                '"category":"Lifestyle"},'
                '{"index":0,"normalized_title":"Two","emoji":"2️⃣",'
                '"category":"Lifestyle"}]}',
                encoding="utf-8",
            )
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")

        processor = CodexExecProcessor(
            executable=sys.executable,
            runner=fake_runner,
        )
        records = parse_payload("2026-09-01\nOne|1|IDR\nTwo|2|IDR")
        with self.assertRaisesRegex(RuntimeError, "duplicate"):
            processor.classify(records)


class NotionMappingTests(unittest.TestCase):
    def test_builds_shortcut_compatible_request_with_fixed_schema(self) -> None:
        from datetime import date

        account_relation_id = "11111111-2222-3333-4444-555555555555"

        request = build_page_request(
            NormalizedExpense("🍗 Ayam Goreng", 20000, "Food", date(2026, 9, 1)),
            "DatabaseID",
            account_relation_id,
        )
        properties = request["properties"]
        self.assertEqual({"database_id": "DatabaseID"}, request["parent"])
        self.assertEqual(
            "🍗 Ayam Goreng", properties["Name"]["title"][0]["text"]["content"]
        )
        self.assertEqual(20000, properties["Expense"]["number"])
        self.assertEqual("Food", properties["Type of Expense"]["select"]["name"])
        self.assertEqual("2026-09-01", properties["Date "]["date"]["start"])
        self.assertNotIn("Date", properties)
        self.assertEqual(
            account_relation_id, properties["Account"]["relation"][0]["id"]
        )

    def test_http_client_posts_one_page_with_required_headers(self) -> None:
        from datetime import date
        import json

        calls = []

        class FakeResponse:
            def __enter__(self):
                return self

            def __exit__(self, *args):
                return False

            def read(self) -> bytes:
                return b'{"object":"page","id":"created-page-id"}'

        def fake_opener(request, **kwargs):
            calls.append((request, kwargs))
            return FakeResponse()

        client = NotionHTTPClient(
            "secret-token",
            "DatabaseID",
            account_relation_id="relation-page-id",
            opener=fake_opener,
        )
        page_id = client.upload(
            NormalizedExpense("ðŸ— Ayam Goreng", 20000, "Food", date(2026, 9, 1))
        )

        self.assertEqual("created-page-id", page_id)
        request, kwargs = calls[0]
        self.assertEqual("POST", request.method)
        self.assertEqual("Bearer secret-token", request.get_header("Authorization"))
        self.assertEqual("2026-03-11", request.get_header("Notion-version"))
        self.assertEqual(30, kwargs["timeout"])
        self.assertEqual(
            {"database_id": "DatabaseID"},
            json.loads(request.data.decode("utf-8"))["parent"],
        )

    def test_generated_template_is_validated_before_use(self) -> None:
        import json

        with TemporaryDirectory() as directory:
            path = Path(directory) / "notion-template.json"
            relation_id = "relation-page-id"
            path.write_text(
                json.dumps(default_page_template(relation_id), ensure_ascii=False),
                encoding="utf-8",
            )
            loaded = load_page_template(path, relation_id)
            loaded["properties"]["Date"] = loaded["properties"].pop("Date ")
            path.write_text(json.dumps(loaded), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "fixed expense database schema"):
                load_page_template(path, relation_id)

    def test_configured_account_relation_replaces_project_reference(self) -> None:
        from datetime import date

        relation_id = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
        template = default_page_template(relation_id)
        request = build_page_request(
            NormalizedExpense("ðŸ— Ayam Goreng", 20000, "Food", date(2026, 9, 1)),
            "DatabaseID",
            relation_id,
            template,
        )

        self.assertEqual(
            relation_id,
            request["properties"]["Account"]["relation"][0]["id"],
        )

    def test_account_relation_can_be_omitted(self) -> None:
        from datetime import date

        template = default_page_template()
        request = build_page_request(
            NormalizedExpense("Expense", 1000, "Lifestyle", date(2026, 9, 1)),
            "DatabaseID",
            template=template,
        )

        self.assertNotIn("Account", request["properties"])

class OpenAIProcessorTests(unittest.TestCase):
    def test_maps_structured_response_by_index(self) -> None:
        response = {
            "output": [
                {
                    "type": "message",
                    "content": [
                        {
                            "type": "output_text",
                            "text": (
                                '{"items":['
                                '{"index":0,"normalized_title":"Ayam Goreng",'
                                '"emoji":"🍗","category":"Food"}]}'
                            ),
                        }
                    ],
                }
            ]
        }

        class FakeHTTPResponse:
            def __enter__(self):
                return self

            def __exit__(self, *args):
                return False

            def read(self) -> bytes:
                import json

                return json.dumps(response).encode("utf-8")

        with patch("urllib.request.urlopen", return_value=FakeHTTPResponse()):
            result = OpenAIProcessor(api_key="test-key").classify(
                parse_payload("2026-08-31\nAyam Goreng|20000|IDR")
            )
        self.assertEqual("🍗 Ayam Goreng", result[0].title)
        self.assertEqual("Food", result[0].category)


if __name__ == "__main__":
    unittest.main()
