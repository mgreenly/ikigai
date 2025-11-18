# Workflows

Real-world examples of ikigai's multi-agent and mark/rewind features in action.

## Parallel Feature Development

**Scenario:** Implementing three features simultaneously without branch switching.

### The Workflow

```bash
# Start in root agent on main branch
[Agent: main] [Branch: main]

[You]
I need to implement three features:
1. OAuth authentication
2. Rate limiting
3. Audit logging

Let's parallelize this work.

# Create OAuth agent
/agent new oauth-impl --worktree

[Agent: oauth-impl] [Branch: oauth-impl]

[You]
Implement OAuth 2.0 authentication with token validation and refresh logic.
Follow security best practices and write comprehensive tests.

[Assistant]
I'll implement OAuth authentication...
[Starts working]

# Switch back to main, create rate limiting agent
Ctrl-\ m

[Agent: main] [Branch: main]

/agent new rate-limit --worktree

[Agent: rate-limit] [Branch: rate-limit]

[You]
Implement rate limiting middleware with Redis backend.
Support per-user and per-IP limits.

[Assistant]
I'll implement rate limiting...
[Starts working]

# Switch back to main, create audit agent
Ctrl-\ m

/agent new audit-log --worktree

[Agent: audit-log] [Branch: audit-log]

[You]
Implement audit logging for all API operations.
Log to PostgreSQL with retention policy.

[Assistant]
I'll implement audit logging...
[Starts working]

# Check progress on all agents
Ctrl-\ m

[Agent: main] [Branch: main]

/agent list

Active Agents:
• main (you are here) - Branch: main
  Worktree: ~/projects/ikigai/main

• oauth-impl - Branch: oauth-impl
  Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl
  Messages: 23 | Ahead: 3 commits
  Files modified: 6

• rate-limit - Branch: rate-limit
  Worktree: ~/projects/ikigai/main/.worktrees/rate-limit
  Messages: 18 | Ahead: 2 commits
  Files modified: 4

• audit-log - Branch: audit-log
  Worktree: ~/projects/ikigai/main/.worktrees/audit-log
  Messages: 15 | Ahead: 2 commits
  Files modified: 3

# Check on OAuth implementation
Ctrl-\ oauth

[Agent: oauth-impl] [Branch: oauth-impl]

[You]
What's the status?

[Assistant]
OAuth implementation complete:
✓ Token validation
✓ Refresh logic
✓ Security headers
✓ Tests (15 passing)
✓ Coverage: 100%

Ready to merge!

# Merge OAuth
/agent merge

Merging oauth-impl → main...
  3 commits merged
  All tests pass
  Coverage: 100%
Done!

# Check on rate limiting
Ctrl-\ rate

[Agent: rate-limit] [Branch: rate-limit]

[You]
Status?

[Assistant]
Rate limiting implementation complete:
✓ Redis integration
✓ Per-user limits
✓ Per-IP limits
✓ Tests (12 passing)

Need to rebase on main (OAuth was just merged).

[You]
Rebase on main, then we'll merge

[Assistant]
Rebasing...
  2 commits rebased
  All tests still pass
Done!

/agent merge
/agent close

# Check on audit logging
Ctrl-\ audit

[Agent: audit-log] [Branch: audit-log]

[You]
Status?

[Assistant]
Audit logging complete:
✓ PostgreSQL integration
✓ All operations logged
✓ Retention policy
✓ Tests (10 passing)

Ready to merge!

/agent merge
/agent close

# All features integrated
Ctrl-\ m

[Agent: main] [Branch: main]

[You]
Run full test suite

[Assistant]
Running all tests...
  OAuth: 15 passing
  Rate limiting: 12 passing
  Audit logging: 10 passing
  Integration: 8 passing

Total: 45 tests, all passing
Coverage: 100%

All features successfully integrated!
```

**Result:**
- Three features developed simultaneously
- No branch switching overhead
- Each feature in isolation until ready
- Clean integration to main
- Total time: ~2 hours (vs ~6 hours sequentially)

## Experimental Approaches with Mark/Rewind

**Scenario:** Trying multiple implementation approaches to find the best.

### The Workflow

```bash
[Agent: main] [Branch: main]

[You]
We need to implement a fast lookup structure for user permissions.
I'm not sure whether to use a hash table or binary tree.

/mark before-data-structure

⟡ Mark: before-data-structure

[You]
Let's try a hash table approach first

[Assistant]
I'll implement with a hash table...

typedef struct {
    ik_hash_table_t *permissions;  // O(1) lookup
    void *ctx;
} ik_permission_cache_t;

Implementation:
- Hash table with user_id as key
- Permission set as value
- Fast lookups: O(1) average case

[You]
What's the memory usage and performance?

[Assistant]
Performance analysis:
  Lookup: O(1) average, O(n) worst case
  Insert: O(1) average
  Delete: O(1) average
  Memory: ~1KB per user (high)

  For 10,000 users: ~10MB
  For 100,000 users: ~100MB

/mark hash-table-approach

⟡ Mark: hash-table-approach

[You]
That's a lot of memory. Let's try a binary search tree.

/rewind before-data-structure

Rewound to Mark: before-data-structure ⟡

[You]
Let's try a binary search tree instead

[Assistant]
I'll implement with a BST...

typedef struct ik_permission_node_t {
    uint64_t user_id;
    ik_permission_set_t *permissions;
    struct ik_permission_node_t *left;
    struct ik_permission_node_t *right;
} ik_permission_node_t;

Implementation:
- Balanced BST (AVL or Red-Black)
- Sorted by user_id
- Lower memory overhead

[You]
What's the memory usage and performance?

[Assistant]
Performance analysis:
  Lookup: O(log n)
  Insert: O(log n)
  Delete: O(log n)
  Memory: ~200 bytes per user (low)

  For 10,000 users: ~2MB
  For 100,000 users: ~20MB

5x less memory than hash table!

/mark bst-approach

⟡ Mark: bst-approach

[You]
Better memory usage, but slower lookups. Let's try a hybrid.

/rewind before-data-structure

Rewound to Mark: before-data-structure ⟡

[You]
Let's try a hybrid: LRU cache + database

[Assistant]
I'll implement a hybrid approach...

typedef struct {
    ik_lru_cache_t *hot_cache;     // 1000 most recent users
    ik_db_connection_t *db;         // PostgreSQL for cold storage
} ik_permission_cache_t;

Implementation:
- LRU cache for hot data (1000 users)
- Database for cold data
- Best of both worlds

[You]
Performance and memory?

[Assistant]
Performance analysis:
  Lookup (cached): O(1)
  Lookup (uncached): Database query (~1ms)
  Memory: ~200KB fixed (for 1000 user cache)

  For 10,000 users: 200KB + database
  For 100,000 users: 200KB + database

Best memory efficiency!
Cache hit rate: ~95% for typical workloads

/mark hybrid-approach

⟡ Mark: hybrid-approach

[You]
Let's compare all three approaches

# Create comparison memory doc
[You]
Create a memory doc comparing all three approaches with benchmarks

[Assistant]
Created #mem-187 (permission-cache/comparison)

Summary:
1. Hash Table:
   - Fastest: O(1) lookups
   - Memory: High (~1KB/user)
   - Best for: <10K users with memory available

2. BST:
   - Moderate: O(log n) lookups
   - Memory: Medium (~200B/user)
   - Best for: Predictable memory constraints

3. Hybrid LRU+DB:
   - Fast for hot data: O(1) cached, O(db) uncached
   - Memory: Constant (200KB)
   - Best for: Large user bases, typical access patterns

Recommendation: Hybrid approach
  - Scales to millions of users
  - Constant memory usage
  - 95% cache hit rate = near O(1) performance

[You]
Implement the hybrid approach

/rewind hybrid-approach

Rewound to Mark: hybrid-approach ⟡

[You]
Implement this with full tests and documentation

[Assistant]
Implementing hybrid permission cache...
[Full implementation]
All tests passing!
```

**Result:**
- Tried three approaches
- Compared objectively
- Documented findings in memory doc
- Chose best approach
- No wasted context from dead ends

## Research and Implementation Pipeline

**Scenario:** Research in one agent, implement in another.

### The Workflow

```bash
[Agent: main] [Branch: main]

[You]
I need to implement WebSocket support, but I need research first.

/agent new websocket-research

[Agent: websocket-research] [No worktree]

[You]
Research WebSocket implementation in C:
1. Best libraries (libwebsockets vs alternatives)
2. Security considerations
3. Performance patterns
4. Testing strategies

Create comprehensive memory doc with findings.

[Assistant]
I'll research WebSocket implementation...

[Researches libraries, reads docs, analyzes options]

Research complete! Key findings:

Libraries:
1. libwebsockets - Most popular, actively maintained
2. libuwsc - Lightweight, simple API
3. nopoll - Older, less active

Recommendation: libwebsockets
- Active development
- Excellent documentation
- Built-in SSL/TLS
- Proven in production

Security:
- Always use WSS (WebSocket Secure)
- Validate Origin header
- Implement message size limits
- Rate limiting critical

Performance:
- Use epoll/kqueue for event loop
- Buffer pooling for messages
- Compression for large messages

Testing:
- Use wscat for manual testing
- Autobahn test suite for compliance
- Load testing with Thor/Artillery

Created #mem-188 (websocket/research)

[You]
Perfect! Now create implementation plan.

[Assistant]
Implementation plan:

Phase 1: Basic WebSocket server
- Integrate libwebsockets
- Basic echo server
- SSL/TLS support

Phase 2: Message handling
- JSON message parsing
- Message routing
- Error handling

Phase 3: Security
- Origin validation
- Message size limits
- Rate limiting

Phase 4: Testing
- Unit tests
- Integration tests
- Load tests with Autobahn

Created #mem-189 (websocket/implementation-plan)

/agent close

Agent closed. Memory docs preserved.

# Now implement
[Agent: main] [Branch: main]

/agent new websocket-impl --worktree

[Agent: websocket-impl] [Branch: websocket-impl]

[You]
Implement WebSocket support using #mem-188 and #mem-189

[Assistant]
[Loads memory docs]

Based on the research, I'll implement using libwebsockets...

Phase 1: Basic server
[Implements basic WebSocket server]
[Tests with wscat]
✓ Echo server working

Phase 2: Message handling
[Implements JSON message parsing and routing]
✓ Message handling complete

Phase 3: Security
[Implements origin validation, size limits, rate limiting]
✓ Security measures in place

Phase 4: Testing
[Writes comprehensive tests]
✓ All tests passing
✓ Autobahn compliance: 100%

Implementation complete!
All tests pass: 28/28
Coverage: 100%

/agent merge
/agent close

# Documentation
Ctrl-\ m

[Agent: main] [Branch: main]

/agent new websocket-docs

[Agent: websocket-docs] [No worktree]

[You]
Create documentation for WebSocket implementation using #mem-188 and #mem-189

[Assistant]
Creating comprehensive documentation...
[References research and implementation]

Created:
- API documentation
- Usage examples
- Security guidelines
- Testing guide

Created #mem-190 (websocket/documentation)

/agent close
```

**Result:**
- Research in dedicated agent
- Implementation in separate worktree
- Documentation in third agent
- Memory docs tie everything together
- Clean separation of concerns

## Bug Fix on Multiple Branches

**Scenario:** Critical bug found, need to fix across multiple branches.

### The Workflow

```bash
# Working on feature
[Agent: main] [Branch: feature-api]

[You]
Continue implementing the API endpoints

[Assistant]
Working on API implementation...

# Bug report comes in
[You]
URGENT: Memory leak found in authentication module.
Affects main, release-1.0, and this feature branch.

# Don't disrupt current work
/mark before-bugfix

⟡ Mark: before-bugfix

# Create bug fix agent on main
/agent new bugfix-auth --worktree --branch=bugfix/auth-leak

[Agent: bugfix-auth] [Branch: bugfix/auth-leak]
[Working directory: .worktrees/bugfix-auth]

[You]
Fix memory leak in src/auth.c:
- Leak occurs in ik_auth_validate_token()
- Token context not freed on error path

[Assistant]
Analyzing src/auth.c...

Found the leak:

res_t ik_auth_validate_token(const char *token) {
    ik_token_ctx_t *ctx = parse_token(token);
    if (!ctx)
        return ERR(ERR_INVALID_TOKEN, "Invalid token");

    if (!validate_signature(ctx)) {
        // BUG: ctx not freed here!
        return ERR(ERR_INVALID_SIGNATURE, "Invalid signature");
    }

    talloc_free(ctx);
    return OK(NULL);
}

Fix:

res_t ik_auth_validate_token(const char *token) {
    ik_token_ctx_t *ctx = parse_token(token);
    if (!ctx)
        return ERR(ERR_INVALID_TOKEN, "Invalid token");

    if (!validate_signature(ctx)) {
        talloc_free(ctx);  // FIX: Free before returning
        return ERR(ERR_INVALID_SIGNATURE, "Invalid signature");
    }

    talloc_free(ctx);
    return OK(NULL);
}

[You]
Add regression test

[Assistant]
Adding regression test...

void test_auth_validate_leak(void) {
    // Test error path doesn't leak
    size_t before = talloc_total_size(NULL);

    res_t result = ik_auth_validate_token("invalid-signature-token");
    ck_assert(IS_ERR(result));

    size_t after = talloc_total_size(NULL);
    ck_assert_int_eq(before, after);  // No leak
}

All tests pass!
Coverage: 100%

[You]
Commit the fix

/commit "Fix memory leak in auth token validation error path"

[bugfix/auth-leak abc1234] Fix memory leak
 2 files changed, 5 insertions(+), 1 deletion(-)

# Merge to main
/agent merge main

Merged to main ✓

# Cherry-pick to release branch
[You]
Cherry-pick this fix to release-1.0 branch

[Assistant]
Cherry-picking abc1234 to release-1.0...
✓ Clean cherry-pick
✓ All tests pass

# Close bugfix agent
/agent close

# Back to feature branch
Ctrl-\ m

[Agent: main] [Branch: feature-api]

[You]
Merge the bugfix into this branch too

[Assistant]
Merging main into feature-api...
  Includes bugfix/auth-leak
  No conflicts
✓ Merged

[You]
Now continue with the API implementation

/rewind before-bugfix

Rewound to Mark: before-bugfix ⟡

# Context restored to pre-bugfix state
# But code includes the fix (from merge)
# Can continue feature work cleanly

[Assistant]
Continuing API endpoint implementation...
```

**Result:**
- Bug fixed without disrupting feature work
- Applied to multiple branches
- Regression test added
- Clean context restoration
- No mental overhead

## Exploration with Nested Marks

**Scenario:** Exploring nested design decisions.

### The Workflow

```bash
[Agent: main] [Branch: main]

[You]
We need to redesign the caching layer

/mark cache-design-start

⟡ Mark: cache-design-start

[You]
First decision: In-memory vs Redis?

/mark decision-storage

⟡ Mark: decision-storage

[You]
Try in-memory approach

[Assistant]
In-memory cache design:
- Hash table in application memory
- Fast: no network overhead
- Limited by RAM
- Lost on restart

[You]
What about persistence?

[Assistant]
Could add write-through to disk...
[Discusses persistence]

/mark in-memory-with-persistence

⟡ Mark: in-memory-with-persistence

[You]
That's getting complex. Rewind.

/rewind decision-storage

Rewound to Mark: decision-storage ⟡

[You]
Try Redis approach

[Assistant]
Redis cache design:
- Centralized cache server
- Persistent
- Slower: network overhead
- Shared across instances

[You]
What about failover?

[Assistant]
Redis Sentinel for high availability...
[Discusses HA setup]

/mark redis-with-ha

⟡ Mark: redis-with-ha

[You]
Seems over-engineered for our scale. What about hybrid?

/rewind decision-storage

Rewound to Mark: decision-storage ⟡

[You]
Hybrid: in-memory L1 cache + Redis L2 cache

[Assistant]
Hybrid cache design:
- L1: In-memory (hot data, 1000 items)
- L2: Redis (warm data, 100K items)
- Best of both worlds

Lookup flow:
1. Check L1 (in-memory) - O(1), ~100ns
2. If miss, check L2 (Redis) - O(1), ~1ms
3. If miss, load from DB - ~10ms
4. Populate L1 and L2

[You]
Perfect! Now let's design the eviction policy.

/mark eviction-policy

⟡ Mark: eviction-policy

[You]
Try LRU eviction

[Assistant]
LRU eviction design:
- Track access time
- Evict least recently used
- Simple, predictable

[You]
What about LFU?

[Assistant]
LFU tracks access frequency...
[Compares LRU vs LFU]

[You]
Actually, LRU is fine. Commit to this design.

# Create final design doc
[You]
Create memory doc with final hybrid cache design (L1/L2) with LRU eviction

[Assistant]
Created #mem-195 (cache/design)

Design Summary:
- L1: In-memory LRU cache (1000 items)
- L2: Redis LRU cache (100K items)
- Eviction: LRU for both levels
- Performance: 95% L1 hit rate = ~100ns avg

# Document the exploration
[You]
Also create memory doc documenting approaches we tried and why we chose this

[Assistant]
Created #mem-196 (cache/exploration)

Approaches Evaluated:
1. Pure in-memory: Fast but limited scale
2. Pure Redis: Persistent but network overhead
3. Hybrid L1+L2: Best balance ✓

Eviction Policies Considered:
- LRU: Simple, predictable ✓
- LFU: More complex, marginal gains

Decision: Hybrid with LRU
Rationale: Best performance/complexity tradeoff

# Now implement
/agent new cache-impl --worktree

[Agent: cache-impl] [Branch: cache-impl]

[You]
Implement the cache design from #mem-195

[Assistant]
Implementing hybrid L1/L2 cache with LRU eviction...
[Clean implementation without confusion from exploration]
```

**Result:**
- Explored multiple approaches systematically
- Nested marks for sub-decisions
- Documented exploration path
- Clean implementation from final design
- No confusion from dead ends

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent concepts
- [mark-rewind.md](mark-rewind.md) - Checkpoint system
- [git-integration.md](git-integration.md) - Git workflows
- [commands.md](commands.md) - Command reference
