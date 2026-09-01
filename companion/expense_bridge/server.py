from __future__ import annotations

import argparse
import hmac
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import sys

from .ai_processor import FallbackAIProcessor, OfflinePreviewProcessor
from .batch_parser import parse_batch_json
from .codex_processor import CodexExecProcessor
from .currency import PrototypeCurrencyConverter
from .parser import PayloadError
from .pipeline import ExpensePipeline
from .notion_sync import DisabledNotionSync, NotionHTTPClient
from .service import ExpenseService
from .sync_state import SQLiteSyncState


MAX_REQUEST_BYTES = 64 * 1024
DEFAULT_DATABASE = Path(__file__).resolve().parent.parent / "data" / "sync-state.sqlite3"


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the Cardputer expense preview server."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--database", type=Path, default=DEFAULT_DATABASE)
    parser.add_argument(
        "--classifier",
        choices=("offline", "codex"),
        default="offline",
        help="Classification provider; Codex mode falls back to offline preview rules",
    )
    parser.add_argument(
        "--codex-path",
        default=os.environ.get("CODEX_CLI_PATH"),
        help="Path to codex.exe/codex.cmd; otherwise resolve codex from PATH",
    )
    parser.add_argument(
        "--codex-timeout",
        type=int,
        default=int(os.environ.get("CODEX_TIMEOUT_SECONDS", "90")),
        help="Maximum seconds for one Codex batch classification",
    )
    parser.add_argument(
        "--codex-model",
        default=os.environ.get("CODEX_MODEL"),
        help="Optional Codex model override; default uses the CLI built-in default",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("CARDPUTER_SYNC_TOKEN"),
        help="Shared request token (prefer CARDPUTER_SYNC_TOKEN)",
    )
    parser.add_argument(
        "--enable-notion",
        action="store_true",
        help="Submit finalized records to Notion (disabled by default)",
    )
    parser.add_argument(
        "--notion-token",
        default=os.environ.get("NOTION_API_TOKEN"),
        help="Notion integration token (prefer NOTION_API_TOKEN)",
    )
    parser.add_argument(
        "--notion-database-id",
        default=os.environ.get("NOTION_DATABASE_ID"),
        help="Expense database ID (prefer NOTION_DATABASE_ID)",
    )
    parser.add_argument(
        "--notion-timeout",
        type=int,
        default=int(os.environ.get("NOTION_TIMEOUT_SECONDS", "30")),
    )
    parser.add_argument(
        "--notion-template",
        type=Path,
        default=(
            Path(os.environ["NOTION_TEMPLATE_PATH"])
            if os.environ.get("NOTION_TEMPLATE_PATH")
            else None
        ),
        help="Validated expense page JSON template",
    )
    parser.add_argument(
        "--notion-account-relation-id",
        default=os.environ.get("NOTION_ACCOUNT_RELATION_PAGE_ID"),
        help="Optional fixed related Account page ID",
    )
    parser.add_argument(
        "--allow-lan",
        action="store_true",
        help="Allow binding beyond localhost; not needed for the current milestone",
    )
    return parser.parse_args()


def make_handler(service: ExpenseService, token: str) -> type[BaseHTTPRequestHandler]:
    class ExpenseRequestHandler(BaseHTTPRequestHandler):
        server_version = "CardputerExpenseBridge/0.1"

        def _send_json(self, status: int, payload: dict) -> None:
            body = (json.dumps(payload, ensure_ascii=False, indent=2) + "\n").encode(
                "utf-8"
            )
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _authorized(self) -> bool:
            supplied = self.headers.get("X-Cardputer-Token", "")
            return hmac.compare_digest(supplied.encode(), token.encode())

        def do_GET(self) -> None:
            if self.path != "/health":
                self._send_json(404, {"error": "Not found"})
                return
            self._send_json(
                200,
                {
                    "status": "ok",
                    "mode": "notion-sync" if service.notion.enabled else "dry-run",
                    "notion_enabled": service.notion.enabled,
                    "classification": service.pipeline.ai.run_info,
                },
            )

        def do_POST(self) -> None:
            if self.path != "/expenses/sync":
                self._send_json(404, {"error": "Not found"})
                return
            if not self._authorized():
                self._send_json(401, {"error": "Invalid or missing token"})
                return
            content_type = self.headers.get("Content-Type", "").split(";", 1)[0]
            if content_type.strip().lower() != "application/json":
                self._send_json(415, {"error": "Content-Type must be application/json"})
                return
            try:
                content_length = int(self.headers.get("Content-Length", ""))
            except ValueError:
                self._send_json(411, {"error": "A valid Content-Length is required"})
                return
            if content_length <= 0:
                self._send_json(400, {"error": "Request body cannot be empty"})
                return
            if content_length > MAX_REQUEST_BYTES:
                self._send_json(413, {"error": "Request body is too large"})
                return
            try:
                raw_body = self.rfile.read(content_length).decode("utf-8")
                payload = json.loads(raw_body)
                batch = parse_batch_json(payload)
            except UnicodeDecodeError:
                self._send_json(400, {"error": "Request body must be UTF-8"})
                return
            except json.JSONDecodeError as exc:
                self._send_json(400, {"error": f"Invalid JSON: {exc.msg}"})
                return
            except PayloadError as exc:
                self._send_json(400, {"error": str(exc)})
                return

            self._send_json(200, service.process_preview(batch))

    return ExpenseRequestHandler


def main() -> int:
    args = _arguments()
    if not args.token or len(args.token) < 16:
        print(
            "error: set CARDPUTER_SYNC_TOKEN to a secret of at least 16 characters",
            file=sys.stderr,
        )
        return 2
    local_hosts = {"127.0.0.1", "localhost", "::1"}
    if args.host not in local_hosts and not args.allow_lan:
        print(
            "error: non-localhost binding requires --allow-lan",
            file=sys.stderr,
        )
        return 2
    if args.enable_notion and args.classifier == "offline":
        print(
            "error: Notion submission requires the Codex classifier",
            file=sys.stderr,
        )
        return 2

    offline = OfflinePreviewProcessor()
    try:
        if args.classifier == "codex":
            ai = FallbackAIProcessor(
                CodexExecProcessor(
                    executable=args.codex_path,
                    timeout_seconds=args.codex_timeout,
                    model=args.codex_model,
                ),
                offline,
            )
        else:
            ai = offline
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    try:
        notion = (
            NotionHTTPClient(
                args.notion_token or "",
                args.notion_database_id or "",
                timeout_seconds=args.notion_timeout,
                template_path=args.notion_template,
                account_relation_id=args.notion_account_relation_id,
            )
            if args.enable_notion
            else DisabledNotionSync()
        )
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    pipeline = ExpensePipeline(ai, PrototypeCurrencyConverter())
    service = ExpenseService(pipeline, SQLiteSyncState(args.database), notion)
    server = HTTPServer((args.host, args.port), make_handler(service, args.token))
    print(
        f"Expense preview server listening on http://{args.host}:{args.port}",
        flush=True,
    )
    print(
        f"Notion submission: {'ENABLED' if notion.enabled else 'disabled'}",
        flush=True,
    )
    print(f"Classifier: {args.classifier}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.", flush=True)
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
