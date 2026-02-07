-- Migration: 006-idle-column
-- Description: Add idle column to agents table for internal tool infrastructure
--
-- This migration adds the idle column to track when agents are waiting for
-- external events (mail, user input, etc.). Used by internal tools to signal
-- when they've deferred work and the agent should not immediately continue
-- the tool loop.
--
-- Key design decisions:
-- - idle is BOOLEAN NOT NULL DEFAULT false
-- - All existing running agents are marked idle (fixup for recovery)

BEGIN;

-- Add idle column (idempotent via IF NOT EXISTS pattern)
DO $$ BEGIN
    ALTER TABLE agents ADD COLUMN idle BOOLEAN NOT NULL DEFAULT false;
EXCEPTION
    WHEN duplicate_column THEN NULL;
END $$;

-- Fixup: mark all currently running agents as idle
-- (Safe to run multiple times - idempotent)
UPDATE agents SET idle = true WHERE status = 'running';

-- Update schema version from 5 to 6
UPDATE schema_metadata SET schema_version = 6 WHERE schema_version = 5;

COMMIT;
