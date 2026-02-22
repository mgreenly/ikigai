-- Migration: 002-agents-table
-- Description: Create agents table for agent registry
--
-- This migration creates the agents table, which serves as the source of truth
-- for agent existence in the agent process model. The table tracks agent lifecycle,
-- parent-child relationships, and status transitions.
--
-- Key design decisions:
-- - uuid is TEXT (base64url, 22 chars) - primary key
-- - parent_uuid self-references for hierarchy with ON DELETE RESTRICT
-- - fork_message_id is BIGINT - tracks where child branched from parent history
-- - status is PostgreSQL ENUM ('running', 'dead') - NOT NULL, defaults to 'running'
-- - created_at and ended_at are BIGINT (Unix epoch in seconds)
-- - Indexes on parent_uuid (for child queries) and status (for running agents)

BEGIN;

-- Create status enum type (idempotent)
DO $$ BEGIN
    CREATE TYPE agent_status AS ENUM ('running', 'dead');
EXCEPTION
    WHEN duplicate_object THEN NULL;
END $$;

-- Create agents table (idempotent)
CREATE TABLE IF NOT EXISTS agents (
    uuid          TEXT PRIMARY KEY,
    name          TEXT,
    parent_uuid   TEXT REFERENCES agents(uuid) ON DELETE RESTRICT,
    fork_message_id BIGINT,
    status        agent_status NOT NULL DEFAULT 'running',
    created_at    BIGINT NOT NULL,
    ended_at      BIGINT
);

-- Create indexes for efficient queries (idempotent)
CREATE INDEX IF NOT EXISTS idx_agents_parent ON agents(parent_uuid);
CREATE INDEX IF NOT EXISTS idx_agents_status ON agents(status);

-- Update schema version from 1 to 2
UPDATE schema_metadata SET schema_version = 2 WHERE schema_version = 1;

COMMIT;
