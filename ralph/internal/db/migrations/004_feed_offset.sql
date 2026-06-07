-- 004_feed_offset.sql — event-plane consumer offset store (event-protocol.md
-- §10.3, minus the dedup table). The DDL is OWNED by the eventplane library
-- (consumer.SchemaSQL); this file must stay byte-identical to that constant —
-- internal/db/migrations_feed_offset_test.go asserts it. ralph's own migration
-- runner applies it so there is a single migration authority per DB file, even
-- though the schema's source of truth lives in the library.
--
-- ralph is now a CONSUMER (of cron's /feed) as well as a domain service. Its one
-- feed_offset row keys on the upstream source "cron". There is deliberately NO
-- dedup table: ralph's run model is in-memory fire-and-run (event-triggering
-- decisions §3), which tolerates a rare duplicate run, so the cursor (plus the
-- first-subscription marker) is its only durable consumer state.
CREATE TABLE feed_offset (
  source     TEXT    PRIMARY KEY,
  cursor     TEXT,
  subscribed INTEGER NOT NULL DEFAULT 0,
  updated_at TEXT    NOT NULL
);
