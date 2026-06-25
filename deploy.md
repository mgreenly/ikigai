# Deploying — bump → ship → stage → deploy

> ⚠️ **`int.ikigenba.com` is the live account.** Do **not** run any of the steps
> below — or otherwise `ssh int` / invoke `opsctl` against the box, even
> read-only — unless you've been **explicitly told to deploy**. The default
> workflow is local-only (`bin/start`); deploying is a separate, explicit
> request.

We deploy to **`int.ikigenba.com`** (the first and only account, `int`). Your
`~/.ssh/config` already has a `Host int.ikigenba.com` (alias `int`) entry pinning
the right key, so `ssh int` and the deploy scripts connect with the correct
identity automatically — no `-i` flag needed.

Deploy ships one static binary into a versioned release dir — **not** `git push`
and **not** an in-place overwrite. Run **both `bin/bump` and `bin/ship` from the
standing `main` worktree** (`git worktree list` shows it — do not hard-code the
path; the repo has been renamed before). `bin/ship` builds that worktree's HEAD,
so invoking it from a feature worktree would build a possibly-stale tree;
`bin/ship` refuses to run off `main` and exits with an error.

## Pre-flight

Before starting, confirm the two channels a deploy needs are live:

- **SSO** (workstation → SSM, needed for the secrets step):
  `aws sts get-caller-identity --profile int`. If it fails, the operator runs
  `aws sso login --profile int` interactively.
- **Box reachable:** `ssh int true`.

## 0. Determine what actually needs deploying

A bump-the-version-and-ship loop only makes sense for apps whose **code** changed
since what's live. Two facts make this checkable:

- **`opsctl status` prints the git commit each live binary was built from** (the
  `SHA` column). So `git diff <box-sha> HEAD -- <app>/` shows exactly what
  changed for that app since it was deployed.
- **Matching VERSION ≠ matching code.** Code routinely advances on `main`
  without a version bump (the build loops don't bump). So an app can be "v0.5.0"
  on the box and "v0.5.0" on `main` yet have a different binary.

> ⚠️ **If the code changed but the VERSION did not, you MUST `bin/bump` before
> staging.** Releases are immutable: `opsctl stage` will refuse to overwrite an
> existing `releases/<ver>/` with a different binary (the SHA-collision guard).
> Bump first, then ship the new version.

Skip apps whose only diff is docs (`project/`, `*.md`) or a transitive-only
`go.mod`/`go.sum` change — those produce a functionally identical binary and
deploying them is churn.

## 1. Seed secrets (only if an app's secrets are new / changed / unseeded)

Secret-bearing apps read their keys from the shared SSM SecureString
`/ikigenba/<account>/app-config` (one blob; each app owns key `.["<app>"]`).
**`ikigenba-launch` hard-fails to boot an app whose key is missing**, and an app
that gained a new required key will fail at runtime without it — so seed *before*
deploying that app. Run the app's own script and type `yes`:

```
<svc>/bin/secrets        # e.g. prompts/bin/secrets, wiki/bin/secrets, gmail/bin/secrets
```

It does a non-destructive read-modify-write of only that app's key (sibling apps'
keys are preserved by a clobber-guard) and never prints secret values — only
masked summaries. Values resolve from `~/.secrets/<NAME>` (or an env override).
Apps with no `bin/secrets` (crm, ledger, cron, scripts, sites) need no seeding.

## 2–5. bump → ship → stage → deploy (per app)

1. **`bin/bump <app> <major|minor|patch>`** — advance the committed bare-SemVer
   `<app>/VERSION` on `main` (the single source of truth) and push it. Skip if
   the version is already where you want it (e.g. you bumped it by hand earlier).
2. **`bin/ship <app>`** — the off-box half. Builds current `main` (HEAD) as a
   static `linux/amd64` binary in a throwaway git worktree, `scp`s the artifact
   to the box `/tmp`, then **prints the two box commands and stops** — it makes
   no other change on the box.
3. **`ssh int sudo opsctl stage <app> v<ver> --artifact /tmp/<app>-v<ver>`** —
   preflight + SHA-collision guard, place the binary into `releases/<ver>/`, and
   delete the `/tmp` artifact on success. Stages a release without making it live.
4. **`ssh int sudo opsctl deploy <app> v<ver>`** — regenerate `etc/manifest.env`
   from the new binary, back up the DB if the schema advances, `migrate`, atomic
   swap `current`, restart the unit, and prune old releases.

**Order across apps:** deploy the **services first and the dashboard last**. The
dashboard re-derives its authorization-server resources from *every* service
manifest on restart, so deploying it last makes its own restart pick up all the
new manifests (versions, new MCP tools) — no separate dashboard restart needed.
A brand-new service needs `opsctl setup <app>` first (and `opsctl init-box` once
per box); these are one-time.

## 6. Verify

Don't stop at "deployed." Confirm each app you touched:

- **Unit up:** `ssh int 'sudo systemctl is-active <app>'` → `active`.
- **Live version:** `ssh int 'sudo opsctl status'` → the new `v<ver>` and the
  HEAD `SHA`.
- **Loopback health:** `ssh int 'curl -s -m5 -o /dev/null -w "%{http_code}" http://127.0.0.1:<port>/health'`
  → `200`.
- **Public chain through nginx** (proves the trust boundary, not just the
  loopback): `https://int.ikigenba.com/` → `200`; `/srv/<svc>/mcp` → **`401`**
  (auth required — a `502`/`503` means the loopback service is down).
- **Event plane:** for producers/consumers, the logs should show feeds
  reconnecting — `ssh int 'sudo journalctl -u <app> -n 30 --no-pager'`.

## Rollback & inspection

- **Roll back:** `ssh int sudo opsctl rollback <app> [ver]` (restores the
  pre-migration DB backup if the rolled-back release advanced the schema).
- **Inspect:** `opsctl status` / `opsctl releases <app>`.
- **Logs:** use `sudo journalctl -u <app> -n N --no-pager` for a **bounded**
  read. `opsctl tail <app>` *follows* the stream and never exits — fine
  interactively, but it will hang a scripted/automated command.

## Special case — migrations won't apply cleanly to the existing DB

When a service's migration lineage was reset (e.g. a rebuild) the new migrations
won't apply onto the old `schema_migrations`, and you've decided the live data
need not be preserved. opsctl supports a fresh start: if no DB file exists at
deploy time it logs *"schema advances but no DB yet — no backup"* and migrates
from empty. Procedure:

```
ssh int sudo opsctl stage <app> v<ver> --artifact /tmp/<app>-v<ver>
ssh int 'sudo systemctl stop <app> && sudo mv /opt/<app>/data/<app>.db /opt/<app>/data/<app>.db.OLD'
ssh int sudo opsctl deploy <app> v<ver>     # migrates a fresh DB, restarts
```

> ⚠️ This **discards the data** and is **not reversible** — `rollback` cannot
> recover a DB you moved aside. Only do it when told the DB need not be
> preserved. Stopping the unit first lets SQLite checkpoint and drop the
> `-wal`/`-shm` sidecars, so only `<app>.db` remains to move.

## References

The full deploy model is `docs/archive/adr-deployment-redesign.md`; versioning is
`docs/archive/versioning.md`; per-service details live under each service's own
directory and `CLAUDE.md`.
