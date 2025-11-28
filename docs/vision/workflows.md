# Workflows

Real-world examples of ikigai's multi-agent and mark/rewind features in action.

## Parallel Feature Development

**Scenario:** Implementing three features simultaneously without branch switching.

### The Workflow

```bash
# Start in root agent on main branch
[Agent: main] [Branch: main]

[You] I need to implement OAuth authentication, rate limiting, and audit logging.
Let's parallelize this work.

# Create OAuth agent
/agent new oauth-impl --worktree
[You] Implement OAuth 2.0 authentication with token validation and refresh logic.
[Assistant starts working]

# Create rate limiting agent
Ctrl-\ m
/agent new rate-limit --worktree
[You] Implement rate limiting middleware with Redis backend.
[Assistant starts working]

# Create audit agent
Ctrl-\ m
/agent new audit-log --worktree
[You] Implement audit logging for all API operations.
[Assistant starts working]

# Check progress
Ctrl-\ m
/agent list

Active Agents:
• main (you are here) - Branch: main
• oauth-impl - Branch: oauth-impl | Ahead: 3 commits | Modified: 6
• rate-limit - Branch: rate-limit | Ahead: 2 commits | Modified: 4
• audit-log - Branch: audit-log | Ahead: 2 commits | Modified: 3

# Check OAuth status
Ctrl-\ oauth
[You] What's the status?
[Assistant] OAuth implementation complete: ✓ Token validation ✓ Refresh logic ✓ Tests (15 passing)

# Merge OAuth
/agent merge && /agent close

# Check rate limiting
Ctrl-\ rate
[You] Status?
[Assistant] Complete. Need to rebase on main (OAuth was just merged).
[You] Rebase on main, then merge
[Assistant rebases]
/agent merge && /agent close

# Check audit logging
Ctrl-\ audit
[You] Status?
[Assistant] Complete! ✓ PostgreSQL integration ✓ Tests (10 passing)
/agent merge && /agent close

# Back to main - all features integrated
Ctrl-\ m
[You] Run full test suite
[Assistant] Total: 45 tests, all passing. Coverage: 100%
```

**Result:** Three features developed simultaneously, no branch switching overhead, clean integration to main, total time: ~2 hours (vs ~6 hours sequentially).

## Experimental Approaches with Mark/Rewind

**Scenario:** Trying multiple implementation approaches to find the best.

### The Workflow

```bash
[Agent: main] [Branch: main]

[You] We need a fast lookup structure for user permissions. Not sure whether to use
hash table or binary tree.

/mark before-data-structure

# Try hash table
[You] Let's try a hash table approach first
[Assistant] I'll implement with a hash table...
  Lookup: O(1) average
  Memory: ~1KB per user (high)
  For 100,000 users: ~100MB

/mark hash-table-approach

# Try BST
[You] That's a lot of memory. Let's try a binary search tree.
/rewind before-data-structure

[You] Try a binary search tree instead
[Assistant] I'll implement with a BST...
  Lookup: O(log n)
  Memory: ~200 bytes per user (low)
  For 100,000 users: ~20MB
  5x less memory than hash table!

/mark bst-approach

# Try hybrid
[You] Better memory usage, but slower lookups. Let's try a hybrid.
/rewind before-data-structure

[You] Let's try a hybrid: LRU cache + database
[Assistant] I'll implement a hybrid approach...
  LRU cache for hot data (1000 users)
  Database for cold data
  Lookup (cached): O(1)
  Lookup (uncached): ~1ms
  Memory: ~200KB fixed
  Cache hit rate: ~95% for typical workloads

/mark hybrid-approach

# Compare all three
[You] Create a memory doc comparing all three approaches
[Assistant] Created #mem-187 (permission-cache/comparison)

Summary:
1. Hash Table: Fastest O(1), High memory, Best for <10K users
2. BST: Moderate O(log n), Medium memory, Predictable constraints
3. Hybrid LRU+DB: Fast for hot data, Constant memory, Best for large user bases

Recommendation: Hybrid approach

# Implement the winner
/rewind hybrid-approach
[You] Implement this with full tests and documentation
[Assistant implements, all tests passing]
```

**Result:** Tried three approaches, compared objectively, documented findings in memory doc, chose best approach, no wasted context from dead ends.

## Research and Implementation Pipeline

**Scenario:** Research in one agent, implement in another.

### The Workflow

```bash
[Agent: main] [Branch: main]

[You] I need to implement WebSocket support, but need research first.

# Research agent
/agent new websocket-research
[You] Research WebSocket implementation in C:
- Best libraries (libwebsockets vs alternatives)
- Security considerations
- Performance patterns
- Testing strategies
Create comprehensive memory doc with findings.

[Assistant researches thoroughly]

Created #mem-188 (websocket/research)
  Libraries: libwebsockets (recommended)
  Security: WSS, origin validation, rate limiting
  Performance: epoll/kqueue, buffer pooling
  Testing: wscat, Autobahn test suite

Created #mem-189 (websocket/implementation-plan)
  Phase 1: Basic server
  Phase 2: Message handling
  Phase 3: Security
  Phase 4: Testing

/agent close

# Implementation agent
[Agent: main]
/agent new websocket-impl --worktree
[You] Implement WebSocket support using #mem-188 and #mem-189

[Assistant loads memory docs]
Based on research, I'll implement using libwebsockets...

Phase 1: ✓ Basic server working
Phase 2: ✓ Message handling complete
Phase 3: ✓ Security measures in place
Phase 4: ✓ All tests passing (28/28), Autobahn compliance: 100%

/agent merge && /agent close

# Documentation agent
Ctrl-\ m
/agent new websocket-docs
[You] Create documentation using #mem-188 and #mem-189

[Assistant creates comprehensive docs]
Created #mem-190 (websocket/documentation)

/agent close
```

**Result:** Research in dedicated agent, implementation in separate worktree, documentation in third agent, memory docs tie everything together, clean separation of concerns.

## Bug Fix on Multiple Branches

**Scenario:** Critical bug found, need to fix across multiple branches.

### The Workflow

```bash
# Working on feature
[Agent: main] [Branch: feature-api]
[You] Continue implementing the API endpoints

# Bug report comes in
[You] URGENT: Memory leak in authentication module.

/mark before-bugfix

# Create bug fix agent
/agent new bugfix-auth --worktree --branch=bugfix/auth-leak

[You] Fix memory leak in src/auth.c: Token context not freed on error path

[Assistant analyzes and fixes]
Found the leak in ik_auth_validate_token():
  BUG: ctx not freed on error path
  FIX: Add talloc_free(ctx) before returning error

[You] Add regression test
[Assistant adds test]

/commit "Fix memory leak in auth token validation error path"

# Merge to main
/agent merge main

# Cherry-pick to release branch
[You] Cherry-pick this fix to release-1.0 branch
[Assistant] Cherry-picking... ✓ Clean cherry-pick ✓ All tests pass

/agent close

# Back to feature branch
Ctrl-\ m
[You] Merge the bugfix into this branch too
[Assistant] Merging main into feature-api... ✓ Merged

# Continue feature work
/rewind before-bugfix
[Assistant] Continuing API endpoint implementation...
```

**Result:** Bug fixed without disrupting feature work, applied to multiple branches, regression test added, clean context restoration.

## Exploration with Nested Marks

**Scenario:** Exploring nested design decisions.

### The Workflow

```bash
[Agent: main] [Branch: main]

[You] We need to redesign the caching layer

/mark cache-design-start

[You] First decision: In-memory vs Redis?

/mark decision-storage

# Try in-memory
[You] Try in-memory approach
[Assistant discusses in-memory cache with persistence]

/mark in-memory-with-persistence

[You] That's getting complex. Rewind.
/rewind decision-storage

# Try Redis
[You] Try Redis approach
[Assistant discusses Redis with HA setup]

/mark redis-with-ha

[You] Seems over-engineered. What about hybrid?
/rewind decision-storage

# Try hybrid
[You] Hybrid: in-memory L1 cache + Redis L2 cache
[Assistant] Hybrid cache design:
  - L1: In-memory (hot data, 1000 items)
  - L2: Redis (warm data, 100K items)
  - Best of both worlds
  Lookup flow: L1 (~100ns) → L2 (~1ms) → DB (~10ms)

/mark eviction-policy

# Design eviction
[You] Try LRU eviction
[Assistant] LRU eviction: Track access time, evict least recently used

[You] Actually, LRU is fine. Commit to this design.

# Create final design doc
[You] Create memory doc with final hybrid cache design (L1/L2) with LRU eviction
[Assistant] Created #mem-195 (cache/design)

# Document exploration
[You] Also create memory doc documenting approaches tried and why we chose this
[Assistant] Created #mem-196 (cache/exploration)
  Approaches: Pure in-memory, Pure Redis, Hybrid L1+L2 ✓
  Eviction: LRU ✓
  Decision: Hybrid with LRU for best performance/complexity tradeoff

# Implement
/agent new cache-impl --worktree
[You] Implement the cache design from #mem-195
[Assistant implements cleanly without confusion from exploration]
```

**Result:** Explored multiple approaches systematically, nested marks for sub-decisions, documented exploration path, clean implementation from final design.

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent concepts
- [mark-rewind.md](mark-rewind.md) - Checkpoint system
- [git-integration.md](git-integration.md) - Git workflows
- [commands.md](commands.md) - Command reference
