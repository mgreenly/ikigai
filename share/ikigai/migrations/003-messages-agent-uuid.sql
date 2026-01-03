-- Migration: 003-messages-agent-uuid
-- Description: Add agent_uuid column to messages table
--
-- This migration adds the agent_uuid column to the messages table to support
-- sub-agent replay. Each message must be attributed to a specific agent so
-- the replay algorithm can query messages by agent_uuid to load each agent's
-- segment.
--
-- Replay query pattern:
--   SELECT * FROM messages
--   WHERE agent_uuid = $1
--     AND id > $2
--     AND ($3 = 0 OR id <= $3)
--   ORDER BY created_at
--
-- Design decisions:
-- - agent_uuid references agents(uuid) for referential integrity
-- - Composite index on (agent_uuid, id) for efficient range queries
-- - Column is nullable for backward compatibility with existing data
-- - New inserts should include agent_uuid

BEGIN;

-- Add agent_uuid column (nullable for existing data)
ALTER TABLE messages ADD COLUMN IF NOT EXISTS agent_uuid TEXT REFERENCES agents(uuid);

-- Index for efficient agent-based range queries
CREATE INDEX IF NOT EXISTS idx_messages_agent ON messages(agent_uuid, id);

UPDATE schema_metadata SET schema_version = 3 WHERE schema_version = 2;

COMMIT;
