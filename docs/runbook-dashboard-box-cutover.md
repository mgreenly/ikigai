# Runbook (🔒 operator-gated) — cutover of `dashboard` to the appkit/optctl contract on the `ai` box

**Status: PENDING the human operator.** No agent runs any of these commands —
they SSH into the **live customer box** `ai`. This runbook cuts the converted
**dashboard** (appkit one-binary contract, integer-keyed migration runner) over
onto the box for the first time under the new release-dir layout. It is the
operator deliverable for the dashboard cutover noted in `PLAN.md` §5 and
`AGENTS.md` (Deployments).

**Why a cutover and not a plain `bin/deploy`.** Adopting appkit's
**integer-keyed** migration runner renumbered the dashboard's migrations from
their old **name/timestamp-keyed** scheme (`schema_migrations.name`) to
`NNN_*.sql` (+ a new `001_schema_migrations.sql`). A *fresh* DB migrates cleanly
to v5 (verified, tests green). But the live `ai` box's
`/opt/dashboard/data/dashboard.db` already applied the OLD name-keyed ledger,
which the integer runner will **not** recognize — so a plain `optctl install`
against the existing DB would fail to boot. Per the **2026-06-05 directive that
no databases need to be preserved**, the fix is simply to reset the DB before
the install: the new binary then creates and migrates a fresh DB to v5. That is
the only difference from a normal `bin/deploy dashboard`.

> **Cutover in one line:** stop → (optional backup) → drop/reset the DB →
> `bin/deploy dashboard` (no version arg; off-box build of current `main` →
> `optctl install`) → restart → verify. No bespoke `schema_migrations` rewrite,
> no data preservation.

---

## Conventions used below

- **Run from** the repo root `/mnt/projects/ikigai/deployments` on your
  workstation. Never `cd` into `dashboard/` for `bin/deploy` — it resolves paths
  itself.
- `$BOX` = `ai.metaspot.org`, `$SSH` = `ssh -i ~/.ssh/id_ed25519_ai4mgreenly
  ec2-user@ai.metaspot.org` (the values come from `dashboard/etc/deploy.env`:
  `ACCOUNT=ai`, `SSH_USER=ec2-user`, `SSH_KEY=~/.ssh/id_ed25519_ai4mgreenly`;
  `HOST` defaults to `${ACCOUNT}.metaspot.org`). Wherever this runbook says "on
  the box", prefix the command with `$SSH` or run it from an interactive `$SSH`
  shell.
- `optctl` runs privileged on the box: always `sudo optctl …`.
- **CAUTION — this is the live `ai` customer box.** The dashboard is the apex/
  `DEFAULT` app (:3000) and the box's sole trust boundary: while it is stopped,
  the box-wide `/internal/authn` is down, so every `/srv/<svc>/` request 401s.
  Keep the stop→install→restart window short; this is a brief, expected outage
  (seconds–minutes), not a multi-step soak.
- **Secrets:** this runbook never prints, echoes, or reads any secret value or
  the SSH key contents. The dashboard **does** carry a service secret (VAPID +
  IdP config); it must already be seeded via `bin/secrets` (a precondition
  below). The launcher injects it into the process env only — never on disk.

---

## 0. Preconditions

**0a. Interactive SSO login (you run this; the token expires).**

```
aws sso login --profile ai
```

- Expected: a browser opens; on approval the terminal prints
  `Successfully logged into Start URL: https://…`.
- A live SSO session is the standard precondition for any box work (and is
  required for the `bin/secrets` precondition in 0e). Abort/restore: none —
  read-only.

**0b. Confirm SSH reachability and that `ai` is the only account.**

```
ssh -i ~/.ssh/id_ed25519_ai4mgreenly ec2-user@ai.metaspot.org 'hostname; uptime'
```

- Expected: the box's hostname and an uptime line, no password prompt.
- `ai` is the first and **only** account. Abort/restore: none — read-only.

**0c. Confirm `optctl` is already on the box.**

```
ssh … 'optctl --help | head -3'
```

- Expected: the usage banner starting `optctl — ikigai on-box platform CLI`.
  If absent, install it first (see runbook D2 §1: build off-box static
  `linux/amd64`, `scp` to `/tmp`, `sudo install -m 0755 …`). Abort/restore:
  read-only.

**0d. Confirm all MCP services are already deployed (dashboard goes LAST).**

```
ssh … 'systemctl is-active crm ledger notify; ls /opt/*/etc/manifest.env'
```

- Expected: the MCP services that should be live are `active`, and their
  `/opt/<svc>/etc/manifest.env` files exist. **The dashboard is cut over LAST**
  so that on restart it re-reads `/opt/*/etc/manifest.env` and registers every
  already-deployed `MCP=true` service in its OAuth-AS resource list. If a service
  is meant to be live but isn't, bring it up first (its own `bin/deploy`), then
  return here. Abort/restore: read-only.

**0e. Confirm dashboard secrets are seeded.** The dashboard hard-fails at start
if its `app-config` key is missing, so the secret must be present *before* the
restart in §3.

```
bin/secrets dashboard      # operator-side, non-destructive read-modify-write of only the dashboard key
```

- Expected: a masked summary confirming the dashboard's key is present in SSM
  `/metaspot/ai/app-config` (siblings preserved, values never printed). If it
  was already seeded this is a safe no-op. Abort/restore: `bin/secrets` only
  touches its own key; rerun is idempotent.

**0f. Confirm the dashboard is currently serving (the thing we are cutting over).**

```
ssh … 'systemctl is-active dashboard; readlink /opt/dashboard/current 2>/dev/null'
curl -s -o /dev/null -w '%{http_code}\n' https://ai.metaspot.org/   # apex reachable today
```

- Expected: `active`, the current release path (old layout), and a 200/302 from
  the apex. This is the pre-cutover baseline. Abort/restore: read-only.

---

## 1. Stop the dashboard unit

The cutover resets state under `/opt/dashboard/data`, so the unit must be stopped
first (a running dashboard holds the SQLite file open).

```
ssh -i ~/.ssh/id_ed25519_ai4mgreenly ec2-user@ai.metaspot.org \
    'sudo systemctl stop dashboard; systemctl is-active dashboard || true'
```

- Expected: `is-active` reports `inactive`/`failed` (i.e. not `active`).
- **From here until §3 completes, the box-wide `/internal/authn` is down** — all
  `/srv/<svc>/mcp` requests 401. This is the expected brief outage; proceed
  promptly.
- **Abort/restore:** to bail out before touching the DB, just
  `sudo systemctl start dashboard` — the old binary/DB are untouched at this
  point and the box returns to its pre-cutover state.

---

## 2. Reset the dashboard DB (no preservation needed)

The live DB applied the OLD name-keyed ledger; the new integer runner won't
recognize it. Per the 2026-06-05 directive **no databases need to be
preserved**, so reset it. An optional snapshot is offered purely as an operator
safety net, not because the data must survive.

**2a. (Optional) snapshot the existing DB.** Either a plain copy or, if you want
the dashboard's own backup format, its `backup` verb against the *current* (old)
binary before you swap.

```
# plain copy (simplest):
ssh … 'sudo cp -a /opt/dashboard/data/dashboard.db /opt/dashboard/data/dashboard.db.pre-cutover'

# OR the binary's backup verb (old binary still in place; writes a local snapshot):
ssh … 'sudo -u dashboard /opt/dashboard/current/dashboard backup --out /opt/dashboard/data/pre-cutover.bak'
```

- Expected: the snapshot file exists under `/opt/dashboard/data/`. This is
  discardable — it exists only so you can eyeball the old state if something
  surprises you. Abort/restore: it is a copy; the live DB is untouched by this
  step.

**2b. Drop/reset the DB so the new binary creates a fresh one.**

```
ssh … 'sudo rm -f /opt/dashboard/data/dashboard.db \
                  /opt/dashboard/data/dashboard.db-wal \
                  /opt/dashboard/data/dashboard.db-shm'
ssh … 'ls -l /opt/dashboard/data/ || true'
```

- Expected: the `dashboard.db` (and any `-wal`/`-shm` sidecars) are gone; the
  data dir is otherwise intact (the optional snapshot from 2a remains). The new
  binary will create a fresh `dashboard.db` during migrate in §3.
- **Abort/restore:** if you took the 2a snapshot and want to bail out before the
  install, restore it (`sudo cp -a …/dashboard.db.pre-cutover …/dashboard.db`)
  and `sudo systemctl start dashboard` with the OLD `current` still in place. The
  old binary boots the old DB exactly as before.

---

## 3. Bump + deploy the converted dashboard

### 3a. Set the release version

The version source of truth is the committed file `dashboard/VERSION` (a **bare**
number, no leading `v`). `bin/bump dashboard <field>` advances it (commits
`dashboard/VERSION` to `main` + pushes); git tags are no longer the version
mechanism. Throughout this runbook `vX.Y.Z` is a placeholder — substitute the
actual version committed in `dashboard/VERSION` (the first cutover shipped
`v0.1.0`).

```
cat dashboard/VERSION             # -> the bare X.Y.Z that will ship as vX.Y.Z
# (or, to advance:  bin/bump dashboard patch)
```

- Expected: `dashboard/VERSION` holds the bare `X.Y.Z` on `main`.
- Abort/restore: a version bump is a path-limited commit on `main`; nothing is
  shipped to the box until `bin/deploy`.

### 3b. Deploy it (real run)

`bin/deploy <app>` (no version arg) builds current `main` (HEAD) in a throwaway
detached worktree
(`CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -trimpath -buildvcs=false`,
ldflags-stamped), reads the version from that worktree's `dashboard/VERSION`
(→ `vX.Y.Z`), `scp`s the single artifact, then runs the box half:
`ssh sudo optctl install dashboard vX.Y.Z --artifact /tmp/dashboard-vX.Y.Z`.

```
bin/deploy dashboard
```

- Expected (workstation side, the `>>` lines from `bin/deploy`):
  ```
  >> dashboard: building current main (HEAD <sha>)
  >> git worktree add --detach <tmp> HEAD
  >> dashboard: release vX.Y.Z (commit <sha>)
  >> build dashboard -> <tmp-artifact>/dashboard
  >> built dashboard (<size>)
  >> scp dashboard vX.Y.Z -> ai.metaspot.org:/tmp/dashboard-vX.Y.Z
  >> ssh sudo optctl install dashboard vX.Y.Z
  ```
- Expected (box side — `optctl install` progress; the DB was reset in §2, so it
  is created during migrate and there is no pre-migration backup):
  ```
  >> preflight dashboard vX.Y.Z
  >> place artifact -> /opt/dashboard/releases/vX.Y.Z/dashboard
  >> regenerate /opt/dashboard/etc/manifest.env
  >> schema advances but no DB yet (fresh) — no backup
  >> migrate dashboard
  >> atomic swap current -> releases/vX.Y.Z
  >> restart dashboard
  >> installed dashboard vX.Y.Z
  ```
- Final `>> deploy complete: dashboard vX.Y.Z (<sha>)` from the wrapper.
- **Preflight gates** (from `preflight.go`, refuse a bad artifact before touching
  anything live): static `linux/amd64`, `dashboard version` self-report ==
  `vX.Y.Z`, `dashboard manifest` parses (`APP=dashboard`, `MOUNT=/`,
  `DEFAULT=true`, no `MCP`). A failure here aborts with the live release
  untouched — but note the dashboard is **already stopped** and its DB reset, so
  on a preflight abort the box has no serving dashboard: fix and re-deploy
  promptly (recovery in §5).
- **Abort/restore:** if `install` aborts before the atomic swap, nothing is
  repointed — re-run after fixing. If it swapped but the unit did not come up,
  optctl prints `… did not come up (recover with: optctl rollback dashboard)` —
  but on this first new-layout install there is no prior new-layout release to
  roll back to, so recovery is fix-and-redeploy (see §5).

### 3c. Confirm the swap and manifest regen

```
ssh … 'ls -l /opt/dashboard/current /opt/dashboard/bin/run; \
        ls /opt/dashboard/releases; cat /opt/dashboard/etc/manifest.env'
```

- Expected:
  - `current -> releases/vX.Y.Z` (a symlink).
  - `bin/run -> ../current/dashboard` (the STABLE path `metaspot-launch` execs).
  - `releases/` contains `vX.Y.Z`.
  - `etc/manifest.env` is the regenerated manifest:
    `APP=dashboard … MOUNT=/ … DEFAULT=true … PORT=3000` (no `MCP`).

---

## 4. Verify

Run each on the box unless noted (services trust the injected identity headers,
so loopback curl drives endpoints directly — `AGENTS.md` "Verify on the box").

**4a. Unit is active.**
```
systemctl is-active dashboard
```
- Expected: `active`. If not, `journalctl -u dashboard -n 80 --no-pager` (4f) and
  recover per §5.

**4b. Binary self-reports the deployed version (the box can't lie).**
```
/opt/dashboard/current/dashboard version
```
- Expected: `vX.Y.Z (<sha>)` — the version token MUST equal `vX.Y.Z` (the same
  self-report `optctl` preflight asserts).

**4c. Apex reachable over real TLS.**

From your workstation:
```
curl -s -o /dev/null -w '%{http_code}\n' https://ai.metaspot.org/
```
- Expected: `200` (logged-out landing/install page) or `302` to login — not a
  502/connection refused. This proves the fresh-DB dashboard is serving the apex.

**4d. `/internal/authn` is back up (the trust boundary restored).**

The box-wide auth hook is loopback-only. Confirm an unauthenticated service mount
challenges correctly through nginx — i.e. the dashboard is answering the
`auth_request` again:
```
curl -s -i https://ai.metaspot.org/srv/crm/mcp | grep -i 'HTTP/\|WWW-Authenticate'
```
- Expected: `401` with a `WWW-Authenticate:` header. A 502 here instead means the
  dashboard is not answering `/internal/authn` — investigate (4f) / recover (§5).

**4e. Each `/srv/<svc>/mcp` 401 challenge carries `resource_metadata` (services
registered).** For every MCP service that should be live (crm, ledger, notify, …):
```
for svc in crm ledger notify; do
  echo "== $svc =="
  curl -s -i https://ai.metaspot.org/srv/$svc/mcp | grep -i 'WWW-Authenticate'
done
```
- Expected: each `WWW-Authenticate:` value contains
  `resource_metadata="…/.well-known/oauth-protected-resource"`. The presence of
  `resource_metadata` proves the **restarted dashboard re-read
  `/opt/*/etc/manifest.env` and registered that service** in its OAuth-AS
  resource list. (Per `AGENTS.md`: until the dashboard restart, a new service's
  401 omits `resource_metadata` — here the restart in §3b is that restart.)
- Cross-check the inventory directly if available:
  ```
  ssh … 'curl -s http://127.0.0.1:3000/services'   # or the dashboard /services endpoint
  ```
  Expected: every live `MCP=true` service listed (name, mount, MCP resource URL).

**4f. Boot / migration lines in the journal.**
```
journalctl -u dashboard -n 80 --no-pager
```
- Expected: a `starting dashboard` line (addr/db_path/version fields), migration
  progress applying up to embedded **v5** on the fresh DB, and the resource
  derivation reading the on-box manifests. No `Warn`/`Error`. (Per `AGENTS.md`,
  best-effort paths log only at Debug on success — silence ≠ failure; look for
  `Warn`.)

**4g. (Optional) end-to-end auth round-trip.** If feasible, prove the full
plugin → connector → dashboard OAuth → service chain: add a connector for one
service from a Claude client, complete the OAuth round-trip (the dashboard mints
its own opaque token; nginx introspects + injects identity), and call that
service's `<svc>_whoami` MCP tool. A clean `whoami` proves the cut-over dashboard
is a working OAuth AS end-to-end. Out of scope for the mechanism proof, but the
strongest single check.

> Note: because the DB was reset, all prior OAuth AS state (DCR clients, grants,
> web sessions) is gone — any existing connectors must re-authorize, and users
> re-login. This is expected and acceptable under the no-preservation directive.

---

## 5. Rollback / recovery note

`optctl rollback dashboard [version]` repoints `current` → the prior (or named)
release and restarts. Because this cutover **advanced the schema from a fresh
DB**, the downgrade-guard/snapshot behavior applies in principle (rolling back to
a release that embeds fewer migrations would restore the pre-migration snapshot
first) — but with **no DB-preservation requirement**, rollback here is
low-stakes: if anything is wrong, the simplest recovery is to fix the cause, bump
`dashboard/VERSION` (`bin/bump dashboard <field>`), and re-run `bin/deploy
dashboard` (the artifact is rebuildable from the committed `main` history).

- **On a failed first new-layout install** (no prior new-layout release to roll
  back to): the dashboard is stopped/DB-reset; fix the cause and re-run `bin/deploy
  dashboard <ver>`. If you must restore the box's *old* serving state instead,
  re-point `current` at the old release dir and restore the §2a snapshot — but
  under the no-preservation directive the forward fix (re-deploy) is preferred.
- **On a later install** (a prior new-layout release exists):
  `sudo optctl rollback dashboard` repoints to it and restarts; the data DB is
  only touched (snapshot/restore) if the rolled-back-from release advanced the
  schema.

**Confirm the box is back to a serving state at the end** (regardless of where
you stopped):

```
ssh … 'systemctl is-active dashboard nginx crm ledger notify'   # all 'active'
ssh … 'readlink /opt/dashboard/current'                          # intended release
ssh … '/opt/dashboard/current/dashboard version'                 # matches that release
curl -s -o /dev/null -w '%{http_code}\n' https://ai.metaspot.org/   # 200/302
```

---

## Cleanup (optional, after a successful cutover)

Leave `dashboard` deployed (it is the live apex app); the shipped version stays
recorded in the committed `dashboard/VERSION` on `main`. The throwaway `bin/deploy`
worktree and `/tmp` artifacts are removed automatically (the wrapper's `trap
cleanup EXIT`). The optional
`/opt/dashboard/data/*.pre-cutover*` snapshot from §2a can be removed once you
are satisfied — it is not needed (no DB preservation).
