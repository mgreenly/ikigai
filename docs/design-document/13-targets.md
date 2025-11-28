# Remote Targets

> **Part VI: Production Operations** — These sections cover running Ikigai in production: managing multiple servers, backup strategies, and configuration as code.

---

Targets define where commands are executed. A target is simply an SSH destination.

---

## Configuration

```
# ~/.config/ikigai/targets

staging = alice@staging.example.com
production = alice@prod.example.com
```

That's the entire configuration. SSH handles authentication via your existing keys.

---

## Usage

```
> target staging
Switched to staging (staging.example.com)

[staging] > deploy monitoring-agent
Deploying to staging.example.com...

[staging] > target production
Switched to production (prod.example.com)

[production] > deploy monitoring-agent
⚠️  You're deploying to PRODUCTION

Type "deploy" to confirm: deploy

Deploying to prod.example.com...
```

---

## Local Target

The implicit `local` target runs commands in the current directory against local services. This is the default when no target is selected.

```
> run monitoring-agent
Running locally in ~/projects/my-agent...
```

Local is for development and testing. When you're ready to deploy somewhere real, switch targets.

---

## Adding Targets

```
> add target staging

Hostname? staging.example.com

Found SSH config for this host:
  User: alice
  IdentityFile: ~/.ssh/work_ed25519

Use this? [Y/n] y

Testing connection... ✓ connected
Checking ikigai-server... ✓ running (v1.2.3)

Target "staging" added.
```

Ikigai reads your `~/.ssh/config` and uses existing SSH configuration. No duplicate credential management.

---

## How It Works

Under the hood, a remote command is just SSH:

```
> target staging
> deploy monitoring-agent
```

Executes:
```bash
ssh alice@staging.example.com "ikigai-server deploy monitoring-agent"
```

No API server, no tokens, no certificates. SSH keys are the authentication layer—proven, simple, already configured on most developer machines.

---

## Safety Rails

Production targets require confirmation:

```ini
# ~/.config/ikigai/targets

staging = alice@staging.example.com
production = alice@prod.example.com  # confirm=true (auto-detected from name)
```

Any target named "production" or "prod" automatically requires confirmation. You can also set this explicitly:

```ini
production = alice@prod.example.com confirm=true
```

---

**Next**: [Backup and Recovery](14-backup.md)
