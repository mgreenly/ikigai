-- 003_feed_offset.sql — event-plane consumer offset store (event-protocol.md
-- §10.3, minus the dedup table). The DDL is OWNED by the eventplane library
-- (consumer.SchemaSQL); this file must stay byte-identical to that constant.
-- scripts' own migration runner applies it so there is a single migration
-- authority per DB file, even though the schema's source of truth lives in the
-- library.
--
-- scripts is a multi-upstream CONSUMER: one feed_offset row per consumed source
-- (cron, crm, ledger, dropbox, prompts). There is deliberately NO dedup table:
-- scripts' run model is fire-and-run, which tolerates a rare duplicate run, so
-- the cursor (plus the first-subscription marker) is its only durable consumer
-- state.
CREATE TABLE feed_offset (
  source     TEXT    PRIMARY KEY,
  cursor     TEXT,
  subscribed INTEGER NOT NULL DEFAULT 0,
  updated_at TEXT    NOT NULL
);
