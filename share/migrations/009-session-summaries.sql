-- Migration: 009-session-summaries
-- Description: Create session_summaries table for sliding context window
--
-- This migration creates the session_summaries table, which stores
-- auto-generated summaries of past message epochs for agents. The sliding
-- context window feature uses these summaries to fill the reserved portion
-- of the context budget when old messages fall off the back of the window.
--
-- Key design decisions:
-- - agent_uuid TEXT references agents(uuid) for ownership
-- - start_msg_id / end_msg_id identify the epoch being summarized (BIGINT)
-- - token_count INTEGER stores the token usage of the summary text
-- - created_at TIMESTAMPTZ for ordering (oldest-first on load)
-- - Unique constraint on (agent_uuid, start_msg_id, end_msg_id) prevents
--   duplicate summaries for the same epoch
-- - Cap of 5 summaries per agent is enforced at insert time by deleting
--   the oldest row(s) after insert

BEGIN;

CREATE TABLE IF NOT EXISTS session_summaries (
    id           BIGSERIAL PRIMARY KEY,
    agent_uuid   TEXT NOT NULL REFERENCES agents(uuid) ON DELETE CASCADE,
    summary      TEXT NOT NULL,
    start_msg_id BIGINT NOT NULL,
    end_msg_id   BIGINT NOT NULL,
    token_count  INTEGER NOT NULL,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Unique index: one summary per epoch per agent
CREATE UNIQUE INDEX IF NOT EXISTS idx_session_summaries_epoch
    ON session_summaries(agent_uuid, start_msg_id, end_msg_id);

-- Index for efficient chronological querying per agent
CREATE INDEX IF NOT EXISTS idx_session_summaries_agent_created
    ON session_summaries(agent_uuid, created_at DESC);

-- Update schema version from 8 to 9
UPDATE schema_metadata SET schema_version = 9 WHERE schema_version = 8;

COMMIT;
