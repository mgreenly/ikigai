-- Migration: 010-background-processes
-- Description: Add background_processes table for process metadata
--
-- This migration adds the schema for tracking background processes started by
-- agents. Process output lives on disk; the DB stores metadata only, enabling
-- crash recovery and audit trail.
--
-- Key design decisions:
-- - agent_uuid TEXT references agents(uuid) ON DELETE CASCADE (not integer id)
-- - bg_process_status ENUM covers all lifecycle states
-- - output_path TEXT stores the disk path to the raw PTY output file
-- - Indexes on agent_uuid (ownership queries) and active status (recovery scan)

BEGIN;

-- Create bg_process_status enum type (idempotent)
DO $$ BEGIN
    CREATE TYPE bg_process_status AS ENUM (
        'starting', 'running', 'exited', 'killed', 'timed_out', 'failed'
    );
EXCEPTION
    WHEN duplicate_object THEN NULL;
END $$;

-- Create background_processes table (idempotent)
CREATE TABLE IF NOT EXISTS background_processes (
    id              SERIAL PRIMARY KEY,
    agent_uuid      TEXT NOT NULL REFERENCES agents(uuid) ON DELETE CASCADE,
    pid             INTEGER,
    command         TEXT NOT NULL,
    label           TEXT NOT NULL,
    status          bg_process_status NOT NULL DEFAULT 'starting',
    exit_code       INTEGER,
    exit_signal     INTEGER,
    ttl_seconds     INTEGER NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    started_at      TIMESTAMPTZ,
    exited_at       TIMESTAMPTZ,
    total_bytes     BIGINT NOT NULL DEFAULT 0,
    output_path     TEXT
);

-- Index for ownership queries (list processes for an agent)
CREATE INDEX IF NOT EXISTS idx_bg_proc_agent
    ON background_processes(agent_uuid);

-- Partial index for active-status queries (startup recovery scan)
CREATE INDEX IF NOT EXISTS idx_bg_proc_active
    ON background_processes(status)
    WHERE status IN ('starting', 'running');

-- Update schema version from 9 to 10
UPDATE schema_metadata SET schema_version = 10 WHERE schema_version = 9;

COMMIT;
