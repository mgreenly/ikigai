# Token Budget Allocation

Structured Memory treats the 200k context window as a resource allocation problem.

## Fixed Budget

```
Total Context: 200k tokens (Claude Sonnet 4.5)

Base Allocation:
├─ System Prompt:        10k  (5%)   - Instructions, tool definitions
├─ Pinned Blocks:       100k (50%)   - Curated persistent knowledge
├─ Auto-Summary:         10k  (5%)   - Index to archival memory
└─ Sliding Window:       90k (40%)   - Recent working conversation
                       ─────
                        210k allocated

Reserve:                -10k         - Safety margin, overhead
                       ─────
                        200k total
```

## Allocation Rationale

### System Prompt: 10k (5%)

**Purpose**: Operating instructions
**Why minimal?** Necessary overhead. Every token here is one less for actual work.

**Optimization strategy**: Compress ruthlessly
- Terse instruction style
- Remove examples where possible
- Reference external docs instead of inline

**Trade-off**: Clarity vs space. System prompt must be understandable but shouldn't waste tokens.

---

### Pinned Blocks: 100k (50%)

**Purpose**: Always-available persistent knowledge
**Why largest?** Highest ROI over time.

**Value proposition**:
- Patterns reused across many conversations
- Decisions referenced for weeks/months
- Knowledge compounds (new blocks build on old)
- Saves re-explaining same concepts

**Optimization strategy**: Manual curation + compression
- User pins high-value content
- `/compact` increases information density
- Trade blocks in/out as focus shifts

**Trade-off**: More blocks → less sliding window. User decides balance via `/pin`/`/unpin`.

**Example ROI**:
```
Without pinned block:
- Explain error handling pattern: 2k tokens × 10 conversations = 20k wasted

With pinned block:
- Error pattern block: 3k tokens × 1 (always present) = 3k total
- Savings: 17k tokens over 10 conversations
```

---

### Auto-Summary: 10k (5%)

**Purpose**: Table of contents for archival
**Why small?** Just needs to be a pointer, not the content.

**Value proposition**:
- Agent knows what exists in archival
- Triggers "did you mean to /recall X?" suggestions
- Provides rough timestamps for search

**Optimization strategy**: Aggressive compression
- Headlines only, no details
- Time-based aging (this session → yesterday → week)
- Hard 10k limit, background agents compress

**Trade-off**: Detail vs coverage. Better to cover more topics shallowly than few deeply.

**Why not larger?**
10k covers ~30-50 summary items. That's enough breadth. Details available via `/recall`.

---

### Sliding Window: 90k (40%)

**Purpose**: Recent working conversation
**Why large?** Agents need room to work.

**Value proposition**:
- Substantial multi-turn conversations
- Long debugging sessions
- Tool-heavy workflows without constant eviction
- Enough context to maintain flow

**Optimization strategy**: Automatic FIFO
- No user management needed
- Exchange-boundary eviction
- Everything goes to archival first

**Trade-off**: Larger window → less room for blocks. 90k empirically good balance.

**Sizing rationale**:
```
Typical exchange: 2-5k tokens
90k window: ~20-40 exchanges
Conversation duration: 1-3 hours of continuous work

Adequate for:
- Long feature implementation
- Extended debugging session
- Research and synthesis
- Design discussion
```

---

## Budget Enforcement

### Pinned Blocks: Hard Limit with Back-Pressure

```c
if (new_total_block_size > 100k) {
    return ERR("Block budget exceeded: %dk/100k tokens. "
               "Use /compact or /unpin to free space.",
               current_total/1000);
}
```

**Error drives behavior**:
- Agent sees error message
- Tries `/compact` on existing blocks
- Or writes less content
- Or suggests user `/unpin` something

**No silent failures**: Agent must explicitly handle budget pressure.

---

### Auto-Summary: Emergency Compression

```c
if (summary_tokens > 10k) {
    // Background agent failed to compress enough
    char *compressed = emergency_compress_summary(summary, 10k);
    // Keep most recent items, drop oldest
}
```

**Hard truncation if needed**: System guarantees ≤10k, even if background agent fails.

---

### Sliding Window: Automatic Eviction

```c
while (sliding_window_tokens > 90k) {
    exchange_t *oldest = get_oldest_exchange();
    evict_exchange(oldest);
    add_to_summary(oldest);
}
```

**No errors**: Silent automatic management. User never sees eviction happening.

---

## Allocation Strategies for Different Use Cases

### Long Document Reading

```
System:      10k
Blocks:      30k  (↓ don't need many patterns)
Summary:     10k
Window:     150k  (↑ need room to load large docs)
```

**Use case**: Reading API docs, analyzing large files
**Strategy**: Temporarily unpin blocks, expand window

---

### Pattern-Heavy Development

```
System:      10k
Blocks:     150k  (↑ many pinned patterns/decisions)
Summary:     10k
Window:      30k  (↓ short exchanges, lots of patterns)
```

**Use case**: Following strict coding standards, complex domain
**Strategy**: Pin many blocks, smaller exchanges sufficient

---

### Balanced (Default)

```
System:      10k
Blocks:     100k
Summary:     10k
Window:      90k
```

**Use case**: General development work
**Strategy**: Good mix of patterns and working space

---

## Dynamic Allocation (Future)

Could allow user-configurable ratios:

```bash
# Expand sliding window, reduce blocks
/allocate window=120k blocks=70k

# Expand blocks, reduce window
/allocate blocks=140k window=50k

# Reset to defaults
/allocate reset
```

**Constraints**:
- System + Summary always 20k (fixed)
- Blocks + Window = 180k (adjustable split)
- User must explicitly change (no auto-adjustment)

---

## Information Density Economics

Different layers have different **value per token**:

| Layer | Tokens | Reuse | Value/Token |
|-------|--------|-------|-------------|
| **Pinned Blocks** | 100k | 100x (many conversations) | Very High |
| **Sliding Window** | 90k | 1x (current session) | Medium |
| **Auto-Summary** | 10k | ~10x (pointers to archival) | High |
| **System Prompt** | 10k | ∞ (every request) | Extreme |

**Optimization goal**: Maximize total value across all layers.

Pinned blocks get largest allocation because:
- High reuse (same knowledge, many conversations)
- Cumulative (new knowledge builds on old)
- Manually curated (high quality)

---

## Budget Visualization (Future)

Status bar shows allocation in real-time:

```
┌─────────────────────────────────────────────────────────┐
│ Blocks: 87k/100k  Summary: 8k/10k  Window: 82k/90k  🟢  │
└─────────────────────────────────────────────────────────┘

🟢 All budgets healthy
🟡 One budget >80% (warning)
🔴 One budget >95% (critical)
```

Clicking shows detailed breakdown:

```
Pinned Blocks (87k/100k):
  skills/ddd                12k
  skills/developer          8k
  blocks/decisions         35k
  blocks/error-patterns    18k
  blocks/current-task      14k

Sliding Window (82k/90k):
  23 exchanges, oldest from 2h ago

Auto-Summary (8k/10k):
  14 items (5 this session, 4 yesterday, 5 this week)
```

---

## The Key Insight

**Context windows aren't just capacity limits - they're resource allocation problems.**

Treating them as such enables:
- Strategic allocation by value
- Active management by user/agent
- Graceful degradation under pressure
- Maximum value extraction from fixed budget

This is fundamentally different from "send the last N messages and hope."
