# Deployment

> Part of [Cross-Cutting Concerns](09-filesystem.md). Deployment works the same for agents and webapps.

---

## Git-Based Deployment

Deployment is a shallow git clone to a specific tag. No build step, no artifact pipeline. Deno runs TypeScript directly.

```
/opt/ikigai/agents.d/
└── monitoring-agent/
    ├── v1.0.0/              # git clone --depth 1 --branch v1.0.0
    ├── v1.1.0/              # git clone --depth 1 --branch v1.1.0
    ├── v1.2.0/              # git clone --depth 1 --branch v1.2.0
    └── current -> v1.2.0/   # symlink to active version
```

**Every tag is deployable.** The `deno.lock` file ensures reproducible dependencies.

---

## Deployment Process

When developer says "deploy monitoring-agent v1.2.0":

1. **First deploy setup** (if needed):
   - Create Linux user `ikigai-monitoring-agent`
   - Create PostgreSQL role `ikigai_agent_monitoring`
   - Create directories in `/etc`, `/var/lib`, `/var/log`
   - Generate systemd unit from template

2. **Clone version** (if not already present):
   ```bash
   git clone --depth 1 --branch v1.2.0 \
       git@github.com:org/monitoring-agent.git \
       /opt/ikigai/agents.d/monitoring-agent/v1.2.0/
   ```

3. **Update symlink**:
   ```bash
   ln -sfn v1.2.0 /opt/ikigai/agents.d/monitoring-agent/current
   ```

4. **Restart service**:
   ```bash
   systemctl restart ikigai-agent@monitoring-agent
   ```

5. **Health check**: Verify agent responds

---

## Rollback

Rollback is instant. Just change the symlink:

```bash
ln -sfn v1.1.0 /opt/ikigai/agents.d/monitoring-agent/current
systemctl restart ikigai-agent@monitoring-agent
```

All previously deployed versions remain on disk. No artifact retrieval needed.

---

## Version Management

```
> deploy monitoring-agent v1.2.0
Cloning v1.2.0... done
Updating symlink... done
Restarting... done
Health check... ✓

> rollback monitoring-agent
Rolling back to v1.1.0...
Updating symlink... done
Restarting... done
Health check... ✓

> list versions monitoring-agent
  v1.0.0  (deployed 2024-01-01)
  v1.1.0  (deployed 2024-01-10)
* v1.2.0  (current, deployed 2024-01-15)
```

---

## Webapps

Webapps use the same git-based versioning but without a `current` symlink:

```
/opt/ikigai/webapps.d/
└── dashboard/
    ├── v1.0.0/
    └── v1.1.0/
```

Traffic routing is controlled via nginx `split_clients` configuration, allowing independent weight-based splits per webapp. See [Web Portal](06-web-portal.md) for details on nginx configuration and traffic management.

---

## Why Not Artifacts?

Traditional CI/CD builds tarballs because compiled languages need a build step. Deno doesn't:

- TypeScript runs directly (no compilation)
- `deno.lock` pins exact dependency versions
- Git tags are immutable and verifiable
- Shallow clones are fast and small

The artifact pipeline adds complexity without benefit. Git is the artifact store.

---

## GitHub Integration

Deploy keys or GitHub App for clone access:

- Read-only access to agent repositories
- Clone specific tags on demand
- Credentials stored in `/etc/ikigai/secrets/system/`

For private repositories, the server needs appropriate SSH keys or GitHub App credentials configured.

---

**Next**: [Observability](12-observability.md)
