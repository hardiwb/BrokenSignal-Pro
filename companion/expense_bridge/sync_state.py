from __future__ import annotations

from abc import ABC, abstractmethod
from contextlib import closing
from dataclasses import dataclass
from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3


@dataclass(frozen=True)
class StoredEntry:
    entry_id: str
    fingerprint: str
    status: str
    normalized: dict | None
    error: str | None
    remote_page_id: str | None = None


class SyncState(ABC):
    @abstractmethod
    def get(self, entry_id: str) -> StoredEntry | None:
        raise NotImplementedError

    @abstractmethod
    def record_processed(
        self, entry_id: str, fingerprint: str, normalized: dict
    ) -> None:
        raise NotImplementedError

    @abstractmethod
    def record_failed(self, entry_id: str, fingerprint: str, error: str) -> None:
        raise NotImplementedError

    @abstractmethod
    def record_synced(
        self, entry_id: str, fingerprint: str, normalized: dict, remote_page_id: str
    ) -> None:
        raise NotImplementedError


class MemorySyncState(SyncState):
    def __init__(self) -> None:
        self._entries: dict[str, StoredEntry] = {}

    def get(self, entry_id: str) -> StoredEntry | None:
        return self._entries.get(entry_id)

    def record_processed(
        self, entry_id: str, fingerprint: str, normalized: dict
    ) -> None:
        self._entries[entry_id] = StoredEntry(
            entry_id, fingerprint, "processed", normalized.copy(), None
        )

    def record_failed(self, entry_id: str, fingerprint: str, error: str) -> None:
        self._entries[entry_id] = StoredEntry(
            entry_id, fingerprint, "failed", None, error
        )

    def record_synced(
        self, entry_id: str, fingerprint: str, normalized: dict, remote_page_id: str
    ) -> None:
        self._entries[entry_id] = StoredEntry(
            entry_id, fingerprint, "synced", normalized.copy(), None, remote_page_id
        )


class SQLiteSyncState(SyncState):
    """Durable ledger; only a synced row contains a Notion page receipt."""

    def __init__(self, database_path: Path) -> None:
        self.database_path = database_path
        database_path.parent.mkdir(parents=True, exist_ok=True)
        self._initialize()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.database_path, timeout=5)
        connection.row_factory = sqlite3.Row
        return connection

    def _initialize(self) -> None:
        with closing(self._connect()) as connection:
            with connection:
                connection.execute(
                    """
                    CREATE TABLE IF NOT EXISTS expense_sync_state (
                        entry_id TEXT PRIMARY KEY,
                        fingerprint TEXT NOT NULL,
                        status TEXT NOT NULL CHECK(status IN ('processed', 'failed', 'synced')),
                        normalized_json TEXT,
                        error TEXT,
                        remote_page_id TEXT,
                        updated_at TEXT NOT NULL
                    )
                    """
                )
                columns = {
                    row["name"]
                    for row in connection.execute("PRAGMA table_info(expense_sync_state)")
                }
                if "remote_page_id" not in columns:
                    connection.execute(
                        "ALTER TABLE expense_sync_state ADD COLUMN remote_page_id TEXT"
                    )

    def get(self, entry_id: str) -> StoredEntry | None:
        with closing(self._connect()) as connection:
            row = connection.execute(
                """
                SELECT entry_id, fingerprint, status, normalized_json, error, remote_page_id
                FROM expense_sync_state WHERE entry_id = ?
                """,
                (entry_id,),
            ).fetchone()
        if row is None:
            return None
        normalized = (
            json.loads(row["normalized_json"])
            if row["normalized_json"] is not None
            else None
        )
        return StoredEntry(
            row["entry_id"], row["fingerprint"], row["status"], normalized,
            row["error"], row["remote_page_id"]
        )

    def record_processed(
        self, entry_id: str, fingerprint: str, normalized: dict
    ) -> None:
        self._upsert(
            entry_id,
            fingerprint,
            "processed",
            json.dumps(normalized, ensure_ascii=False, separators=(",", ":")),
            None,
            None,
        )

    def record_failed(self, entry_id: str, fingerprint: str, error: str) -> None:
        self._upsert(entry_id, fingerprint, "failed", None, error, None)

    def record_synced(
        self, entry_id: str, fingerprint: str, normalized: dict, remote_page_id: str
    ) -> None:
        self._upsert(
            entry_id,
            fingerprint,
            "synced",
            json.dumps(normalized, ensure_ascii=False, separators=(",", ":")),
            None,
            remote_page_id,
        )

    def _upsert(
        self,
        entry_id: str,
        fingerprint: str,
        status: str,
        normalized_json: str | None,
        error: str | None,
        remote_page_id: str | None,
    ) -> None:
        updated_at = datetime.now(timezone.utc).isoformat()
        with closing(self._connect()) as connection:
            with connection:
                connection.execute(
                    """
                    INSERT INTO expense_sync_state (
                        entry_id, fingerprint, status, normalized_json, error,
                        remote_page_id, updated_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?)
                    ON CONFLICT(entry_id) DO UPDATE SET
                        fingerprint = excluded.fingerprint,
                        status = excluded.status,
                        normalized_json = excluded.normalized_json,
                        error = excluded.error,
                        remote_page_id = excluded.remote_page_id,
                        updated_at = excluded.updated_at
                    """,
                    (
                        entry_id,
                        fingerprint,
                        status,
                        normalized_json,
                        error,
                        remote_page_id,
                        updated_at,
                    ),
                )
