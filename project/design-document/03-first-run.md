# First-Run Experience

Ikigai optimizes for two personas with different needs.

---

## Solo Creator (Default Path)

The solo developer wants to build something and see it work. Minimal setup, no infrastructure decisions.

```
$ sudo apt install ikigai
$ ikigai

Welcome to Ikigai.

Setting up the platform...

  Installing Deno runtime... done
  Configuring PostgreSQL... done
  Starting ikigai-server... done

Ready. Your first agent is just a conversation away.

> create an agent that checks if my website is up every 5 minutes
```

**What happens:**
- Deno is sideloaded to `/opt/ikigai/lib/deno/` (not system-wide)
- PostgreSQL is configured for local use
- `ikigai-server` starts as a systemd service
- Config lives in `/etc/ikigai/` and is managed directly
- No backup configured (that's fine for experimentation)

**Time to first agent: ~2 minutes.**

---

## Production Path

For production deployments, the setup process walks through infrastructure decisions:

```
$ ikigai setup --production

Welcome to Ikigai production setup.

This will configure:
  • Configuration management via git repository
  • Backup for PostgreSQL data and secrets
  • Disaster recovery procedures

Estimated time: 15 minutes

Continue? [Y/n]
```

The production path configures:
- Git repository as source of truth for configuration
- WAL archiving to S3-compatible storage
- Encrypted secrets backup
- Webhook-based deployment pipeline

See [Backup and Recovery](14-backup.md) and [GitOps Configuration](15-gitops.md) for details.

---

## ikigai-server

A lightweight daemon that runs on every Ikigai host. It handles platform operations that can't be done through conversation alone.

### Responsibilities

| Function | Description |
|----------|-------------|
| Apply configuration | Reconcile actual state with desired state |
| Health monitoring | Check agent health, restart if needed |
| Webhook listener | Receive git push notifications (production mode) |
| Telemetry aggregation | Collect and store agent metrics |

### Modes

**Local mode (default):**
- Watches `/etc/ikigai/` for changes
- Applies changes when config files are modified
- No external dependencies

**GitOps mode (production):**
- Listens for webhooks from git repository
- Pulls latest config on notification
- Reconciles state and applies changes

```ini
# /etc/ikigai/ikigai.conf

[controller]
mode = local          # or "gitops"

# GitOps mode settings (ignored if mode = local)
git_repo = git@github.com:yourorg/ikigai-platform.git
webhook_secret = ${WEBHOOK_SECRET}
```

### systemd Integration

```ini
# /usr/lib/systemd/system/ikigai-server.service

[Unit]
Description=Ikigai Platform Controller
After=postgresql.service network.target

[Service]
Type=simple
ExecStart=/opt/ikigai/bin/ikigai-server
Restart=always

[Install]
WantedBy=multi-user.target
```

---

## Dependency Management

### Deno

Deno is not available in most Linux distribution repositories and moves faster than distro packaging cycles. Ikigai sideloads Deno to its own directory:

```
/opt/ikigai/
└── lib/
    └── deno/
        ├── 2.0.1/
        │   └── deno
        └── current -> 2.0.1/
```

**Key properties:**
- Installed to `/opt/ikigai/lib/deno/`, not system-wide
- Multiple versions can coexist
- Added to PATH only for ikigai operations
- Does not interfere with any system Deno installation
- Updated by ikigai, not system package manager

**Installation during setup:**
```
Installing Deno runtime...
  Downloading deno 2.0.1 for linux-x86_64
  Installing to /opt/ikigai/lib/deno/2.0.1/
  Setting current version... done
```
