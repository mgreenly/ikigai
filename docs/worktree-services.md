# Worktree-Aware Services

## Vision

Projects use bare git repos with worktree checkouts. Each worktree is a fully isolated development environment — its own database, services, and ports. A developer can work in multiple worktrees simultaneously without conflicts.

## The Core Problem

Multiple worktrees = multiple isolated environments that must coexist without conflict.

## Per-Component Analysis

### PostgreSQL

**Option A: Separate databases per worktree (same cluster)**
```
ikigai_main, ikigai_feature_auth, ikigai_bugfix_123
```
- Pros: Single postgres process, simple management
- Cons: Schema migrations could conflict, shared resources

**Option B: Separate clusters per worktree**
- Pros: Complete isolation
- Cons: Memory overhead (~10-20MB per cluster), port management

**Recommendation**: Option A with worktree-namespaced databases. Naming convention: `ikigai_<worktree_name>` or use a hash if worktree names get unwieldy.

### Nginx

Port-per-worktree seems inevitable. Options:

1. **Fixed offset scheme**: `8888 + hash(worktree) % 1000`
   - Risk: Collisions possible

2. **Explicit assignment**: Store port in `.agents/worktree.conf`
   - First worktree gets 8888, next gets 8889, etc.
   - Persist the mapping somewhere central

3. **Unix sockets + single nginx**: One nginx routes to all worktrees via path prefix
   - `localhost:8888/main/api/...`
   - `localhost:8888/feature-auth/api/...`
   - More complex routing but single port

### Runit Services

Separate `sv` directories per worktree. Each worktree already has its own `.agents/` so this "just works" — but requires separate `runsvdir` processes.

**Complication**: Process table clutter. 5 worktrees = 5 runsvdir processes plus all child services.

## Bigger Complications

### 1. Resource Exhaustion

5 worktrees × (nginx + postgres connections + N services) = lots of processes and ports.

**Mitigation**: Lazy startup. Services only run when you're actively working in a worktree. Ikigai could detect which worktree is "active" (current directory, recent file access) and only run those services.

### 2. Port Discovery

Developer switches to `feature-auth` worktree. What port is nginx on?

**Mitigation**:
- `ikigai status` shows current worktree's ports
- Write port to a well-known file: `.agents/ports.json`
- Or: environment variable set by direnv per worktree

### 3. Shared vs. Isolated State

Some things you might *want* shared:
- Deno cache (immutable, content-addressed)
- Maybe some reference data in postgres?

Some things must be isolated:
- Database schema/data
- Running services
- Logs

### 4. The Bare Repo Location

If the project root is a bare repo, where does `.agents/` live?

```
myproject/           # bare repo
├── .git/            # actual git data
├── worktrees/
│   ├── main/
│   │   └── .agents/ # worktree-specific
│   └── feature-auth/
│       └── .agents/
└── .agents-shared/  # shared resources?
```

### 5. Database Migrations

Worktree A has migration 42, worktree B has migration 43. If they share a cluster, you need namespaced schemas or separate databases.

**Recommendation**: Separate database per worktree. Migrations are worktree-local.

## Proposed Architecture

```
project.git/                    # bare repo
├── .agents-shared/
│   ├── db/                     # single postgres cluster
│   ├── ports.lock              # port allocation registry
│   └── deno/                   # shared deno install
└── worktrees/
    ├── main/
    │   ├── .agents/
    │   │   ├── db.conf         # DB=ikigai_main
    │   │   ├── nginx.conf      # port 8888
    │   │   ├── sv/             # this worktree's services
    │   │   └── logs/
    │   └── <project files>
    └── feature-auth/
        ├── .agents/
        │   ├── db.conf         # DB=ikigai_feature_auth
        │   ├── nginx.conf      # port 8889
        │   ├── sv/
        │   └── logs/
        └── <project files>
```

**Key insight**: One postgres cluster (shared), many databases (isolated). One port registry (shared), many nginx instances (isolated). One deno binary (shared), many service processes (isolated).

## Worktree Registry

A central registry in the bare repo tracks all worktrees and their allocated resources:

```
project.git/worktree-registry
# name:port
main:8888
feature-auth:8889
```

**Location**: Stored in the bare repo directory (not on any branch). This is purely local infrastructure state — if you re-clone the bare repo, you're rebuilding everything anyway.

**Why not a branch?** Committing infrastructure state pollutes history and creates merge conflicts when multiple developers add worktrees simultaneously.

### Lifecycle Commands

```bash
ikigai worktree add feature-auth
# 1. Allocate next port from worktree-registry
# 2. git worktree add worktrees/feature-auth
# 3. CREATE DATABASE ikigai_feature_auth
# 4. Initialize worktrees/feature-auth/.agents/ with allocated port

ikigai worktree remove feature-auth
# 1. Remove from worktree-registry
# 2. git worktree remove worktrees/feature-auth
# 3. DROP DATABASE ikigai_feature_auth
# 4. Release port 8889 for reuse
```

### Garbage Collection

The registry enables cleanup of orphaned resources:

```bash
ikigai worktree gc
# Compare registry against:
#   - Actual git worktrees (git worktree list)
#   - Databases in postgres (SELECT datname FROM pg_database WHERE datname LIKE 'ikigai_%')
#   - Allocated ports
# Report orphans, offer to clean up
```

Catches cases like:
- Manual `rm -rf` of a worktree (orphan database remains)
- Manual `git worktree remove` (registry out of sync)
- Database exists but not in registry (leftover from failed creation)

## Open Questions

1. **Naming**: How to derive the worktree identifier? Git worktree path? Explicit config?

2. **Lifecycle**: Start all worktree services on `ikigai start`, or only the active one?

3. **Single nginx vs. many**: Path-based routing (one nginx) or port-based (many)?
