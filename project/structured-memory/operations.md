# Memory Operations

Commands and workflows for managing Structured Memory.

## Pinned Block Management

### `/pin` - Auto-Include a Block

```bash
/pin skills/ddd
/pin blocks/project-decisions
```

**Effect**: Block is always included in LLM context

**Behavior**:
- Reads block from `ikigai:///skills/ddd.md`
- Adds to agent's pinned set
- Included in all future requests
- Counts toward 100k block budget

**Use when**:
- Starting new work phase (pin relevant patterns)
- Need constant reference to decisions/standards
- Context needed for extended period

---

### `/unpin` - Remove from Auto-Include

```bash
/unpin skills/research
/unpin blocks/old-feature
```

**Effect**: Block no longer included automatically

**Behavior**:
- Removes from pinned set
- Frees tokens from block budget
- Content remains in database (can re-pin later)
- Agent can still explicitly read with `file_read`

**Use when**:
- Switching focus areas
- Need to free block budget
- Information no longer immediately relevant

---

### `/pinned` - List Current Pins

```bash
/pinned
```

**Output**:
```
Pinned Blocks (87k/100k tokens):
  skills/ddd                    12k  [ro]
  skills/developer              8k   [ro]
  blocks/project-decisions     35k
  blocks/error-patterns        18k
  blocks/current-task          14k

[ro] = read-only (system skills)

Commands:
  /compact <block>  - Compress a block
  /unpin <block>    - Remove from context
  /blocks           - Show all available blocks
```

---

### `/blocks` - List All Available Blocks

```bash
/blocks
```

**Output**:
```
Available Memory Blocks:

Pinned (auto-included):
  skills/ddd                    12k  [ro]
  blocks/project-decisions     35k

Unpinned:
  skills/research              15k   [ro]
  skills/task-authoring        9k    [ro]
  blocks/api-research          22k
  blocks/archive-feature       18k

Use /pin <label> to auto-include a block
```

---

## Knowledge Extraction

### `/remember` - Extract from Conversation

```bash
# Agent decides where to save
/remember error handling decisions

# User specifies destination
/remember database schema → ikigai:///blocks/db-decisions.md

# Explicit content
/remember We decided to use PostgreSQL with event sourcing
```

**Workflow**:
```
User: /remember error handling patterns

Agent: Reviewing recent conversation for error handling patterns...

Found:
- Use Result<T> with OK()/ERR() macros (3 examples)
- PANIC only for unrecoverable errors (2 examples)
- TRY() for error propagation (5 examples)

Append to which block?
1. blocks/error-patterns (recommended)
2. blocks/coding-standards
3. Create new block

User: 1

[Agent calls file_write with append mode]
Done. Added 3 patterns to error-patterns block.
```

**Agent behavior**:
- Reviews sliding window (or archival if specified)
- Extracts key information
- Proposes or creates target block
- Appends or merges content
- May auto-pin if not already pinned

**Use when**:
- After making important decisions
- Learned new patterns/approaches
- Want to preserve context before it evicts

---

## Content Optimization

### `/compact` - Compress a Block

```bash
/compact blocks/project-decisions
/compact ikigai:///blocks/research-notes.md
```

**Workflow**:
```
User: /compact blocks/decisions

Agent: Analyzing project-decisions block (8.5k tokens)...

Found opportunities:
- 3 decisions described redundantly (merge)
- 2 decisions superseded by later ones (remove)
- 5 decisions can be compressed (condense)

Proposed compression: 8.5k → 5.2k tokens (38% reduction)

Before (excerpt):
  "We decided to use PostgreSQL for the database layer
   because it provides ACID guarantees, has excellent JSON
   support via JSONB, and offers robust full-text search..."

After:
  "Database: PostgreSQL (ACID, JSONB, FTS)"

Proceed?

User: yes

[Agent rewrites block]
Done. Compacted to 5.2k tokens.
Block budget: 82k/100k (freed 3.3k)
```

**Agent behavior**:
- Reads current block content
- Analyzes for:
  - Duplication
  - Obsolete information
  - Verbose phrasing
  - Mergeable sections
- Rewrites with compressed content
- Shows before/after preview
- Waits for user approval

**Use when**:
- Block budget pressure (>90% full)
- Block write failed with budget error
- Periodic maintenance
- Before pinning large block

**Works on filesystem too**:
```bash
/compact docs/architecture.md
```

Same logic, compresses regular files.

---

### `/forget` - Remove Content

```bash
# From conversation (existing behavior)
/forget database migration discussion

# From memory block (new)
/forget GraphQL designs --from blocks/api-decisions
```

**Workflow (block scope)**:
```
User: /forget old authentication approaches --from blocks/auth

Agent: Searching blocks/auth for old authentication approaches...

Found:
- Section: "Session-based auth" (decided against, 800 tokens)
- Section: "OAuth2 exploration" (not pursued, 600 tokens)

Remove from block?

User: yes

[Agent edits block to remove sections]
Done. Removed 1.4k tokens from auth block.
Block budget: 85k/100k (freed 1.4k)
```

**Agent behavior**:
- Searches block for matching content
- Shows what will be removed
- Waits for confirmation
- Edits block to remove sections
- Reports tokens freed

**Use when**:
- Removing obsolete decisions
- Cleaning up exploratory dead ends
- Making room in block budget

---

### `/refresh` - Rebuild from Source

```bash
# Rebuild block from archival memory
/refresh ikigai:///blocks/api-decisions.md

# Rebuild file from git history
/refresh docs/architecture.md
```

**Workflow (memory block)**:
```
User: /refresh blocks/error-patterns

Agent: Rebuilding error-patterns from archival memory...

Searching all past conversations for error handling...
Found 85 messages across 6 sessions.

Extracting patterns:
- Result<T> usage (23 examples)
- PANIC scenarios (8 examples)
- Error propagation (31 examples)

Rebuild block with synthesized patterns?

User: yes

[Agent generates fresh summary from archival search]
[Overwrites block]

Done. Block refreshed (6.8k tokens).
```

**Workflow (filesystem)**:
```
User: /refresh docs/api-decisions.md

Agent: Rebuilding from git history...

Found 47 commits mentioning API:
- Initial design (3 weeks ago)
- Versioning added (2 weeks ago)
- REST conversion (1 week ago)

Rebuild document from commit history?

User: yes

[Agent analyzes commits, generates summary]
Done. Document refreshed (4.2k tokens).
```

**Use when**:
- Block drift (manually edited, now inconsistent)
- Want fresh synthesis of old discussions
- Consolidating multiple sessions
- Recovering from bad /compact

---

## Context Inspection

### `/history` - View Sliding Window

```bash
/history
```

**Output**:
```
Sliding Window (82k/90k tokens):

[Exchanges outside window - in archival only]
  Exchange 1: "Implement auth" (evicted 3h ago, 4.2k)
  Exchange 2: "Add tests" (evicted 2h ago, 3.8k)

[Exchanges in window - currently visible]
  Exchange 3: "Refactor error handling" (1h ago, 8.5k)
  Exchange 4: "Update docs" (30min ago, 5.2k)
  Exchange 5: "Add logging" (current, 2.1k in progress)

Use /recall to search evicted exchanges
```

---

### `/summary` - View Auto-Generated Summary

```bash
/summary
```

**Output**: Shows current auto-summary content

```markdown
## Previously in This Session

1. Implemented JWT authentication with refresh tokens (45min ago, ~12k tokens)
2. Added API rate limiting using token bucket algorithm (30min ago, ~8k tokens)

## Yesterday

1. Designed user database schema with roles and permissions (~18k tokens)
2. Discussed error handling patterns: Result<T> vs exceptions (~9k tokens)

## This Week

1. Researched libcurl HTTP/2 multiplexing support (Mon, ~25k tokens)
2. Evaluated PostgreSQL vs SQLite for agent persistence (Tue, ~15k tokens)

---
*Older history available via /recall*
```

---

### `/recall` - Search Archival Memory

```bash
/recall authentication discussion
/recall yesterday's API decisions
/recall error patterns from last week
```

**Workflow**:
```
User: /recall authentication approaches

Agent: Searching archival memory for "authentication approaches"...

Found 3 conversations:

1. Dec 15 (5 exchanges, 12k tokens):
   "Initial auth design - discussed JWT vs sessions"

2. Dec 17 (8 exchanges, 18k tokens):
   "Implemented JWT with refresh token rotation"

3. Dec 18 (3 exchanges, 6k tokens):
   "Added OAuth2 provider support"

Load which conversation?

User: 2

[Agent loads exchanges into context]

Here's the JWT implementation discussion from Dec 17:
[Shows relevant exchanges]
```

**Agent behavior**:
- Searches archival database
- Shows matching conversations with metadata
- User selects which to load
- Agent presents content (doesn't add to sliding window, just displays)

---

## Workflow Examples

### Starting New Feature Work

```bash
# Pin relevant context
/pin skills/ddd
/pin blocks/coding-standards

# Unpin unrelated context
/unpin blocks/old-feature

# Check budget
/pinned
# Shows: 65k/100k - plenty of room
```

---

### Budget Pressure Response

```bash
# Agent tries to write large block
[Tool error: "Block budget exceeded: 98k/100k"]

Agent: "Block budget nearly full. I'll compact existing blocks first."

[Agent runs /compact blocks/decisions]
[Frees 3k tokens]

[Agent retries write]
[Succeeds]
```

---

### Knowledge Consolidation

```bash
# After long implementation session
/remember database migration strategy

# After research session
/remember libcurl HTTP/2 findings → blocks/research-notes

# Clean up old decisions
/forget abandoned approaches --from blocks/api-design

# Compress regularly referenced block
/compact blocks/coding-standards
```

---

### Context Archaeology

```bash
# See what fell off window
/history

# Check summary for old topics
/summary

# Deep dive into old discussion
/recall authentication from 2 weeks ago
```

---

## Command Summary

| Command | Scope | Purpose |
|---------|-------|---------|
| `/pin` | Block management | Auto-include block |
| `/unpin` | Block management | Remove from auto-include |
| `/pinned` | Inspection | Show pinned blocks |
| `/blocks` | Inspection | Show all blocks |
| `/remember` | Extraction | Save conversation to block |
| `/compact` | Optimization | Compress block/file |
| `/forget` | Optimization | Remove content from block/file |
| `/refresh` | Optimization | Rebuild from archival/git |
| `/history` | Inspection | Show sliding window |
| `/summary` | Inspection | Show auto-summary |
| `/recall` | Retrieval | Search archival memory |

All commands work seamlessly with both filesystem paths and `ikigai://` URIs.
