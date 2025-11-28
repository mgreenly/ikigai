# Identity and Security

> Part of [Cross-Cutting Concerns](09-filesystem.md). Identity applies to all pillars—developers access the Terminal, agents run as services, web users authenticate through the Portal.

---

## Philosophy

The platform uses Linux as its identity layer. No custom authentication system—identity is Linux users, authorization is file permissions and PostgreSQL roles, federation is PAM.

---

## Identity Model

| Entity | Linux User | PostgreSQL Role |
|--------|------------|-----------------|
| Developer | `alice` (human user) | `ikigai_dev_alice` |
| Admin | `bob` (human user, sudo) | `ikigai_admin` |
| Agent | `ikigai-monitoring-agent` (system user) | `ikigai_agent_monitoring` |

**Developer users** are regular Linux users, managed through standard tools. They authenticate to remote servers via SSH keys. PostgreSQL maps their Linux identity to database roles via `pg_ident.conf`.

**Agent users** are system users created automatically on first deploy:

```bash
useradd --system \
        --no-create-home \
        --shell /usr/sbin/nologin \
        ikigai-monitoring-agent
```

Agents cannot log in interactively. They exist solely to own files and run as systemd services.

---

## Secrets Management

Secrets are files with restricted permissions:

```
/etc/ikigai/secrets/agents/monitoring-agent/
├── database_url     # mode 600, owner ikigai-monitoring-agent
└── api_key          # mode 600, owner ikigai-monitoring-agent
```

Only the owning agent user can read its secrets. Developers with appropriate sudo access can manage secrets. No secrets management service required.

**Setting a secret via Ikigai:**

```
> set secret monitoring-agent api_key
Enter value: ********
```

Ikigai SSHs to the server, sudo's to the agent user, writes the file with correct permissions.

---

## PostgreSQL Access Control

Each agent has a dedicated PostgreSQL role with minimal required permissions:

```sql
-- Role for monitoring-agent
CREATE ROLE ikigai_agent_monitoring;

-- Can only access its queues and mailboxes
GRANT SELECT, UPDATE ON task_queue TO ikigai_agent_monitoring;
GRANT SELECT, INSERT ON mailbox TO ikigai_agent_monitoring;
GRANT INSERT ON telemetry TO ikigai_agent_monitoring;

-- Row-level security enforces queue/mailbox access
ALTER TABLE task_queue ENABLE ROW LEVEL SECURITY;
CREATE POLICY monitoring_queue_access ON task_queue
    FOR ALL TO ikigai_agent_monitoring
    USING (queue_name = 'monitoring-tasks');
```

---

## Enterprise Integration

For organizations with existing identity infrastructure:

- **LDAP/Active Directory**: Configure PAM with `sssd` or `pam_ldap`
- **SSO**: PAM modules for SAML, OIDC
- **Centralized sudo**: Manage developer permissions via LDAP groups

The platform inherits whatever identity infrastructure the organization already uses.

---

**Next**: [Deployment](11-deployment.md)
