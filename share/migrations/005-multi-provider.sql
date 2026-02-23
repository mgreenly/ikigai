-- Migration: 005-multi-provider
-- Description: Add provider, model, and thinking_level columns to agents table
--
-- This migration adds three new columns to support multi-provider agent configuration:
-- - provider: LLM provider name (anthropic, openai, google, etc.)
-- - model: Model identifier (claude-opus-4.5, gpt-4o, etc.)
-- - thinking_level: Thinking budget/level for extended thinking models
--
-- All columns are nullable to support agents without provider configuration.
--
-- TRUNCATE: This is a developer-only migration for rel-07. All tables are truncated
-- to provide a clean slate for the new multi-provider architecture.
--
-- Rollback instructions:
-- To manually rollback this migration:
--   ALTER TABLE agents DROP COLUMN IF EXISTS provider;
--   ALTER TABLE agents DROP COLUMN IF EXISTS model;
--   ALTER TABLE agents DROP COLUMN IF EXISTS thinking_level;
--   UPDATE schema_metadata SET schema_version = 4;

BEGIN;

-- Add provider configuration columns
ALTER TABLE agents ADD COLUMN IF NOT EXISTS provider TEXT;
ALTER TABLE agents ADD COLUMN IF NOT EXISTS model TEXT;
ALTER TABLE agents ADD COLUMN IF NOT EXISTS thinking_level TEXT;

-- Clean slate for rel-07 (developer-only migration)
TRUNCATE TABLE agents CASCADE;
TRUNCATE TABLE messages CASCADE;
TRUNCATE TABLE mail CASCADE;
TRUNCATE TABLE sessions CASCADE;

-- Update schema version
UPDATE schema_metadata SET schema_version = 5 WHERE schema_version = 4;

COMMIT;
