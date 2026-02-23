-- Migration: 008-agent-reaped-status
-- Description: Add 'reaped' status to agent_status enum
--
-- This migration extends the agent_status enum to include 'reaped' status.
-- The 'reaped' status indicates agents that have been removed from memory
-- via the /reap command but are preserved in the database for audit trail
-- and to maintain foreign key integrity with messages and mail.
--
-- The /agents command filters for status IN ('running', 'dead'), so reaped
-- agents will not appear in the active agent list.

BEGIN;

-- Add 'reaped' to agent_status enum
ALTER TYPE agent_status ADD VALUE IF NOT EXISTS 'reaped';

-- Update schema version from 7 to 8
UPDATE schema_metadata SET schema_version = 8 WHERE schema_version = 7;

COMMIT;
