# Database Schema Changes

## Overview

Multi-provider support requires storing provider, model, and thinking level with each agent and message. This enables session restoration and per-message attribution.

## Migration Strategy

**Migration file:** `migrations/005-multi-provider.sql`

Follows existing convention: `NNN-kebab-case-description.sql` (see 001-004 in `migrations/`).

**For rel-07:** Truncate all tables (clean slate for developer).

```sql
-- 005-multi-provider.sql
-- Multi-provider support: add provider fields, truncate for clean slate

BEGIN;

-- Add new columns to agents table
ALTER TABLE agents ADD COLUMN IF NOT EXISTS provider TEXT;
ALTER TABLE agents ADD COLUMN IF NOT EXISTS model TEXT;
ALTER TABLE agents ADD COLUMN IF NOT EXISTS thinking_level TEXT;

-- Clean slate for developer dogfooding
TRUNCATE agents CASCADE;

COMMIT;
```

**Transaction Safety:** Migration is wrapped in BEGIN/COMMIT. If any statement fails, the entire migration rolls back, preventing partial schema changes or data loss.

**Future migrations** (for users beyond developer) would include:
- Backfill existing rows with provider='openai'
- Infer model from existing data
- Default thinking_level='none'

## Schema Changes

### agents Table

**Before (rel-06):**
```sql
CREATE TABLE agents (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    parent_id TEXT REFERENCES agents(id) ON DELETE CASCADE,
    depth INTEGER NOT NULL DEFAULT 0
);
```

**After (rel-07):**
```sql
CREATE TABLE agents (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    parent_id TEXT REFERENCES agents(id) ON DELETE CASCADE,
    depth INTEGER NOT NULL DEFAULT 0,

    -- New columns
    provider TEXT,        -- "anthropic", "openai", "google", etc.
    model TEXT,           -- Provider-specific model ID
    thinking_level TEXT   -- "none", "low", "med", "high"
);
```

**Notes:**
- `provider`, `model`, `thinking_level` may be NULL for newly created agents
- User must set via `/model` command before sending first message
- Stored as TEXT (not foreign key) - simple and flexible

### messages Table (data column)

The `messages.data` JSONB column structure changes:

**Before (rel-06):**
```json
{
  "input_tokens": 1234,
  "output_tokens": 5678,
  "model": "gpt-4o"
}
```

**After (rel-07):**
```json
{
  "provider": "anthropic",
  "model": "claude-sonnet-4-5-20250929",
  "thinking_level": "med",

  "thinking": "Let me analyze this...",  // Thinking summary (if available)
  "thinking_tokens": 1234,

  "input_tokens": 5000,
  "output_tokens": 2000,
  "total_tokens": 8234,

  "provider_data": {                     // Opaque provider-specific data
    "thought_signature": "encrypted...",  // Google Gemini 3 only
    "finish_reason": "end_turn"          // Provider-specific finish reason
  }
}
```

**Fields:**

| Field | Type | Description | Required |
|-------|------|-------------|----------|
| `provider` | string | Provider that generated response | Yes (assistant messages) |
| `model` | string | Model that generated response | Yes (assistant messages) |
| `thinking_level` | string | Thinking level used | Yes (assistant messages) |
| `thinking` | string | Thinking/reasoning summary | Optional |
| `thinking_tokens` | integer | Thinking token count | Optional |
| `input_tokens` | integer | Input/prompt tokens | Yes |
| `output_tokens` | integer | Output tokens (excluding thinking) | Yes |
| `total_tokens` | integer | Sum of all tokens | Yes |
| `provider_data` | object | Opaque provider-specific metadata | Optional |

**No schema migration needed** - JSONB is flexible. New fields are added automatically.

## Queries

### Get Agent with Provider Info

```sql
SELECT id, name, provider, model, thinking_level
FROM agents
WHERE id = $1;
```

### Get Conversation with Attribution

```sql
SELECT
    m.id,
    m.role,
    m.content,
    m.data->>'provider' AS provider,
    m.data->>'model' AS model,
    m.data->>'thinking_level' AS thinking_level,
    m.data->>'thinking' AS thinking_text,
    m.data->>'thinking_tokens' AS thinking_tokens,
    m.data->>'input_tokens' AS input_tokens,
    m.data->>'output_tokens' AS output_tokens
FROM messages m
WHERE m.agent_id = $1
ORDER BY m.created_at ASC;
```

### Find Agents by Provider

```sql
SELECT id, name, model
FROM agents
WHERE provider = 'anthropic'
ORDER BY created_at DESC;
```

### Thinking Token Usage Stats

```sql
SELECT
    data->>'provider' AS provider,
    data->>'model' AS model,
    SUM((data->>'thinking_tokens')::integer) AS total_thinking_tokens,
    SUM((data->>'output_tokens')::integer) AS total_output_tokens,
    COUNT(*) AS message_count
FROM messages
WHERE role = 'assistant'
  AND data->>'thinking_tokens' IS NOT NULL
GROUP BY provider, model
ORDER BY total_thinking_tokens DESC;
```

## Indexes

**Existing indexes** (from rel-06):
```sql
CREATE INDEX idx_messages_agent_id ON messages(agent_id);
CREATE INDEX idx_agents_parent_id ON agents(parent_id);
```

**New indexes** (optional, for performance):
```sql
-- Find agents by provider
CREATE INDEX idx_agents_provider ON agents(provider);

-- Find messages by provider (for analytics)
CREATE INDEX idx_messages_provider ON messages((data->>'provider'));
```

**Note:** These indexes are optional for rel-07. Add if performance analysis shows benefit.

## Data Integrity

### Constraints

```sql
-- Ensure thinking_level is valid
ALTER TABLE agents ADD CONSTRAINT agents_thinking_level_check
    CHECK (thinking_level IN ('none', 'low', 'med', 'high') OR thinking_level IS NULL);

-- Ensure provider is known (if set)
ALTER TABLE agents ADD CONSTRAINT agents_provider_check
    CHECK (provider IN ('anthropic', 'openai', 'google', 'xai', 'meta') OR provider IS NULL);
```

**Note:** Constraints not added in rel-07 (flexibility > strictness). Consider for future releases.

### Validation in Code

Application-level validation should enforce:
- `thinking_level` is one of: "none", "low", "med", "high"
- `provider` is one of: "anthropic", "openai", "google", "xai", "meta"

Example update query:
```sql
UPDATE agents SET provider = $1, model = $2, thinking_level = $3
WHERE id = $4;
```

## Migration Testing

Verify migration success by checking that new columns exist:

```sql
-- Test query: should succeed after migration
SELECT provider, model, thinking_level FROM agents LIMIT 1;
```

Integration tests should verify:
- Migration runs without errors
- New columns are queryable
- NULL values are handled correctly
- Truncate cascade works as expected

## Rollback Plan

If migration fails or causes issues:

```sql
-- Rollback: remove columns
ALTER TABLE agents DROP COLUMN provider;
ALTER TABLE agents DROP COLUMN model;
ALTER TABLE agents DROP COLUMN thinking_level;

-- Restore from backup if needed
```

**For rel-07:** Rollback is simple (truncate), no data to preserve.

## Provider Data Examples

### Anthropic

```json
{
  "provider": "anthropic",
  "model": "claude-sonnet-4-5-20250929",
  "thinking_level": "med",
  "thinking": "I should provide a concise answer...",
  "thinking_tokens": 1234,
  "input_tokens": 5000,
  "output_tokens": 2000,
  "total_tokens": 8234,
  "provider_data": {
    "finish_reason": "end_turn",
    "stop_sequence": null
  }
}
```

### OpenAI

```json
{
  "provider": "openai",
  "model": "o3",
  "thinking_level": "med",
  "thinking": null,  // Not exposed by default
  "thinking_tokens": 3456,
  "input_tokens": 4000,
  "output_tokens": 1500,
  "total_tokens": 8956,
  "provider_data": {
    "finish_reason": "stop",
    "response_id": "resp_abc123"
  }
}
```

### Google

```json
{
  "provider": "google",
  "model": "gemini-3-pro",
  "thinking_level": "high",
  "thinking": "Step 1: Analyze the question...",
  "thinking_tokens": 2000,
  "input_tokens": 6000,
  "output_tokens": 3000,
  "total_tokens": 11000,
  "provider_data": {
    "thought_signature": "encrypted_signature_string",
    "finish_reason": "STOP",
    "safety_ratings": [...]
  }
}
```

## Future Schema Extensions

### Cost Tracking (rel-08+)

```sql
ALTER TABLE messages ADD COLUMN cost_usd DECIMAL(10, 6);
```

Calculate based on token counts and provider pricing.

### Model Aliases (rel-08+)

```sql
ALTER TABLE agents ADD COLUMN model_alias TEXT;
```

Store user-friendly alias (e.g., "fast", "smart") separate from actual model ID.

### Thinking Visibility Preference (rel-08+)

```sql
ALTER TABLE agents ADD COLUMN show_thinking BOOLEAN DEFAULT true;
```

Per-agent preference for displaying thinking content.

## Backup and Recovery

### Backup Before Migration

```bash
pg_dump ikigai > backup-pre-rel07.sql
```

### Restore if Needed

```bash
dropdb ikigai
createdb ikigai
psql ikigai < backup-pre-rel07.sql
```

**For rel-07:** No backup needed (truncating anyway).

## Performance Considerations

### JSONB Indexing

For frequent queries on provider/model:

```sql
-- GIN index for JSONB queries
CREATE INDEX idx_messages_data_gin ON messages USING gin(data);

-- Specific path index
CREATE INDEX idx_messages_provider ON messages((data->>'provider'));
```

**Note:** Monitor query performance. Add indexes if slow.

### Vacuum After Truncate

```sql
VACUUM ANALYZE agents;
VACUUM ANALYZE messages;
```

Reclaim space and update statistics after truncation.
