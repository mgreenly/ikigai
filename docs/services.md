# Ikigai Services

> This document describes the infrastructure layer also known as [iki-genba](iki-genba.md), the PaaS for running autonomous agents.

## Vision

Ikigai provides a unified runtime for building and deploying TypeScript applications. The same infrastructure runs in development and production: nginx, PostgreSQL, and runit. In development, these run as child processes. In production, they're system services. Same configs, same structure, no "works on my machine" problems.

**User Experience Goal:**
Developers create services following a simple convention. Ikigai handles all infrastructure automatically - proxy configuration, database provisioning, service supervision, log aggregation. The plumbing becomes invisible.

**Future State:**
```bash
ikigai start   # Everything comes up automatically
ikigai stop    # Clean shutdown
```

Or even simpler - Ikigai detects a project's services and manages them automatically.

---

## Current State (Prototype)

The infrastructure exists but requires manual control via shell scripts. This proves out the architecture before Ikigai absorbs service management.

### Directory Structure

The `.ikigai/` directory lives at the project root, shared across all worktrees:

```
myproject/
├── .git/
├── .ikigai/                    # Shared infrastructure
│   ├── daemon.sock             # Unix socket for agent communication
│   ├── bin/                    # Wrapper scripts (added to PATH via direnv)
│   │   ├── deno                # Auto-installs deno on first use
│   │   ├── nginx               # Wrapper for local nginx
│   │   ├── psql                # Connects to local postgres
│   │   ├── pg_ctl              # Manages local postgres
│   │   └── runsvdir            # Service supervisor control
│   ├── db/                     # PostgreSQL data directory (worktree-aware schema)
│   ├── deno/                   # Deno installation (auto-provisioned)
│   ├── httpd/                  # Nginx config and web root
│   │   ├── nginx.conf
│   │   ├── pid/
│   │   └── root/
│   ├── logs/                   # Service logs
│   └── sv/                     # Runit service directory
│       └── backend/
├── main/                       # Worktree
├── rel-05/                     # Worktree
└── rel-06/                     # Worktree
```

Agents discover the daemon by walking up the directory tree to find `.ikigai/daemon.sock`, similar to how git finds `.git/`.

### Manual Service Control

#### Nginx (port 8888)
```bash
nginx           # Run foreground (blocking)
nginx start     # Run as daemon
nginx stop      # Stop daemon
nginx -s reload # Reload config
```

#### PostgreSQL (port 15432)
```bash
.ikigai/scripts/postgres  # Run foreground (blocking, auto-inits on first run)
pg_ctl start              # Start as daemon
pg_ctl stop               # Stop daemon
psql                      # Connect (defaults to ikigai/ikigai/ikigai)
```

#### Runit Service Supervisor
```bash
runsvdir           # Run foreground (blocking)
runsvdir start     # Run as daemon
runsvdir stop      # Stop daemon
runsvdir status    # Check if running
```

#### Managing Individual Services

With `runsvdir` running:
```bash
sv status .ikigai/sv/backend   # Check status
sv start .ikigai/sv/backend    # Start
sv stop .ikigai/sv/backend     # Stop
sv restart .ikigai/sv/backend  # Restart
```

Or set `SVDIR` for shorter commands:
```bash
export SVDIR=$PWD/.ikigai/sv
sv status backend
sv restart backend
```

### Viewing Logs
```bash
tail -f .ikigai/logs/backend/current   # Watch backend logs
tail -f .ikigai/logs/nginx.log         # Watch nginx logs
tail -f .ikigai/logs/postgres.log      # Watch postgres logs
```

### Quick Start (Manual)

```bash
# 1. Enable direnv
direnv allow

# 2. Start services
nginx start
pg_ctl start
runsvdir start

# 3. Verify
curl http://localhost:8888/frontend/
curl http://localhost:8888/api/hello

# 4. Stop services (when done)
runsvdir stop
nginx stop
pg_ctl stop
```

---

## Future: Named Services

Projects will define services in a conventional structure:

```
myproject/
├── serviceA/
│   ├── frontend/    # Static files
│   └── backend/     # Deno TypeScript
├── serviceB/
│   ├── frontend/
│   └── backend/
```

Ikigai will:
- Detect services automatically
- Symlink into nginx/runit directories
- Generate proxy configurations
- Provision databases as needed
- Aggregate logs
- Start/stop everything as a unit

The manual scripts we have today become internal implementation details, invisible to the user.
