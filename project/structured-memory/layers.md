# Memory Layers

Structured Memory divides context into four distinct layers, each optimized for different purposes.

## Layer 1: Pinned Blocks (100k tokens)

**Purpose**: Curated persistent knowledge that should always be visible

**Characteristics**:
- High information density (manually curated)
- Slow-changing (weekly/monthly updates)
- User-controlled via `/pin` and `/unpin`
- Hard budget limit (writes fail if exceeded)

**Content Examples**:
- `skills/ddd.md` - Domain terminology and patterns
- `blocks/project-decisions.md` - Architectural choices
- `blocks/error-patterns.md` - Code patterns and conventions
- `blocks/user-preferences.md` - Coding style, preferences

**Management**:
- Manual: `/pin`, `/unpin` to control what's included
- Agent: `/compact` to increase information density
- Error back-pressure: Writes fail when budget exceeded

**Why Largest Allocation?**
Persistent knowledge has compounding value. The same patterns apply to many conversations over weeks/months. Maximizing this allocation provides the best ROI.

---

## Layer 2: Auto-Summary (10k tokens)

**Purpose**: Index to what fell off the sliding window

**Characteristics**:
- Low resolution (headlines only, not details)
- Auto-managed by background agents
- High churn (constantly updated as window slides)
- Time-based compression (this session → yesterday → this week)

**Content Format**:
```markdown
## Previously in This Session
1. Implemented JWT auth (45min ago, ~12k tokens)
2. Added rate limiting (30min ago, ~8k tokens)

## Yesterday
1. Designed database schema (~18k tokens)
2. Discussed error handling (~9k tokens)

## This Week
1. Researched libcurl HTTP/2 (Mon, ~25k tokens)
2. Evaluated PostgreSQL vs SQLite (Tue, ~15k tokens)

---
*Older history available via /recall*
```

**Management**:
- Fully automated by background agents
- Read-only to main agent
- Progressive compression as items age

**Purpose of Low Resolution**:
The summary doesn't need to contain the information, just enough to:
1. Remind the agent that discussion happened
2. Trigger suggestions to use `/recall`
3. Provide rough timestamps for archival search

**Why Small Allocation?**
A table of contents doesn't need detail. "We discussed X" is enough to trigger deeper search.

---

## Layer 3: Sliding Window (90k tokens)

**Purpose**: Recent working conversation

**Characteristics**:
- Medium information density (mix of signal and noise)
- Auto-managed FIFO eviction by exchange boundaries
- High churn (new exchanges constantly arriving)
- Everything preserved in archival before eviction

**Content**:
- User messages
- Assistant responses
- Tool calls and results
- Mark/rewind events
- Recent conversation flow

**Eviction Rules**:
```
If total_tokens > 90k:
  1. Remove oldest exchange atomically
  2. Add to auto-summary
  3. Repeat until under budget
```

**Exchange Boundaries**:
An exchange is atomic:
- User input
- Assistant tool calls
- Tool results
- Assistant final response

All parts evicted together (never orphan tool calls).

**Management**:
- Fully automatic
- No user intervention needed
- Always maintains recent context

**Why Large Allocation?**
Agents need room for substantial work:
- Multi-turn debugging sessions
- Long implementation discussions
- Back-and-forth exploration
- Tool-heavy workflows

---

## Layer 4: Archival Memory (unlimited)

**Purpose**: Permanent storage of everything forever

**Characteristics**:
- Zero context cost (not in LLM request)
- Infinite size (PostgreSQL storage)
- Searchable via `/recall`
- Never auto-loaded (explicit user/agent request only)

**Content**:
- Every message ever sent
- All tool calls and results
- Command history
- Evicted exchanges
- Session metadata

**Access Patterns**:
```bash
# Search by semantic content
/recall authentication discussion

# Search by time range
/recall yesterday's database decisions

# Search by topic
/recall error handling patterns from last week
```

**Management**:
- Write-only from agent perspective
- Read via explicit search tools
- Database maintains indexes for fast retrieval

**Why Unlimited?**
Disk is cheap. Context is expensive. Everything goes to archival, costs nothing until retrieved.

---

## Information Flow

Information naturally flows down through layers over time:

```
1. Conversation happens in Sliding Window
   ↓
2. Important knowledge extracted to Pinned Blocks (/remember)
   ↓
3. Old conversation evicted, summarized in Auto-Summary
   ↓
4. Old summary items compressed, eventually dropped
   ↓
5. Everything remains in Archival, searchable via /recall
```

This creates a **natural degradation gradient**:
- Highest value → Pinned blocks (manual curation)
- Recent work → Sliding window (automatic)
- Recent past → Auto-summary (compressed)
- Deep history → Archival (zero context cost)

---

## Layer Comparison

| Layer | Tokens | Management | Density | Churn | Purpose |
|-------|--------|------------|---------|-------|---------|
| **Pinned Blocks** | 100k | Manual + agent | High | Low | Persistent knowledge |
| **Auto-Summary** | 10k | Background agents | Low | High | Archival index |
| **Sliding Window** | 90k | Automatic FIFO | Medium | Medium | Working memory |
| **Archival** | ∞ | Write-only | N/A | None | Permanent storage |

---

## Design Rationale

**Why not one big context?**
Different information has different access patterns and value density. Treating everything the same is inefficient.

**Why manual pinning?**
Humans know what knowledge is valuable long-term. Agents are good at extraction, humans at curation.

**Why auto-summary instead of more sliding window?**
90k is enough for substantial work. Summary provides 10k of "pointers" to unlimited archival - better ROI.

**Why archival not in context?**
Most historical information isn't relevant to current work. On-demand retrieval prevents context pollution.

---

This layered architecture maximizes the value extracted from a fixed 200k token budget.
