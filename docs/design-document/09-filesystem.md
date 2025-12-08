# Filesystem Layout

> **Part V: Cross-Cutting Concerns**: These sections describe infrastructure that spans all four pillars: where files live, how identity works, how deployment happens, and how you observe the system.

---

The platform follows the Filesystem Hierarchy Standard (FHS):

```
/opt/ikigai/                         # Application (static, deployable)
├── bin/
│   └── ikigai                       # Terminal application
├── lib/
│   └── deno/                        # Sideloaded Deno runtime
│       ├── 2.0.1/
│       └── current -> 2.0.1/
├── agents.d/                        # Deployed agent code
│   └── monitoring-agent/
│       ├── v1.0.0/                  # Shallow git clone of tag
│       ├── v1.1.0/
│       ├── v1.2.0/
│       └── current -> v1.2.0/       # Active version
└── webapps.d/                       # Deployed web applications
    └── dashboard/
        ├── v1.0.0/                  # No current symlink, nginx controls routing
        └── v1.1.0/

/etc/ikigai/                         # Configuration (host-specific)
├── ikigai.conf                      # Main configuration
├── agents.conf.d/                   # Per-agent config overrides
│   └── monitoring-agent.conf
├── nginx.d/                         # Per-webapp nginx configs
│   ├── dashboard.conf               # Traffic split for /
│   └── monitoring.conf              # Traffic split for /monitoring/
├── secrets/
│   ├── system/                      # System secrets
│   │   ├── deploy_key               # SSH key for git clone
│   │   └── postgres_admin
│   └── agents/                      # Agent secrets
│       └── monitoring-agent/
│           ├── database_url
│           └── api_key
└── ssl/                             # Certificates

/var/lib/ikigai/                     # Variable state data
├── agents/                          # Agent runtime state
│   └── monitoring-agent/
│       └── state.json
└── cache/                           # Rebuild-able caches

/var/log/ikigai/                     # Logs
├── ikigai.log
├── runtime.log
└── agents/
    └── monitoring-agent.log

/var/run/ikigai/                     # Runtime (PIDs, sockets)
├── ikigai.pid
└── agents/
    └── monitoring-agent.pid

/usr/lib/systemd/system/             # systemd units
├── ikigai.service
├── ikigai-server.service            # Platform controller daemon
└── ikigai-agent@.service            # Template for agents
```

---

## Versioned Deployments

Agents and webapps use versioned directories with a `current` symlink:

```
agents.d/monitoring-agent/
├── v1.0.0/           # Each version is a shallow git clone
│   ├── manifest.json
│   ├── deno.json
│   ├── deno.lock
│   ├── run.ts
│   └── src/
├── v1.1.0/
├── v1.2.0/
└── current -> v1.2.0/
```

- **Deploy**: Clone tag to new version directory, update symlink
- **Rollback**: Update symlink to previous version
- **Cleanup**: Remove old version directories when no longer needed

The systemd unit runs from the `current` symlink, so restarts pick up the new version automatically.

---

## Key Properties

| Path | Purpose | Survives upgrade | Backup required |
|------|---------|------------------|-----------------|
| `/opt/ikigai/` | Code, binaries | Replaced | No |
| `/etc/ikigai/` | Config, secrets | Yes | Yes |
| `/var/lib/ikigai/` | State | Yes | Yes |
| `/var/log/ikigai/` | Logs | Yes | Optional |
| `/var/run/ikigai/` | PIDs, sockets | No (ephemeral) | No |

---

**Next**: [Identity and Security](10-security.md)
