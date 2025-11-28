-- Migration: 001-initial-schema
-- Description: Initial database schema for ikigai event stream architecture
--
-- This migration creates the foundational schema for persistent conversation storage.
-- The design uses an event stream model where the messages table stores individual
-- events (clear, system, user, assistant, mark, rewind) rather than full conversation
-- snapshots. This enables complete audit trails and replay functionality.
--
-- Event Kinds:
--   - clear: Context reset (session start or /clear command)
--   - system: System prompt message
--   - user: User input message
--   - assistant: LLM response message
--   - mark: Checkpoint created by /mark command
--   - rewind: Rollback operation created by /rewind command
--
-- The schema supports Model B (Continuous Sessions) where sessions persist across
-- app launches and are only ended explicitly via /new-session command.

BEGIN;

-- Schema versioning table to track applied migrations
CREATE TABLE IF NOT EXISTS schema_metadata (
    schema_version INTEGER PRIMARY KEY
);

-- Sessions table: Groups messages by launch session
-- Sessions persist across app launches until explicitly ended
CREATE TABLE IF NOT EXISTS sessions (
    id BIGSERIAL PRIMARY KEY,
    started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at TIMESTAMPTZ,          -- NULL indicates active session
    title TEXT                      -- Optional user-defined session title
);

-- Index for finding recent sessions and active session lookup
CREATE INDEX IF NOT EXISTS idx_sessions_started ON sessions(started_at DESC);

-- Messages table: Event stream storage
-- Each row represents a single event in the conversation timeline
CREATE TABLE IF NOT EXISTS messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    kind TEXT NOT NULL,                     -- Event type discriminator
    content TEXT,                           -- Message text (NULL for clear events)
    data JSONB,                             -- Event-specific metadata (LLM params, tokens, rewind targets)
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Composite index for session message retrieval and replay
-- Ordering by created_at enables chronological event stream processing
CREATE INDEX IF NOT EXISTS idx_messages_session ON messages(session_id, created_at);

-- Full-text search index for content search across all messages
-- Uses PostgreSQL's GIN index with English text search configuration
CREATE INDEX IF NOT EXISTS idx_messages_search ON messages USING gin(to_tsvector('english', content));

-- Record schema version 1 as the current version
-- This enables future migrations to check current version and apply updates incrementally
INSERT INTO schema_metadata (schema_version)
VALUES (1)
ON CONFLICT (schema_version) DO NOTHING;

COMMIT;
