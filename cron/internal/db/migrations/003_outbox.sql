-- Event-plane outbox (event-protocol.md §4.5). The DDL is OWNED by the
-- eventplane library (outbox.SchemaSQL); this file must stay byte-identical to
-- that constant — migrations_outbox_test.go asserts it. cron's own migration
-- runner applies it so there is a single migration authority per DB file, even
-- though the schema's source of truth lives in the library. cron is the
-- event-plane producer of the dynamic cron.<name> types (event-triggering
-- decisions §2); the tick worker (internal/tick) Appends into this table.
CREATE TABLE outbox (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,
  event_id   TEXT    NOT NULL,
  type       TEXT    NOT NULL,
  payload    TEXT    NOT NULL,
  created_at TEXT    NOT NULL
);
CREATE INDEX idx_outbox_created_at ON outbox(created_at);
