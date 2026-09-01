from __future__ import annotations

from abc import ABC, abstractmethod
from copy import deepcopy
import json
from pathlib import Path
import urllib.error
import urllib.request

from .models import NormalizedExpense


NAME_PROPERTY = "Name"
EXPENSE_PROPERTY = "Expense"
TYPE_PROPERTY = "Type of Expense"
DATE_PROPERTY = "Date "  # The trailing space is part of the Notion schema.
ACCOUNT_PROPERTY = "Account"
DATABASE_ID_PLACEHOLDER = "{{NOTION_DATABASE_ID}}"
TITLE_PLACEHOLDER = "{{Title}}"
NOMINAL_PLACEHOLDER = "{{Nominal}}"
CATEGORY_PLACEHOLDER = "{{Jenis pengeluaran}}"
DATE_PLACEHOLDER = "{{Tanggal}}"


def default_page_template(account_relation_id: str | None = None) -> dict:
    properties = {
        NAME_PROPERTY: {
            "title": [{"text": {"content": TITLE_PLACEHOLDER}}],
        },
        EXPENSE_PROPERTY: {"number": NOMINAL_PLACEHOLDER},
        TYPE_PROPERTY: {"select": {"name": CATEGORY_PLACEHOLDER}},
        DATE_PROPERTY: {"date": {"start": DATE_PLACEHOLDER}},
    }
    if account_relation_id:
        properties[ACCOUNT_PROPERTY] = {
            "relation": [{"id": account_relation_id}]
        }
    return {
        "parent": {"database_id": DATABASE_ID_PLACEHOLDER},
        "properties": properties,
    }


def load_page_template(path: Path, account_relation_id: str | None = None) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except OSError as exc:
        raise ValueError(f"Cannot read Notion template {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"Notion template is invalid JSON: {exc.msg}") from exc
    if value != default_page_template(account_relation_id):
        raise ValueError(
            "Notion template does not match the fixed expense database schema"
        )
    return value


def build_page_request(
    record: NormalizedExpense,
    database_id: str,
    account_relation_id: str | None = None,
    template: dict | None = None,
) -> dict:
    """Build the Apple Shortcut-compatible request without performing I/O."""
    if not database_id.strip():
        raise ValueError("Notion database_id cannot be empty")
    source = (
        template
        if template is not None
        else default_page_template(account_relation_id)
    )
    if source != default_page_template(account_relation_id):
        raise ValueError("Notion template does not match the fixed expense schema")
    request = deepcopy(source)
    request["parent"]["database_id"] = database_id
    properties = request["properties"]
    properties[NAME_PROPERTY]["title"][0]["text"]["content"] = record.title
    properties[EXPENSE_PROPERTY]["number"] = record.nominal
    properties[TYPE_PROPERTY]["select"]["name"] = record.category
    properties[DATE_PROPERTY]["date"]["start"] = record.recorded_date.isoformat()
    return request


class NotionSync(ABC):
    @property
    @abstractmethod
    def enabled(self) -> bool:
        raise NotImplementedError

    @abstractmethod
    def upload(self, record: NormalizedExpense) -> str:
        raise NotImplementedError


class DisabledNotionSync(NotionSync):
    @property
    def enabled(self) -> bool:
        return False

    def upload(self, record: NormalizedExpense) -> str:
        raise RuntimeError("Notion submission is disabled by configuration")


class NotionHTTPClient(NotionSync):
    ENDPOINT = "https://api.notion.com/v1/pages"
    NOTION_VERSION = "2026-03-11"

    def __init__(
        self,
        api_token: str,
        database_id: str,
        *,
        account_relation_id: str | None = None,
        timeout_seconds: int = 30,
        template_path: Path | None = None,
        opener=urllib.request.urlopen,
    ) -> None:
        self.api_token = api_token.strip()
        self.database_id = database_id.strip()
        self.timeout_seconds = timeout_seconds
        self.account_relation_id = (account_relation_id or "").strip() or None
        self.opener = opener
        if not self.api_token:
            raise ValueError("NOTION_API_TOKEN cannot be empty")
        if not self.database_id:
            raise ValueError("NOTION_DATABASE_ID cannot be empty")
        if timeout_seconds <= 0:
            raise ValueError("Notion timeout must be greater than zero")
        self.template = (
            load_page_template(template_path, self.account_relation_id)
            if template_path is not None
            else default_page_template(self.account_relation_id)
        )

    @property
    def enabled(self) -> bool:
        return True

    def upload(self, record: NormalizedExpense) -> str:
        body = json.dumps(
            build_page_request(
                record,
                self.database_id,
                self.account_relation_id,
                self.template,
            ),
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        request = urllib.request.Request(
            self.ENDPOINT,
            data=body,
            method="POST",
            headers={
                "Authorization": f"Bearer {self.api_token}",
                "Content-Type": "application/json",
                "Notion-Version": self.NOTION_VERSION,
            },
        )
        try:
            with self.opener(request, timeout=self.timeout_seconds) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            try:
                detail = json.loads(exc.read().decode("utf-8")).get("message")
            except (UnicodeDecodeError, json.JSONDecodeError, AttributeError):
                detail = None
            message = detail or exc.reason or "request rejected"
            raise RuntimeError(f"Notion HTTP {exc.code}: {message}") from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(f"Notion connection failed: {exc.reason}") from exc
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise RuntimeError("Notion returned an invalid JSON response") from exc

        if payload.get("object") != "page" or not isinstance(payload.get("id"), str):
            raise RuntimeError("Notion response did not contain a created page ID")
        return payload["id"]
