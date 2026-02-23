-- Migration: 004-mail-table
-- Description: Create mail table for inter-agent messaging
--
-- This migration creates the mail table for Erlang-style message passing between agents.
-- Each message is scoped to a session and includes sender/recipient UUIDs, body, and read status.
--
-- Design decisions:
-- - session_id scopes mail to current session
-- - CASCADE delete removes mail when session deleted
-- - Index optimizes inbox queries (check unread mail by recipient)

BEGIN;

CREATE TABLE IF NOT EXISTS mail (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    from_uuid TEXT NOT NULL,
    to_uuid TEXT NOT NULL,
    body TEXT NOT NULL,
    timestamp BIGINT NOT NULL,
    read INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_mail_recipient
ON mail(session_id, to_uuid, read);

UPDATE schema_metadata SET schema_version = 4 WHERE schema_version = 3;

COMMIT;
