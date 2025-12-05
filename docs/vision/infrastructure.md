# Infrastructure

## Overview

Ikigai provides a unified runtime for building and deploying TypeScript applications. The same infrastructure runs in development and production: nginx, PostgreSQL, and runit. In development, these run as child processes. In production, they're system services. Same configs, same structure, no "works on my machine" problems.

---

## Design Philosophy

### Same Stack Everywhere

Development and production use identical infrastructure:
- **PostgreSQL**: The coordination layer, message queues, history, state
- **nginx**: HTTP reverse proxy, static file serving
- **runit**: Process supervision for agents

The difference is only in how they're started (child processes vs system services) and where data lives (local `.agents/` directory vs `/opt/ikigai/`).

### Invisible Plumbing

The goal is that developers create services following a simple convention, and Ikigai handles all infrastructure automatically:
- Proxy configuration
- Database provisioning
- Service supervision
- Log aggregation

The plumbing becomes invisible.

---

## Current State (Prototype)

The infrastructure exists but requires manual control via shell scripts. This proves out the architecture before Ikigai absorbs service management.

### Directory Structure

```
.agents/
├── bin/           # Wrapper scripts (added to PATH via direnv)
│   ├── deno       # Auto-installs deno on first use
│   ├── nginx      # Wrapper for local nginx
│   ├── psql       # Connects to local postgres
│   ├── pg_ctl     # Manages local postgres
│   └── runsvdir   # Service supervisor control
├── db/            # PostgreSQL data directory
├── deno/          # Deno installation (auto-provisioned)
├── httpd/         # Nginx config and web root
│   ├── nginx.conf
│   ├── pid/
│   └── root/      # Static files served at localhost:8888
│       └── frontend/
├── logs/          # Service logs
│   ├── archive/   # Rotated logs
│   └── backend/   # Backend service logs (via svlogd)
└── sv/            # Runit service directory
    └── backend/   # Example deno backend service
        ├── run
        ├── main.ts
        └── log/
            └── run
```

### Manual Service Control

#### nginx (port 8888)
```bash
nginx           # Run foreground (blocking)
nginx start     # Run as daemon
nginx stop      # Stop daemon
nginx -s reload # Reload config
```

#### PostgreSQL (port 15432)
```bash
.agents/scripts/postgres  # Run foreground (blocking, auto-inits on first run)
pg_ctl start              # Start as daemon
pg_ctl stop               # Stop daemon
psql                      # Connect (defaults to ikigai/ikigai/ikigai)
```

#### runit Service Supervisor
```bash
runsvdir           # Run foreground (blocking)
runsvdir start     # Run as daemon
runsvdir stop      # Stop daemon
runsvdir status    # Check if running
```

#### Managing Individual Services

With `runsvdir` running:
```bash
sv status .agents/sv/backend   # Check status
sv start .agents/sv/backend    # Start
sv stop .agents/sv/backend     # Stop
sv restart .agents/sv/backend  # Restart
```

Or set `SVDIR` for shorter commands:
```bash
export SVDIR=$PWD/.agents/sv
sv status backend
sv restart backend
```

### Viewing Logs
```bash
tail -f .agents/logs/backend/current   # Watch backend logs
tail -f .agents/logs/nginx.log         # Watch nginx logs
tail -f .agents/logs/postgres.log      # Watch postgres logs
```

---

## Future State

### Automatic Service Management

```bash
ikigai start   # Everything comes up automatically
ikigai stop    # Clean shutdown
```

Or even simpler: Ikigai detects a project's services and manages them automatically.

### Named Services

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

The manual scripts become internal implementation details, invisible to the user.

---

## Platform Services

Agents consume platform services through the `@ikigai/platform` library:

| Service | Purpose |
|---------|---------|
| **Queues** | Claim tasks, complete work |
| **Mailboxes** | Point-to-point messaging |
| **Pub/Sub** | Broadcast to subscribers |
| **Storage** | Persistent data |
| **Cache** | Fast access to hot data |
| **Telemetry** | Logs, metrics, events |
| **LLM Access** | Prompts, conversations, tool execution |

Agents don't care whether their queue is PostgreSQL or Redis. They use the platform API; the infrastructure is configuration.

---

## Why PostgreSQL for Everything

PostgreSQL handles queues, messaging, storage, caching, and telemetry out of the box. One dependency, zero configuration.

Benefits:
- **LISTEN/NOTIFY**: Real-time pub/sub
- **FOR UPDATE SKIP LOCKED**: Work queue semantics
- **JSONB**: Flexible document storage
- **Full-text search**: Built-in search capabilities
- **Transactions**: Atomic operations across multiple concerns

You can swap individual services to specialized backends when specific needs arise. But the defaults work.

---

## Deployment

### Git-Based, No Artifacts

Deployment is a shallow git clone to a specific tag. No build step, no artifact pipeline. Deno runs TypeScript directly.

```
/opt/ikigai/agents.d/
└── monitoring-agent/
    ├── v1.0.0/              # git clone --depth 1 --branch v1.0.0
    ├── v1.1.0/              # git clone --depth 1 --branch v1.1.0
    ├── v1.2.0/              # git clone --depth 1 --branch v1.2.0
    └── current -> v1.2.0/   # symlink to active version
```

Every tag is deployable. The `deno.lock` file ensures reproducible dependencies.

### Deployment Process

When you deploy an agent:

1. **First deploy** (if needed): Create Linux user, PostgreSQL role, directories
2. **Clone version**: `git clone --depth 1 --branch v1.2.0`
3. **Update symlink**: `ln -sfn v1.2.0 current`
4. **Restart service**: Graceful shutdown of old, start new
5. **Health check**: Verify agent responds

### Rollback

Rollback is instant. Just change the symlink:

```bash
ln -sfn v1.1.0 current
sv restart monitoring-agent
```

All previously deployed versions remain on disk. No artifact retrieval needed.

### Why Not Artifacts?

Traditional CI/CD builds tarballs because compiled languages need a build step. Deno doesn't:
- TypeScript runs directly (no compilation)
- `deno.lock` pins exact dependency versions
- Git tags are immutable and verifiable
- Shallow clones are fast and small

Git is the artifact store.

---

## Security Model

### Linux-Native Identity

Rather than building custom systems, Ikigai uses Linux:

| Concern | Linux Solution |
|---------|----------------|
| Identity | PAM/passwd |
| Authorization | File permissions, sudo |
| Secrets | Files with restricted ownership |
| Process management | runit (dev), systemd (prod) |
| Logs | svlogd, journald |

This eliminates entire categories of complexity. Organizations with existing Linux infrastructure get integration for free.

### Identity Model

| Entity | Linux User | PostgreSQL Role |
|--------|------------|-----------------|
| Developer | `alice` (human user) | `ikigai_dev_alice` |
| Agent | `ikigai-monitoring-agent` (system user) | `ikigai_agent_monitoring` |

**Agent users** are system users created automatically on first deploy:

```bash
useradd --system --no-create-home --shell /usr/sbin/nologin ikigai-monitoring-agent
```

Agents cannot log in interactively. They exist solely to own files and run as services.

### Secrets Management

Secrets are files with restricted permissions:

```
/etc/ikigai/secrets/agents/monitoring-agent/
├── database_url     # mode 600, owner ikigai-monitoring-agent
└── api_key          # mode 600, owner ikigai-monitoring-agent
```

Only the owning agent user can read its secrets. No secrets management service required.

### PostgreSQL Access Control

Each agent has a dedicated PostgreSQL role with minimal required permissions. Row-level security ensures agents can only access their own queues and mailboxes:

```sql
ALTER TABLE task_queue ENABLE ROW LEVEL SECURITY;
CREATE POLICY monitoring_queue_access ON task_queue
    FOR ALL TO ikigai_agent_monitoring
    USING (queue_name = 'monitoring-tasks');
```

### Enterprise Integration

For organizations with existing identity infrastructure:
- **LDAP/Active Directory**: Configure PAM with `sssd` or `pam_ldap`
- **SSO**: PAM modules for SAML, OIDC
- **Centralized sudo**: Manage developer permissions via LDAP groups

The platform inherits whatever identity infrastructure the organization already uses.

---

## Vertical First

Start with one server. A modern Linux machine with fast storage and enough RAM handles hundreds of concurrent agents.

On a single box:
- No network partitions
- No distributed consensus
- No eventual consistency
- Just processes talking through local sockets and a shared database

One person can maintain it. That's the point.

When you outgrow a single server, the architecture supports scaling out. But preserve the simplicity of a single server as long as you can. It makes everything easier to operate, debug, and reason about.
