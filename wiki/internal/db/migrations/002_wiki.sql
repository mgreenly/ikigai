-- wiki domain schema (Task 3.1).
--
-- The wiki service has NEVER been deployed, so per the migration-immutability
-- rule's pre-production carve-out this placeholder is rewritten in place with the
-- real schema (rather than added as a 003). Any local dev DB MUST be reset so
-- this body runs on a fresh DB. Once wiki ships, this file is frozen and every
-- change is a new, higher-numbered, additive migration.
--
-- Two tables, both owner-scoped and collection-keyed (collection defaults to
-- 'default' per PLAN Decision 4 — the model carries the key from day one so
-- splitting wikis later is additive, not a migration):
--
--   wiki_ingest  — the queryable provenance ledger backing the immutable raw
--                  filesystem store. One row per raw-doc ingest.
--   wiki_jobs    — the agentkit job-record table. Backs a SQLite implementation
--                  of agentkit/job.Store: the generic Record columns
--                  (id, flight_key, status, started_at, ended_at, usage_json,
--                  error) PLUS owner + collection (owner-scoping and collection
--                  are consumer-side per the agentkit seam design).

-- Provenance ledger for the immutable raw/ store. Keyed by content sha256 within
-- an (owner, collection); the same bytes re-ingested are a safe no-op on disk and
-- a no-op here (INSERT OR IGNORE on the UNIQUE constraint).
CREATE TABLE wiki_ingest (
    id           TEXT    PRIMARY KEY,             -- ULID, consumer-minted
    owner        TEXT    NOT NULL,                -- X-Owner-Email (owner-scoped)
    collection   TEXT    NOT NULL DEFAULT 'default',
    sha256       TEXT    NOT NULL,                -- content hash; the raw-store key
    title        TEXT    NOT NULL DEFAULT '',     -- caller-supplied provenance
    source       TEXT    NOT NULL DEFAULT '',     -- caller-supplied provenance
    tags         TEXT    NOT NULL DEFAULT '[]',   -- caller-supplied tags, JSON array
    raw_path     TEXT    NOT NULL,                -- collection-relative raw/<sha256>.md
    source_path  TEXT    NOT NULL DEFAULT '',     -- resulting source page path, when filed
    ingested_at  TEXT    NOT NULL,                -- RFC3339, stamped at ingest
    UNIQUE (owner, collection, sha256)
);

CREATE INDEX wiki_ingest_owner_coll ON wiki_ingest (owner, collection);

-- agentkit job-record table. The first seven columns map 1:1 onto
-- agentkit/job.Record (id, flight_key, status, started_at, ended_at, usage_json,
-- error); owner + collection are the consumer-side scoping the agentkit seam
-- keeps out of Record. The single-flight gate (Store.Insert rejecting a second
-- running row for a flight_key) is enforced by the partial UNIQUE index below
-- under the suite's single-writer SQLite connection.
CREATE TABLE wiki_jobs (
    id          TEXT    PRIMARY KEY,              -- run id (ULID); Record.ID
    flight_key  TEXT    NOT NULL,                 -- single-flight key; Record.FlightKey
    status      TEXT    NOT NULL,                 -- running|succeeded|failed|cancelled
    started_at  TEXT    NOT NULL,                 -- RFC3339; Record.StartedAt
    ended_at    TEXT    NOT NULL DEFAULT '',      -- RFC3339, '' until terminal; Record.EndedAt
    usage_json  TEXT    NOT NULL DEFAULT '',      -- opaque accounting blob; Record.UsageJSON
    error       TEXT    NOT NULL DEFAULT '',      -- terminal error msg; Record.Error
    owner       TEXT    NOT NULL,                 -- X-Owner-Email (owner-scoped)
    collection  TEXT    NOT NULL DEFAULT 'default'
);

-- At most one RUNNING job per flight_key (the single-flight gate): a second
-- Insert with the same flight_key while one is running violates this index, which
-- is the ErrFlightInUse contract agentkit/job.Store.Insert relies on. Terminal
-- rows are not constrained, so a key can run again once its prior run finishes.
CREATE UNIQUE INDEX wiki_jobs_flight_running
    ON wiki_jobs (flight_key)
    WHERE status = 'running';

CREATE INDEX wiki_jobs_owner_coll ON wiki_jobs (owner, collection);
