# GitOps Configuration

> Part of [Production Operations](13-targets.md). GitOps provides configuration-as-code for production deployments.

---

In production mode, the git repository is the source of truth for platform configuration.

---

## Repository Structure

```
ikigai-platform/
├── platform.yaml           # Global platform settings
├── agents/
│   ├── monitoring-agent/
│   │   ├── manifest.json
│   │   └── config.yaml     # Environment-specific overrides
│   └── alerting-agent/
│       ├── manifest.json
│       └── config.yaml
└── webapps/
    └── dashboard/
        └── manifest.json
```

---

## Workflow

```
Developer                    GitHub                      Server
    │                           │                           │
    │  "deploy monitoring-agent"│                           │
    │──────────────────────────▶│                           │
    │   (ikigai commits)        │                           │
    │                           │  webhook                  │
    │                           │──────────────────────────▶│
    │                           │                           │
    │                           │      ikigai-server        │
    │                           │      pulls & applies      │
    │                           │                           │
    │                           │◀──────────────────────────│
    │                           │   status update           │
    │◀──────────────────────────│                           │
    │   "deployed successfully" │                           │
```

---

## What Goes in Git vs. What Doesn't

| In Git | Not in Git |
|--------|------------|
| Agent manifests | Secrets |
| Agent config (non-secret) | PostgreSQL data |
| Platform settings | Runtime state |
| Webapp manifests | Logs |

Secrets are backed up separately to S3 with encryption. See [Backup and Recovery](14-backup.md).

---

## Repository Setup

```
> setup gitops

I'll create a repository to manage your platform configuration.

Repository name: [ikigai-platform]
GitHub org or user: [your-username]

⚠️  This repository should be PRIVATE. It will contain:
  • Agent configurations and manifests
  • Platform settings
  • Deployment history

It will NOT contain secrets (those are backed up to S3).

Create? [Y/n]

Creating repository... done
Configuring webhook... done
Exporting current config... done
Initial commit... done

GitOps mode enabled. Configuration changes will now be
committed to git and applied via webhook.
```

---

**Next**: [Part VII: Reference](16-appendix.md) — terminology, open questions, and future considerations.
