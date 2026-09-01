from __future__ import annotations

from .batch_parser import ExpenseBatch
from .models import NormalizedExpense
from .notion_sync import DisabledNotionSync, NotionSync
from .pipeline import ExpensePipeline
from .sync_state import SyncState


class ExpenseService:
    def __init__(
        self,
        pipeline: ExpensePipeline,
        state: SyncState,
        notion: NotionSync | None = None,
    ) -> None:
        self.pipeline = pipeline
        self.state = state
        self.notion = notion or DisabledNotionSync()

    def process_preview(self, batch: ExpenseBatch) -> dict:
        candidates = []
        sync_queue: list[tuple[str, str, NormalizedExpense]] = []
        processed: list[str] = []
        synced: list[str] = []
        already_processed: list[str] = []
        already_synced: list[str] = []
        failures: list[dict] = []
        fallback_previewed: list[str] = []
        results: list[dict] = []
        classification_info = {"source": "stored", "fallback": False}

        for record in batch.entries:
            stored = self.state.get(record.source_id)
            if stored is None or stored.status == "failed":
                candidates.append(record)
                continue
            if stored.fingerprint != record.fingerprint:
                failures.append(
                    {
                        "id": record.source_id,
                        "error": "Entry ID was already used for different expense data",
                    }
                )
                continue
            if stored.normalized is None:
                failures.append(
                    {"id": record.source_id, "error": "Stored normalized record is missing"}
                )
                continue
            try:
                normalized = NormalizedExpense.from_dict(stored.normalized)
            except (TypeError, ValueError) as exc:
                failures.append(
                    {"id": record.source_id, "error": f"Stored record is invalid: {exc}"}
                )
                continue

            results.append({"id": record.source_id, "normalized": stored.normalized})
            if stored.status == "synced":
                already_processed.append(record.source_id)
                already_synced.append(record.source_id)
            elif self.notion.enabled:
                sync_queue.append((record.source_id, record.fingerprint, normalized))
            else:
                already_processed.append(record.source_id)

        if candidates:
            try:
                classifications = self.pipeline.classify(candidates)
            except Exception as exc:
                message = f"Classification failed: {exc}"
                for record in candidates:
                    self.state.record_failed(
                        record.source_id, record.fingerprint, message
                    )
                    failures.append({"id": record.source_id, "error": message})
            else:
                classification_info = self.pipeline.ai.run_info
                used_fallback = bool(classification_info.get("fallback"))
                for record, classification in zip(
                    candidates, classifications, strict=True
                ):
                    try:
                        normalized = self.pipeline.normalize(record, classification)
                    except (RuntimeError, ValueError) as exc:
                        message = str(exc)
                        self.state.record_failed(
                            record.source_id, record.fingerprint, message
                        )
                        failures.append({"id": record.source_id, "error": message})
                        continue
                    normalized_dict = normalized.to_dict()
                    if used_fallback:
                        message = (
                            "Offline fallback preview; retry when Codex CLI is available"
                        )
                        self.state.record_failed(
                            record.source_id, record.fingerprint, message
                        )
                        fallback_previewed.append(record.source_id)
                    else:
                        self.state.record_processed(
                            record.source_id, record.fingerprint, normalized_dict
                        )
                        if self.notion.enabled:
                            sync_queue.append(
                                (record.source_id, record.fingerprint, normalized)
                            )
                        else:
                            processed.append(record.source_id)
                    results.append(
                        {"id": record.source_id, "normalized": normalized_dict}
                    )

        for entry_id, fingerprint, normalized in sync_queue:
            try:
                page_id = self.notion.upload(normalized)
            except Exception as exc:
                failures.append(
                    {"id": entry_id, "stage": "notion", "error": str(exc)}
                )
                continue
            self.state.record_synced(
                entry_id, fingerprint, normalized.to_dict(), page_id
            )
            processed.append(entry_id)
            synced.append(entry_id)

        return {
            "device": batch.device,
            "dry_run": not self.notion.enabled,
            "notion_enabled": self.notion.enabled,
            "processed": processed,
            "synced": synced,
            "already_processed": already_processed,
            "already_synced": already_synced,
            "fallback_previewed": fallback_previewed,
            "failed": failures,
            "records": results,
            "classification": classification_info,
        }
