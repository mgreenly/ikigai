-- Migration: 007-agents-session-id
-- Description: Add session_id column to agents table
--
-- This migration adds the session_id foreign key to the agents table to scope
-- agents to their session. This enables filtering dead agents by session and
-- prevents /agents from showing dead agents from previous sessions.
--
-- Key design decisions:
-- - session_id is BIGINT NOT NULL - every agent belongs to exactly one session
-- - Foreign key references sessions(id) with ON DELETE CASCADE
-- - Index on session_id for efficient session-scoped queries
-- - For existing agents, we set session_id to the most recent session (acceptable
--   since this is a development feature being added early in the project lifecycle)

BEGIN;

-- Add session_id column (defaults to NULL temporarily)
ALTER TABLE agents ADD COLUMN IF NOT EXISTS session_id BIGINT;

-- Set session_id for existing agents to the most recent session
-- This handles the case where agents already exist before this migration
UPDATE agents
SET session_id = (SELECT id FROM sessions ORDER BY started_at DESC LIMIT 1)
WHERE session_id IS NULL;

-- Now make the column NOT NULL and add the foreign key
ALTER TABLE agents ALTER COLUMN session_id SET NOT NULL;
ALTER TABLE agents ADD CONSTRAINT fk_agents_session
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE;

-- Create index for efficient session-scoped queries
CREATE INDEX IF NOT EXISTS idx_agents_session ON agents(session_id);

-- Update schema version from 6 to 7
UPDATE schema_metadata SET schema_version = 7 WHERE schema_version = 6;

COMMIT;
