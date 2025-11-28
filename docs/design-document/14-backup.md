# Backup and Recovery

> Part of [Production Operations](13-targets.md). Backup is critical for production deployments.

---

Backup is optional for solo creators but critical for production. The platform supports both modes without requiring infrastructure decisions upfront.

---

## Solo Creator: No Backup (Default)

For experimentation and learning, no backup is configured. The `/doctor` command gently reminds you:

```
> /doctor

✓ PostgreSQL running
✓ Deno installed
✓ 2 agents running

ℹ No backup configured
  └─ That's fine for experimentation
  └─ Say "setup backups" when you're ready to get serious
```

---

## Production: Full Backup

Production deployments back up three things:

| What | Where | How |
|------|-------|-----|
| PostgreSQL data | S3-compatible storage | WAL archiving + periodic base backups |
| Secrets | S3-compatible storage | Encrypted blob |
| Configuration | Git repository | Committed on every change |

---

## PostgreSQL Backup

WAL (Write-Ahead Log) archiving provides continuous backup with minimal data loss:

```ini
# /etc/ikigai/ikigai.conf

[backup]
enabled = true
s3_bucket = s3://your-bucket/ikigai-backup
s3_region = us-east-1

# WAL archiving (near-zero RPO)
wal_archive = true

# Base backup schedule
base_backup_schedule = 0 3 * * *   # daily at 3am
```

Ikigai configures PostgreSQL automatically:

```ini
# postgresql.conf (managed by ikigai)
archive_mode = on
archive_command = '/opt/ikigai/bin/ikigai-wal-archive %p %f'
```

---

## Secrets Backup

Secrets are encrypted before upload. The encryption key is derived from a passphrase you provide during setup:

```
> setup backups

Configuring backup to S3...

S3 bucket URL? s3://my-bucket/ikigai-backup

Secrets need a backup passphrase. This passphrase encrypts your
secrets before upload. Store it securely—without it, secrets
cannot be restored.

Passphrase: ********
Confirm: ********

Testing S3 access... ✓
Configuring WAL archiving... ✓
Backing up current secrets... ✓

Backup configured. Run "/doctor" anytime to check status.
```

---

## Disaster Recovery

Restoring to a new server:

```
$ sudo apt install ikigai
$ ikigai restore --from s3://my-bucket/ikigai-backup

Restoring Ikigai platform...

Found backup from 2024-01-15 10:23:00

Restoring PostgreSQL... done
  (point-in-time recovery to 10:22:45)

Restoring secrets...
  Passphrase: ********
  Decrypting... done

Restoring configuration...
  Cloning git@github.com:you/ikigai-platform.git... done

Starting services... done

Platform restored. Run "/doctor" to verify.
```

---

## The /doctor Command

`/doctor` is a diagnostic command that audits platform health and offers to fix issues.

### Solo Creator View

```
> /doctor

Checking platform health...

✓ PostgreSQL running (16.1)
✓ Deno installed (2.0.1)
✓ ikigai-server running
✓ 2 agents deployed
  ├─ monitoring-agent: running (up 3 days)
  └─ alerting-agent: running (up 3 days)

ℹ No backup configured
  └─ That's fine for experimentation

Platform is healthy.
```

### Production View

```
> /doctor

Checking platform health...

✓ PostgreSQL running (16.1)
✓ Deno installed (2.0.1)
✓ ikigai-server running (gitops mode)
✓ Config synced with github.com/you/ikigai-platform
  └─ Last sync: 2 minutes ago
✓ WAL archiving to s3://backup-bucket
  └─ Last archive: 5 minutes ago
✓ Secrets backed up
  └─ Last backup: 1 hour ago
✓ 5 agents deployed, all healthy

Platform is healthy.
```

### When Problems Exist

```
> /doctor

Checking platform health...

✓ PostgreSQL running (16.1)
✓ Deno installed (2.0.1)
✗ ikigai-server not running
  └─ Last exit: 10 minutes ago, exit code 1
  └─ Check logs: journalctl -u ikigai-server

✗ monitoring-agent unhealthy
  └─ Not responding to health checks
  └─ Last log: "connection refused to postgres"

✓ Config synced
✗ WAL archiving behind
  └─ Last archive: 3 hours ago (expected: <10 minutes)
  └─ Check: PostgreSQL archive_command may be failing

3 issues found. Want me to investigate?
```

If you say yes, `/doctor` digs deeper, explains what it finds, and offers to fix what it can.

---

**Next**: [GitOps Configuration](15-gitops.md)
