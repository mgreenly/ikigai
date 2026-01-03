# Storage and Database Schema

Database schema and persistence for Structured Memory.

## StoredAssets Table

Stores all StoredAssets (both system skills and user blocks):

```sql
CREATE TABLE stored_assets (
  id SERIAL PRIMARY KEY,
  path TEXT UNIQUE NOT NULL,  -- "blocks/decisions.md", "skills/ddd.md"
  content TEXT,
  description TEXT,           -- Help text for agents
  created_at TIMESTAMP DEFAULT NOW(),
  updated_at TIMESTAMP DEFAULT NOW(),

  -- Metadata
  token_count INTEGER,        -- Cached token estimate
  read_only BOOLEAN DEFAULT FALSE,  -- System skills locked

  -- Optional grouping
  category TEXT               -- "skill", "block", "preference"
);

CREATE INDEX idx_stored_assets_path ON stored_assets(path);
CREATE INDEX idx_stored_assets_category ON stored_assets(category);
```

**Examples**:
```sql
INSERT INTO stored_assets (path, content, read_only, category) VALUES
  ('skills/ddd.md', '[skill content]', TRUE, 'skill'),
  ('blocks/decisions.md', '[decisions]', FALSE, 'block'),
  ('blocks/user-prefs.md', 'K&R style...', FALSE, 'preference');
```

## Asset Pins Table

Tracks which blocks are pinned (auto-included) per agent:

```sql
CREATE TABLE asset_pins (
  agent_id UUID REFERENCES agents(id),
  asset_path TEXT REFERENCES stored_assets(path),
  pinned_at TIMESTAMP DEFAULT NOW(),

  PRIMARY KEY (agent_id, asset_path)
);

CREATE INDEX idx_asset_pins_agent ON asset_pins(agent_id);
```

**Queries**:
```sql
-- Get pinned blocks for agent
SELECT sa.*
FROM stored_assets sa
JOIN asset_pins ap ON sa.path = ap.asset_path
WHERE ap.agent_id = $1
ORDER BY ap.pinned_at;

-- Pin a block
INSERT INTO asset_pins (agent_id, asset_path) VALUES ($1, $2)
ON CONFLICT DO NOTHING;

-- Unpin a block
DELETE FROM asset_pins
WHERE agent_id = $1 AND asset_path = $2;
```

## Session Summaries Table

Auto-generated summaries for each agent:

```sql
CREATE TABLE session_summaries (
  agent_id UUID PRIMARY KEY REFERENCES agents(id),
  content TEXT,               -- Markdown summary
  token_count INTEGER,
  last_updated TIMESTAMP DEFAULT NOW(),

  -- Track what's been summarized
  last_summarized_exchange_id INTEGER,
  last_aging_at TIMESTAMP     -- Last time sections were aged
);
```

**Queries**:
```sql
-- Get current summary
SELECT content FROM session_summaries WHERE agent_id = $1;

-- Update summary
INSERT INTO session_summaries (agent_id, content, token_count)
VALUES ($1, $2, $3)
ON CONFLICT (agent_id) DO UPDATE
SET content = $2, token_count = $3, last_updated = NOW();
```

## Messages Table (Extended)

Track sliding window status:

```sql
CREATE TABLE messages (
  id SERIAL PRIMARY KEY,
  agent_id UUID REFERENCES agents(id),
  session_id UUID,            -- Session grouping

  -- Message content
  role TEXT NOT NULL,         -- user, assistant, system
  content TEXT,
  tool_calls JSONB,           -- Array of tool calls
  tool_results JSONB,         -- Array of tool results

  -- Tokens
  token_count INTEGER,

  -- Sliding window tracking
  in_sliding_window BOOLEAN DEFAULT TRUE,
  evicted_at TIMESTAMP,
  exchange_id INTEGER,        -- Groups messages by exchange

  -- Timestamps
  created_at TIMESTAMP DEFAULT NOW()
);

CREATE INDEX idx_messages_agent_sliding ON messages(agent_id, in_sliding_window);
CREATE INDEX idx_messages_exchange ON messages(exchange_id);
```

**Queries**:
```sql
-- Get sliding window messages
SELECT * FROM messages
WHERE agent_id = $1 AND in_sliding_window = TRUE
ORDER BY created_at;

-- Get archival messages
SELECT * FROM messages
WHERE agent_id = $1 AND in_sliding_window = FALSE
ORDER BY created_at DESC;

-- Mark exchange as evicted
UPDATE messages
SET in_sliding_window = FALSE, evicted_at = NOW()
WHERE exchange_id = $1;
```

## Exchanges Table

Track exchange boundaries explicitly:

```sql
CREATE TABLE exchanges (
  id SERIAL PRIMARY KEY,
  agent_id UUID REFERENCES agents(id),

  -- Boundaries
  start_msg_id INTEGER REFERENCES messages(id),
  end_msg_id INTEGER REFERENCES messages(id),

  -- Metadata
  token_count INTEGER,
  created_at TIMESTAMP DEFAULT NOW(),

  -- Sliding window
  in_sliding_window BOOLEAN DEFAULT TRUE,
  evicted_at TIMESTAMP
);

CREATE INDEX idx_exchanges_agent ON exchanges(agent_id, in_sliding_window);
```

**Queries**:
```sql
-- Get exchanges in sliding window
SELECT * FROM exchanges
WHERE agent_id = $1 AND in_sliding_window = TRUE
ORDER BY created_at;

-- Calculate total sliding window tokens
SELECT SUM(token_count) FROM exchanges
WHERE agent_id = $1 AND in_sliding_window = TRUE;

-- Evict oldest exchange
UPDATE exchanges
SET in_sliding_window = FALSE, evicted_at = NOW()
WHERE id = (
  SELECT id FROM exchanges
  WHERE agent_id = $1 AND in_sliding_window = TRUE
  ORDER BY created_at
  LIMIT 1
);
```

## Summary Update Queue

Queue for background summary updates:

```sql
CREATE TABLE summary_update_queue (
  agent_id UUID PRIMARY KEY REFERENCES agents(id),
  priority INTEGER DEFAULT 0,
  queued_at TIMESTAMP DEFAULT NOW(),
  processing BOOLEAN DEFAULT FALSE,
  processing_started_at TIMESTAMP
);

CREATE INDEX idx_summary_queue_priority ON summary_update_queue(priority DESC, queued_at);
```

**Queries**:
```sql
-- Enqueue update
INSERT INTO summary_update_queue (agent_id, priority)
VALUES ($1, $2)
ON CONFLICT (agent_id) DO UPDATE
SET priority = GREATEST(summary_update_queue.priority, $2);

-- Dequeue for processing
UPDATE summary_update_queue
SET processing = TRUE, processing_started_at = NOW()
WHERE agent_id = (
  SELECT agent_id FROM summary_update_queue
  WHERE processing = FALSE
  ORDER BY priority DESC, queued_at
  LIMIT 1
)
RETURNING agent_id;

-- Mark complete
DELETE FROM summary_update_queue WHERE agent_id = $1;
```

## Budget Tracking Views

Useful views for monitoring budget usage:

```sql
-- Pinned block budget per agent
CREATE VIEW v_block_budget AS
SELECT
  ap.agent_id,
  SUM(sa.token_count) as total_tokens,
  COUNT(*) as block_count
FROM asset_pins ap
JOIN stored_assets sa ON ap.asset_path = sa.path
GROUP BY ap.agent_id;

-- Sliding window usage per agent
CREATE VIEW v_sliding_window_budget AS
SELECT
  agent_id,
  SUM(token_count) as total_tokens,
  COUNT(DISTINCT exchange_id) as exchange_count
FROM messages
WHERE in_sliding_window = TRUE
GROUP BY agent_id;

-- Combined budget view
CREATE VIEW v_context_budget AS
SELECT
  a.id as agent_id,
  COALESCE(bb.total_tokens, 0) as block_tokens,
  COALESCE(ss.token_count, 0) as summary_tokens,
  COALESCE(sw.total_tokens, 0) as window_tokens,
  COALESCE(bb.total_tokens, 0) +
    COALESCE(ss.token_count, 0) +
    COALESCE(sw.total_tokens, 0) as total_tokens
FROM agents a
LEFT JOIN v_block_budget bb ON a.id = bb.agent_id
LEFT JOIN session_summaries ss ON a.id = ss.agent_id
LEFT JOIN v_sliding_window_budget sw ON a.id = sw.agent_id;
```

**Usage**:
```sql
-- Check agent's budget
SELECT * FROM v_context_budget WHERE agent_id = $1;

-- Find agents near budget limit
SELECT agent_id, block_tokens, window_tokens, total_tokens
FROM v_context_budget
WHERE block_tokens > 90000  -- >90% of 100k budget
ORDER BY block_tokens DESC;
```

## Migrations

Schema evolution:

```sql
-- V1: Initial schema
CREATE TABLE stored_assets (...);
CREATE TABLE asset_pins (...);

-- V2: Add summaries
CREATE TABLE session_summaries (...);

-- V3: Add exchange tracking
ALTER TABLE messages ADD COLUMN exchange_id INTEGER;
CREATE TABLE exchanges (...);

-- V4: Add background queue
CREATE TABLE summary_update_queue (...);
```

## Backup and Archival

Critical data to backup:

```sql
-- Backup StoredAssets (user knowledge)
COPY stored_assets TO '/backup/stored_assets.csv' CSV HEADER;

-- Backup messages (conversation history)
COPY messages TO '/backup/messages.csv' CSV HEADER;

-- Backup summaries
COPY session_summaries TO '/backup/summaries.csv' CSV HEADER;
```

**Restore process**:
1. Restore stored_assets (knowledge base)
2. Restore messages (history)
3. Regenerate exchanges from messages
4. Regenerate summaries via background agents

## Performance Considerations

**Indexes**:
- `stored_assets(path)` - Fast lookups by URI
- `asset_pins(agent_id)` - Fast pinned block queries
- `messages(agent_id, in_sliding_window)` - Fast window queries
- `exchanges(agent_id, in_sliding_window)` - Fast eviction

**Token Count Caching**:
- Store pre-computed token counts
- Update on write, read cached value
- Avoid expensive re-estimation

**Query Optimization**:
```sql
-- Fast: Uses index
SELECT * FROM messages
WHERE agent_id = $1 AND in_sliding_window = TRUE;

-- Slow: No index on created_at alone
SELECT * FROM messages
WHERE created_at > NOW() - INTERVAL '1 day';

-- Fast: Composite index
CREATE INDEX idx_messages_agent_created
ON messages(agent_id, created_at);
```

## Storage Size Estimates

Typical storage requirements:

```
StoredAssets:
  - 50 blocks × 10k tokens × 4 bytes/token = 2MB per agent

Messages:
  - 10k messages × 1k tokens × 4 bytes = 40MB per agent
  - Plus metadata ~10MB
  - Total: ~50MB per agent

Summaries:
  - 10k tokens × 4 bytes = 40KB per agent

Total per agent: ~52MB
100 agents: ~5.2GB
```

PostgreSQL handles this easily.

---

Structured Memory's storage design balances:
- **Flexibility**: Store any content, any structure
- **Performance**: Fast queries via indexes
- **Integrity**: Foreign keys, constraints
- **Scalability**: Efficient for thousands of agents
