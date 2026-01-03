# Background Agents

The auto-summary layer is maintained by background agents that run asynchronously.

## Purpose

Background agents continuously update the 10k auto-summary to reflect:
- Newly evicted exchanges from sliding window
- Time-based aging (this session → yesterday → this week)
- Compression to stay under budget

**User-facing agents never wait** for summary updates. Updates happen in background.

## Triggering Updates

### Event-Based Triggers

```c
// After each exchange completes
void on_exchange_complete(ik_agent_ctx_t *agent) {
    // Check if sliding window evicted anything
    int newly_evicted = db_count_newly_evicted_exchanges(agent);

    if (newly_evicted >= BATCH_SIZE) {
        // Queue summary update (low priority)
        enqueue_summary_update(agent->id, PRIORITY_LOW);
    }
}
```

**Batching**: Don't update for every single eviction. Wait for BATCH_SIZE (e.g., 3-5 exchanges) to accumulate.

### Time-Based Triggers

```c
// Periodic background worker
void summary_worker_thread() {
    while (true) {
        // Age existing summaries
        age_summaries_once_per_day();

        // Check for queued updates
        ik_agent_id_t agent_id = dequeue_summary_update();

        if (agent_id) {
            spawn_background_summary_agent(agent_id);
        }

        sleep(60); // Check every minute
    }
}
```

**Aging**: Once per day, run aging logic on all summaries:
- "This session" items older than 24h → move to "Yesterday"
- "Yesterday" items older than 7 days → move to "This week"
- "This week" items older than 14 days → drop entirely

## Background Agent Task

```c
void spawn_background_summary_agent(ik_agent_id_t agent_id) {
    // 1. Load current summary
    char *current_summary = db_read_session_summary(agent_id);

    // 2. Get newly evicted exchanges
    ik_exchange_t *evicted = db_get_newly_evicted_exchanges(agent_id);

    // 3. Spawn background agent with task
    ik_agent_ctx_t *bg_agent = create_background_agent(agent_id);

    char *prompt = format_string(
        "Update the session summary at ikigai:///summaries/%s.md\n"
        "\n"
        "Current summary:\n%s\n"
        "\n"
        "Newly evicted exchanges to add:\n%s\n"
        "\n"
        "Tasks:\n"
        "1. Add summaries for new exchanges to 'Previously in This Session'\n"
        "2. Merge related items where appropriate\n"
        "3. Age sections based on time (this session → yesterday → week)\n"
        "4. Compress if over 10k token budget\n"
        "5. Keep most recent items, compress or drop oldest\n"
        "\n"
        "Format:\n"
        "## Previously in This Session\n"
        "1. Item (timestamp, ~tokens)\n"
        "\n"
        "## Yesterday\n"
        "1. Item (~tokens)\n"
        "\n"
        "## This Week\n"
        "1. Item (day, ~tokens)\n",
        agent_id,
        current_summary,
        format_evicted_exchanges(evicted)
    );

    // 4. Execute asynchronously
    execute_background_task(bg_agent, prompt);
}
```

**Agent has full context**:
- Current summary content
- List of newly evicted exchanges with full messages
- Instructions on format and constraints

## Summary Update Algorithm

Background agent's reasoning process:

```
1. Read current summary sections
2. For each newly evicted exchange:
   - Determine importance (decision? implementation? debugging?)
   - Generate 1-line summary with timestamp and token count
   - Add to "This Session" section

3. Age sections:
   - Items in "This Session" older than 24h → move to "Yesterday"
   - Items in "Yesterday" older than 7d → move to "This Week"
   - Items in "This Week" older than 14d → drop

4. Compress if over budget:
   - Merge related items
   - Condense verbose summaries
   - Drop least important old items
   - Prioritize keeping recent items

5. Write updated summary
```

## Compression Strategies

When summary exceeds 10k tokens, background agent uses:

### Merging Related Items

**Before**:
```
## This Week
1. Added JWT authentication (Mon, ~8k tokens)
2. Fixed JWT token expiration bug (Mon, ~3k tokens)
3. Added JWT refresh token support (Tue, ~6k tokens)
```

**After**:
```
## This Week
1. Implemented JWT auth with refresh tokens (Mon-Tue, ~17k tokens)
```

### Condensing Verbose Summaries

**Before**:
```
1. Implemented comprehensive error handling using Result<T> pattern
   with OK() and ERR() macros for all failure cases (Wed, ~12k tokens)
```

**After**:
```
1. Added Result<T> error handling (Wed, ~12k tokens)
```

### Dropping Low-Importance Items

```
// High importance: Keep
- Architectural decisions
- API design choices
- Major feature implementations

// Medium importance: Compress
- Bug fixes
- Refactoring
- Test additions

// Low importance: Drop first
- Debugging sessions
- Exploratory dead ends
- Trivial changes
```

### Time-Based Prioritization

**Keep detail for recent, compress older**:

```
This Session (detailed):
  "Implemented JWT with RS256 signing, 15min expiry"

Yesterday (compressed):
  "Implemented JWT authentication"

This Week (very compressed):
  "Auth implementation"
```

## Emergency Compression

If background agent fails to stay under 10k:

```c
ik_result_t update_session_summary(ik_agent_ctx_t *agent,
                                    const char *new_summary) {
    int token_count = estimate_tokens(new_summary);

    if (token_count > SUMMARY_MAX_TOKENS) {
        // Agent failed to compress enough
        // System does emergency truncation

        // Keep most recent items up to budget
        char *truncated = truncate_to_budget(new_summary, SUMMARY_MAX_TOKENS);

        // Log warning
        LOG_WARN("Summary exceeded budget (%d tokens), emergency truncation applied",
                 token_count);

        return db_write_session_summary(agent->id, truncated);
    }

    return db_write_session_summary(agent->id, new_summary);
}
```

**Hard 10k limit enforced** even if background agent's compression is insufficient.

## Concurrency Handling

### Queue-Based Updates

```sql
CREATE TABLE summary_update_queue (
  agent_id UUID PRIMARY KEY,
  priority INTEGER,
  queued_at TIMESTAMP,
  processing BOOLEAN DEFAULT FALSE
);
```

**Single queue entry per agent**:
- Multiple evictions before processing? One update handles all
- Prevents duplicate background agents for same agent_id
- Priority determines processing order

### Lock-Free Reads

Main agent reads summary without waiting:

```c
char *get_current_summary(ik_agent_ctx_t *agent) {
    // Direct read, no locks
    return db_read_session_summary(agent->id);
}
```

**Summary is eventually consistent**. Main agent sees slightly stale summary until background agent finishes. This is acceptable - summary is advisory, not authoritative.

### Write Conflicts

```sql
-- Last write wins
UPDATE summary_documents
SET content = $1, updated_at = NOW()
WHERE agent_id = $2;
```

If multiple background agents run (shouldn't happen, but defensive):
- Both write to database
- Last write wins
- Next update will reconcile

**Risk is low**: Queue ensures one agent per agent_id at a time.

## Performance Characteristics

**Latency**:
- Main agent: Zero blocking (reads current summary instantly)
- Background updates: 5-30 seconds typically
- User never notices delay

**Frequency**:
- Updates trigger every BATCH_SIZE evictions (~3-5 exchanges)
- For typical conversation: Every 15-30 minutes
- Low overhead on system

**Token Cost**:
- Background agent request: ~15-20k tokens per update
- Small cost for maintaining index
- Amortized over many main agent requests (net savings)

## Error Handling

### Background Agent Fails

```c
void on_background_agent_error(ik_agent_id_t agent_id, ik_error_t *err) {
    LOG_ERROR("Summary update failed for agent %s: %s",
              agent_id, err->message);

    // Re-queue with lower priority
    enqueue_summary_update(agent_id, PRIORITY_RETRY);

    // Increment failure counter
    increment_failure_count(agent_id);

    if (get_failure_count(agent_id) > MAX_RETRIES) {
        // Give up, log alert
        LOG_ALERT("Summary updates failing repeatedly for agent %s", agent_id);
        // Admin intervention needed
    }
}
```

**Graceful degradation**: Summary becomes stale but main agent continues working.

### Database Unavailable

```c
char *get_current_summary(ik_agent_ctx_t *agent) {
    char *summary = db_read_session_summary(agent->id);

    if (!summary) {
        // Fallback to empty summary
        return "[Summary temporarily unavailable]";
    }

    return summary;
}
```

Main agent sees placeholder. Not ideal, but not fatal.

## Monitoring

### Metrics to Track

```c
typedef struct {
    int updates_queued;
    int updates_completed;
    int updates_failed;
    int avg_update_duration_ms;
    int avg_summary_size_tokens;
} ik_summary_metrics_t;
```

**Dashboard view**:
```
Summary Updates (last 24h):
  Completed: 1,247
  Failed: 3 (0.2%)
  Avg duration: 8.2s
  Avg size: 7.8k/10k tokens
```

### Alerts

- Update failure rate >5%
- Average duration >30s
- Average size >9.5k tokens (approaching limit)
- Queue depth >50 (backlog building)

## Future Optimizations

### Incremental Updates

Instead of rewriting entire summary:

```json
{
  "action": "append",
  "section": "this_session",
  "items": [
    "Implemented feature X (10min ago, ~5k tokens)"
  ]
}
```

Faster than full read-analyze-write cycle.

### Compression Heuristics

Pre-compute compression strategies:

```c
// Fast path: If under 8k, just append
if (current_size + new_items_size < 8000) {
    return quick_append(summary, new_items);
}

// Medium path: Merge recent items only
if (current_size < 9500) {
    return merge_recent_section(summary, new_items);
}

// Slow path: Full rewrite with aggressive compression
return full_compress(summary, new_items);
```

Avoid expensive LLM call when simple append works.

### Shared Summaries

For collaborative agents working on same project:

```sql
CREATE TABLE shared_summaries (
  project_id UUID,
  content TEXT
);

-- Multiple agents contribute to one summary
```

More complex but enables team context.

---

Background agents make auto-summary feel "magical" - it's always up-to-date without the main agent or user doing anything.
