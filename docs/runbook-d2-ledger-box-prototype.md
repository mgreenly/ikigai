# Runbook D2 (🔒 operator-gated) — prototype deploy + rollback of `ledger` on the `ai` box

**Status: PENDING the human operator.** This is step D2 of `PLAN.md` §3. No agent
runs any of these commands — they SSH into the **live customer box** `ai`. The
acceptance gate "operator runs the full install → verify → rollback → verify loop
green" is only satisfied once a human executes this runbook and pastes results
back. Until then D2 stays unchecked in `PLAN.md` §6.

This runbook proves the whole P1 design end-to-end on one real service: build a
tagged `ledger` off-box, hand it to the new on-box `opsctl`, verify it serves,
roll it back, re-verify, and prune. It exercises `opsctl setup`, `bin/deploy` →
`opsctl install`, `opsctl rollback`, and `opsctl prune` — the exact CLI surfaces
in `opsctl/cmd/opsctl/main.go`, `opsctl/internal/opsctl/*`, and `bin/deploy`.

---

## Conventions used below

- **Run from** the repo root `/mnt/projects/ikigai/deployments` on your
  workstation. Never `cd` into a sub-service for these — `bin/deploy` resolves
  paths itself.
- `$BOX` = `int.ikigenba.com`, `$SSH` = `ssh -i ~/.ssh/id_ed25519_int_ikigenba_com
  ec2-user@int.ikigenba.com` (the values come from `ledger/etc/deploy.env`:
  `ACCOUNT=int`, `SSH_USER=ec2-user`, `SSH_KEY=~/.ssh/id_ed25519_int_ikigenba_com`;
  `HOST` defaults to `${ACCOUNT}.ikigenba.com`). Wherever this runbook says "on
  the box", prefix the command with `$SSH` or run it from an interactive `$SSH`
  shell.
- `opsctl` runs privileged on the box: always `sudo opsctl …`.
- **CAUTION — this is the live `ai` customer box.** It already serves the
  dashboard (apex/`DEFAULT`, :3000), `crm` (:3001) and `notify` (:3003). `ledger`
  (:3002) is being brought up here for the first time under the *new* release-dir
  layout. Every mutating step below has an **Abort/restore** line; read it before
  you run the step. A dashboard restart is NOT part of this runbook, so the
  box-wide `/internal/authn` is never interrupted.
- **Secrets:** this runbook never prints, echoes, or reads any secret value or the
  SSH key contents (§2.8). `ledger` carries **no service secret** (it is a
  loopback producer; its only config is the non-secret manifest), so no `secrets`
  seeding step is required before its first start. If a future service needs one,
  seed it operator-side *before* first start; do not put it here.

---

## 0. Preconditions

**0a. Interactive SSO login (you run this; the token expires).**

```
aws sso login --profile int
```

- Expected: a browser opens; on approval the terminal prints
  `Successfully logged into Start URL: https://…`.
- This runbook does **not** call `aws` directly (no SSM/secrets steps for
  `ledger`), but a live SSO session is the standard precondition for any box work
  and is cheap to confirm. Abort/restore: none — read-only.

**0b. Confirm SSH reachability and that `ai` is the only account.**

```
ssh -i ~/.ssh/id_ed25519_int_ikigenba_com ec2-user@int.ikigenba.com 'hostname; uptime'
```

- Expected: the box's hostname and an uptime line, no password prompt.
- `ai` is the first and **only** account (per `AGENTS.md` Deployments). There is
  no second box to confuse this with. Abort/restore: none — read-only.

**0c. Confirm the box substrate already exists (so we can skip `init-box`).**

```
ssh … 'systemctl is-active nginx dashboard; ls -d /etc/nginx/conf.d/locations'
```

- Expected: `active` for both nginx and dashboard, and the locations include dir
  exists. This is the proof the apex/`init-box` substrate is already in place
  (see §2 below). Abort/restore: none — read-only.

---

## 1. Build + push `opsctl` to the box

`opsctl` is a Go binary and **the box never compiles** (§2.1). Build it OFF-box,
static, `linux/amd64`, then ship it.

**1a. Build `opsctl` off-box.**

```
( cd opsctl && CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off \
    go build -trimpath -buildvcs=false -o /tmp/opsctl ./cmd/opsctl )
file /tmp/opsctl
```

- Expected: no build output; `file /tmp/opsctl` reports
  `ELF 64-bit LSB executable, x86-64, … statically linked …`.
- Note the subshell `( cd … )` so your shell's cwd is unaffected.
- Abort/restore: build failures are local only — nothing shipped. Fix and rerun.

**1b. Ship it to the box and install it.**

```
scp -i ~/.ssh/id_ed25519_int_ikigenba_com /tmp/opsctl ec2-user@int.ikigenba.com:/tmp/opsctl
ssh -i ~/.ssh/id_ed25519_int_ikigenba_com ec2-user@int.ikigenba.com \
    'sudo install -m 0755 /tmp/opsctl /usr/local/bin/opsctl && opsctl --help | head -3'
```

- Expected: `scp` shows a 100% transfer; the `opsctl --help` head prints the
  usage banner starting `opsctl — suite on-box platform CLI`.
- Abort/restore: `opsctl` is a standalone binary that does nothing until invoked;
  installing it touches no running service. To remove:
  `ssh … 'sudo rm -f /usr/local/bin/opsctl'`.

---

## 2. Provision `ledger` on the box (`opsctl setup ledger`)

### init-box finding — DO NOT re-run `init-box` on this box

`opsctl init-box` provisions the **box-global apex substrate**: it (re)writes the
apex nginx `server{}` block at `/etc/nginx/conf.d/<default-app>.conf`, reloads
nginx, and **re-runs certbot** for the apex cert (`opsctl/internal/opsctl/
initbox.go`). The `ai` box already serves the dashboard apex, so that substrate
**already exists** (confirmed in step 0c). Re-running `init-box` would rewrite the
live apex block and touch the cert for **no benefit** — and risks disturbing the
trust boundary the whole box depends on.

**Therefore: SKIP `init-box` entirely. `opsctl setup ledger` is the only
provisioning this prototype needs.** `setup` checks that the box-global
`conf.d/locations/` dir exists (created by the original dashboard provisioning)
and fails fast if it does not — so it is safe and self-guarding.

**2a. Stage `ledger`'s nginx fragment source on the box.**

`opsctl setup` reads the fragment SOURCE from a path you pass via `--fragment`
(the operator stages it next to where `opsctl` runs — carry-forward from D1). Copy
the committed `ledger/etc/nginx.conf` up:

```
scp -i ~/.ssh/id_ed25519_int_ikigenba_com ledger/etc/nginx.conf \
    ec2-user@int.ikigenba.com:/tmp/ledger-nginx.conf
```

- Expected: 100% transfer.
- Abort/restore: it lands in `/tmp`; nothing live touched.

**2b. Run `setup`.**

The real flag surface (from `opsctl/cmd/opsctl/main.go cmdSetup`) is:

```
opsctl setup <app> [--port N] [--fragment <path>]
```

`ledger` binds loopback **3002** (`ledger/etc/manifest.env PORT=3002`,
`ledger/etc/deploy.env`), and its fragment templates `__PORT__`. So, on the box:

```
ssh -i ~/.ssh/id_ed25519_int_ikigenba_com ec2-user@int.ikigenba.com \
    'sudo opsctl setup ledger --port 3002 --fragment /tmp/ledger-nginx.conf'
```

- Expected (the `>>`-prefixed progress lines, in order, from
  `opsctl/internal/opsctl/setup.go`):
  ```
  >> ensure app user ledger (home /opt/ledger)
  >> create /opt/ledger tree
  >> write systemd unit /etc/systemd/system/ledger.service
  >> write nginx fragment /etc/nginx/conf.d/locations/ledger.conf
  >> setup complete for ledger — next: opsctl install ledger <version>
  ```
- `setup` **enables but does NOT start** the unit (there is no binary yet), creates
  the `--system` `ledger` user and the `/opt/ledger/{releases,bin,etc,data,backups}`
  tree, writes `ledger.service` (`ExecStart=/usr/local/bin/ikigenba-launch ledger`),
  drops the `__PORT__`-substituted fragment, and runs `nginx -t` + reload.
- If you see `setup: …/conf.d/locations missing — run opsctl init-box first`: the
  box substrate is NOT what step 0c implied — STOP and reassess; do **not** run
  `init-box` against the live apex without a separate decision.
- **Abort/restore** (setup is idempotent; to fully undo before any deploy):
  ```
  ssh … 'sudo systemctl disable ledger 2>/dev/null; \
         sudo rm -f /etc/systemd/system/ledger.service \
                    /etc/nginx/conf.d/locations/ledger.conf; \
         sudo systemctl daemon-reload; sudo nginx -t && sudo systemctl reload nginx; \
         sudo rm -rf /opt/ledger'
  ```
  (Removing `/opt/ledger` is safe here only because no DB exists yet — this is the
  pre-deploy state. Never `rm -rf /opt/ledger` once `data/ledger.db` holds real
  state.)

---

## 3. Bump + deploy v1

### 3a. Set the release version

The version source of truth is the committed file `ledger/VERSION` (a **bare**
number, e.g. `0.1.0`). `bin/bump ledger <field>` advances it (commits `ledger/VERSION`
to `main` + pushes); for this first release ensure the committed value is `0.1.0`
(`cat ledger/VERSION`, or `bin/bump ledger patch`/`minor` to land on it). Git tags
are no longer the version mechanism.

```
cat ledger/VERSION                # -> 0.1.0
# (or, to advance:  bin/bump ledger patch)
```

- Expected: `ledger/VERSION` holds the bare `0.1.0` on `main`.
- Abort/restore: a version bump is a path-limited commit on `main`; nothing is
  shipped to the box until `bin/deploy`.

### 3b. Deploy it (real run — not `--dry-run`)

The wrapper's surface (`bin/deploy`): `bin/deploy <app>` — **no version arg**. It
builds current `main` (HEAD) in a throwaway detached worktree, reads the version
from that worktree's `ledger/VERSION` (here `0.1.0` → release `v0.1.0`), `scp`s the
artifact, then runs the box half:
`ssh sudo opsctl install ledger v0.1.0 --artifact /tmp/ledger-v0.1.0`.

```
bin/deploy ledger
```

- Expected (workstation side, the `>>` lines from `bin/deploy`):
  ```
  >> ledger: building current main (HEAD <sha>)
  >> git worktree add --detach <tmp> HEAD
  >> ledger: release v0.1.0 (commit <sha>)
  >> build ledger -> <tmp-artifact>/ledger
  >> built ledger (<size>)
  >> scp ledger v0.1.0 -> int.ikigenba.com:/tmp/ledger-v0.1.0
  >> ssh sudo opsctl install ledger v0.1.0
  ```
- Expected (box side — `opsctl install` progress, from
  `opsctl/internal/opsctl/install.go`; first install so the DB is created during
  migrate and there is no pre-migration backup):
  ```
  >> preflight ledger v0.1.0
  >> place artifact -> /opt/ledger/releases/v0.1.0/ledger
  >> regenerate /opt/ledger/etc/manifest.env
  >> schema advances but no DB yet (first install) — no backup
  >> migrate ledger
  >> atomic swap current -> releases/v0.1.0
  >> restart ledger
  >> installed ledger v0.1.0
  ```
- Final box `>> deploy complete: ledger v0.1.0 (<sha>)` from the wrapper.

**On-box end state to confirm:**

```
ssh … 'ls -l /opt/ledger/current /opt/ledger/bin/run; \
        ls /opt/ledger/releases; cat /opt/ledger/etc/manifest.env'
```

- Expected:
  - `current -> releases/v0.1.0` (a symlink).
  - `bin/run -> ../current/ledger` (the STABLE path `ikigenba-launch` execs).
  - `releases/` contains `v0.1.0`.
  - `etc/manifest.env` is the regenerated manifest (byte-equals the committed
    `ledger/etc/manifest.env`: `APP=ledger … PORT=3002 … FEED=/feed …`).
- **Preflight gates** (refuse a bad artifact before touching anything live, from
  `preflight.go`): static `linux/amd64`, `ledger version` self-report ==
  `v0.1.0`, `ledger manifest` parses with `APP=ledger`. A failure here aborts with
  the (empty) live release untouched.
- **Abort/restore:** if `install` aborts mid-way, nothing is swapped (the swap is
  the last-but-two step); rerun after fixing. If `install` swapped but the unit
  did not come up, opsctl returns `… did not come up (recover with: opsctl
  rollback ledger)` — but there is no prior release to roll back to on a first
  install, so recovery is: fix the cause and re-deploy, or tear down per §2's
  Abort/restore (safe only because the DB is brand-new). The data DB is never
  overwritten (§2.7).

---

## 4. Verify v1

Run each on the box (services trust the injected identity headers, so loopback
curl drives `/mcp` directly — `AGENTS.md` "Verify on the box").

**4a. Unit is active.**
```
systemctl is-active ledger
```
- Expected: `active`. Abort/restore: if not active, `journalctl -u ledger -n 50`
  (see 4f); recover via `opsctl rollback` only once a prior release exists (it does
  not yet) — otherwise fix + re-deploy.

**4b. Loopback PRM → 200.**
```
curl -s -o /dev/null -w '%{http_code}\n' \
     http://127.0.0.1:3002/.well-known/oauth-protected-resource
```
- Expected: `200`.

**4c. Loopback `/mcp` with no identity headers → 401 carrying `resource_metadata`.**
```
curl -s -i http://127.0.0.1:3002/mcp | grep -i 'HTTP/\|WWW-Authenticate'
```
- Expected: a `401` status and a `WWW-Authenticate:` header whose value contains
  `resource_metadata="…/.well-known/oauth-protected-resource"`. (The loopback
  `/mcp` has no nginx-injected `X-Owner-Email`/`X-Client-Id`, so appkit's identity
  gate challenges.)

**4d. Public `/srv/ledger/mcp` 401 carries `resource_metadata`.**

From your workstation (through nginx + TLS), unauthenticated:
```
curl -s -i https://int.ikigenba.com/srv/ledger/mcp | grep -i 'HTTP/\|WWW-Authenticate'
```
- Expected: `401` with a `WWW-Authenticate:` carrying `resource_metadata=…`. This
  proves the nginx fragment routes the mount and the PRM bootstrap is reachable.
  Also confirm the open PRM path returns 200 publicly:
  ```
  curl -s -o /dev/null -w '%{http_code}\n' \
       https://int.ikigenba.com/srv/ledger/.well-known/oauth-protected-resource
  ```
  Expected `200`.

**4e. Binary self-reports the deployed version (the box can't lie).**
```
/opt/ledger/current/ledger version
```
- Expected: `v0.1.0 (<sha>)` — the version token MUST equal `v0.1.0` (this is the
  same self-report `opsctl` preflight asserts against the install arg).

**4f. Boot / migration lines in the journal.**
```
journalctl -u ledger -n 50 --no-pager
```
- Expected: a `starting ledger` line with `addr`, `resource_id`, `auth_server`,
  `db_path`, `version` fields, and migration progress (the runner applying up to
  embedded version 3). No `Warn`/`Error`. (Per `AGENTS.md`, best-effort paths log
  only at Debug on success — silence ≠ failure; look for `Warn`.)

> Optional deeper auth proof (not required for the loop, mirrors the notify
> worked example): a connector OAuth round-trip from a Claude client + the
> `ikigenba_ledger_health` MCP tool. Out of scope for the install/rollback mechanism proof.

---

## 5. Rollback drill

The goal is to prove the **mechanism**: install a second release, roll back to the
first, and confirm `current` repoints and the binary self-reports v0.1.0 again.

### 5a. Bump + deploy a second version

A second build with a new version stamp is sufficient to exercise the
swap/rollback machinery (the binaries differ only by the ldflags-stamped version,
which is exactly what we assert on). `bin/bump ledger patch` advances
`ledger/VERSION` 0.1.0 → 0.1.1 (commit to `main` + push); `bin/deploy` then builds
current `main` and ships `v0.1.1`.

```
bin/bump ledger patch             # ledger/VERSION 0.1.0 -> 0.1.1, commit to main + push
bin/deploy ledger
```

- Expected (box side): the same `>> preflight … place … regenerate … migrate …
  atomic swap current -> releases/v0.1.1 … restart … installed ledger v0.1.1`
  sequence. Because v0.1.1 embeds the **same** migrations as v0.1.0
  (`applied == embedded`, both 3), install reports **no schema advance** and takes
  **no** pre-migration backup — so the rollback in 5c will be a pure symlink
  repoint, no DB restore.
- Confirm: `ssh … 'ls /opt/ledger/releases; readlink /opt/ledger/current'`
  → `releases/` has `v0.1.0` and `v0.1.1`; `current` → `releases/v0.1.1`.
- `/opt/ledger/current/ledger version` → `v0.1.1 (<sha>)`.
- **Abort/restore:** identical to §3b — a failed install never repoints `current`;
  if it swapped but failed `is-active`, run the rollback in 5c immediately.

> **Optional extended drill (schema-advance path).** To also exercise the
> backup-on-advance → restore-on-rollback path, v0.1.1 must carry a **new,
> higher-numbered, additive** migration (§2.5) so install reports
> `applied=3 embedded=4`, logs `schema advances — backup … -> backups/pre-v0.1.1.db`,
> and the §5c rollback first runs `ledger restore --from …/backups/pre-v0.1.1.db`
> before the swap. That requires a real migration change in the `ledger` tree and a
> fresh version bump — out of scope for the minimal mechanism proof, but the opsctl code
> path is in place (`install.go` step 4, `rollback.go` step 2).

### 5b. (Confirm pre-rollback state)
```
systemctl is-active ledger        # active
readlink /opt/ledger/current      # releases/v0.1.1
/opt/ledger/current/ledger version  # v0.1.1 (<sha>)
```

### 5c. Roll back

The surface (`opsctl/cmd/opsctl/main.go cmdRollback`): `opsctl rollback <app>
[version]`. With no explicit version it targets the **immediately-prior** release
(v0.1.0).

```
ssh -i ~/.ssh/id_ed25519_int_ikigenba_com ec2-user@int.ikigenba.com \
    'sudo opsctl rollback ledger'
```

- Expected (from `rollback.go`; no DB restore line because v0.1.1 did not advance
  the schema):
  ```
  >> atomic swap current -> releases/v0.1.0
  >> restart ledger
  >> rolled back ledger: v0.1.1 -> v0.1.0
  ```
- **Abort/restore:** rollback is itself the recovery primitive. If it fails
  `is-active` on the target, you can `sudo opsctl rollback ledger v0.1.1` to repoint
  forward to the known-good v0.1.1 (explicit target form), then investigate. The
  data DB is untouched here (no schema advance → no restore).

### 5d. Re-verify (v1 again)

```
readlink /opt/ledger/current         # releases/v0.1.0
systemctl is-active ledger           # active
/opt/ledger/current/ledger version   # v0.1.0 (<sha>)  — the self-report matches v1 again
curl -s -o /dev/null -w '%{http_code}\n' \
     http://127.0.0.1:3002/.well-known/oauth-protected-resource   # 200
```
- Expected as annotated. This is the core proof: `current` repointed atomically,
  the unit came back up, and the binary self-reports the rolled-back-to version.

### 5e. Prune check

The surface (`cmdPrune`): `opsctl prune <app> [--keep N]`, default keep **3**
(`DefaultKeep`). The kept set is the **newest N** releases, *plus* — regardless of
N — `current`'s target and `current`'s immediate predecessor (the live rollback
target). With only two releases (`v0.1.0`, `v0.1.1`) and default keep=3, prune
deletes nothing — confirming it never over-prunes.

Note the post-rollback geometry here: after §5c, `current` → **v0.1.0** and the
rolled-back-FROM **v0.1.1** is the *newest* release. So even `--keep 1` is a no-op
on this box: the keep window's newest-1 is v0.1.1, and `current` (v0.1.0) is
separately protected — both releases are retained. There is **no** non-protected
release to drop in this state; observing a real deletion would need a third,
older, non-current/non-predecessor release. (Real deletion behaviour is covered
directly by opsctl's prune unit tests; this step only re-confirms the
never-over-prune property on the box.)

```
ssh … 'sudo opsctl prune ledger'                 # keep=3 default: no-op, prints nothing
ssh … 'ls /opt/ledger/releases'                  # still v0.1.0 and v0.1.1
```
- Expected: with default keep, `prune` prints no `>> prune release …` lines and
  both releases remain. (Install already runs prune at its tail, so this standalone
  call mostly confirms the kept count.)
- Optional, to confirm the never-over-prune property explicitly:
  ```
  ssh … 'sudo opsctl prune ledger --keep 1'
  ssh … 'ls /opt/ledger/releases'
  ```
  Expected: still a **no-op** — `prune` prints no `>> prune release …` lines and
  `releases/` still holds **both** v0.1.0 and v0.1.1. With `current` → v0.1.0,
  v0.1.1 is the newest-1 (kept by the window) and v0.1.0 is current (kept
  unconditionally), so neither is eligible to drop.
  - **Abort/restore:** prune deletes *non-current, non-predecessor* release dirs
    and their backups; it never touches `current`, the rollback target, or the
    data DB. Rollback to a kept prior release stays on-box (`opsctl rollback`), so
    you never need to rebuild a past version off-box. If you pruned a release you
    wanted back, it is still rebuildable from git: `bin/deploy` builds current
    `main`, so land the desired `<app>/VERSION` on `main` (e.g. `bin/bump`) and
    redeploy — the box state is recoverable from the committed history + a
    redeploy.

---

## 6. Abort / restore — consolidated

| Step | Mutates | Recovery |
|---|---|---|
| 1b install opsctl | `/usr/local/bin/opsctl` | `sudo rm -f /usr/local/bin/opsctl` (inert until invoked) |
| 2b `opsctl setup ledger` | user, `/opt/ledger` tree, unit, nginx fragment | disable unit + `rm` unit/fragment + `daemon-reload` + `nginx -t`/reload + `rm -rf /opt/ledger` (only pre-DB) — see §2b box block |
| 3b `bin/deploy` (v0.1.0, first install) | `releases/v0.1.0`, `current`, manifest, **creates** data DB on migrate | no prior release to roll back to; fix + re-deploy, or §2b teardown (DB is brand-new) |
| 5a `bin/bump` + `bin/deploy` (v0.1.1) | `ledger/VERSION` 0.1.1 committed to `main`; box: `releases/v0.1.1`, repoints `current` | `sudo opsctl rollback ledger` → back to v0.1.0 |
| 5c `opsctl rollback` | repoints `current` (+ DB restore only if FROM-release advanced schema) | `sudo opsctl rollback ledger v0.1.1` to repoint forward to known-good |
| 5e `opsctl prune` | deletes old release dirs + their backups | re-`bin/deploy ledger` (rebuildable from the committed `main` history) |

**Confirm the box is back to a serving state at the end** (regardless of where you
stopped):

```
systemctl is-active ledger nginx dashboard      # all 'active'
readlink /opt/ledger/current                     # points at the intended release
/opt/ledger/current/ledger version               # matches that release
curl -s -o /dev/null -w '%{http_code}\n' \
     https://int.ikigenba.com/srv/ledger/.well-known/oauth-protected-resource   # 200
```

The data DB at `/opt/ledger/data/ledger.db` is **never** deleted or overwritten by
any deploy/rollback/prune step (§2.7); only the explicit pre-migration backup +
matching restore touch it, and only on a schema-advancing install/rollback.

---

## Cleanup (optional, after a successful drill)

Leave `ledger` deployed (it is now a real service on the box). The drill's version
bumps (`ledger/VERSION` 0.1.0 → 0.1.1) are committed on `main` and stay there — the
version ledger is meant to be append-only. The throwaway `bin/deploy` worktree and
`/tmp` artifacts are removed automatically (the wrapper's `trap cleanup EXIT`); the
release dirs on the box are governed by prune.

---

## GAPs found (feed F1 / follow-ups)

- **Version stamp lacks `-dirty`.** `bin/deploy` and appkit's `versionString()`
  emit `<version> (<sha>)` only; the `-dirty` suffix the verify steps reference is
  **not yet** produced (it is F1's job — `bin/deploy` builds from a clean detached
  worktree so the build is never dirty anyway). The version self-report
  checks in §4e/§5d assert the leading `vX.Y.Z` token only, which is what
  `opsctl` preflight also keys on — so this gap does not block the loop.
- **No `opsctl status`/`current`-reporting verb.** There is no
  `opsctl current ledger` or `opsctl status` verb in the CLI; this runbook reads
  on-box state via `readlink /opt/ledger/current`, `ls /opt/ledger/releases`, and
  the binary's own `version` self-report instead. A reporting verb would make
  verification one command — candidate for a follow-up (and for the future
  operations-MCP in §1.3).
- **No `opsctl teardown` verb.** Undoing `setup` is done by hand (the §2b box
  block). A `teardown <app>` verb (reverse of `setup`, refusing if a data DB
  exists) would make the abort path safer — follow-up, not a blocker for D2.
- **Schema-advance rollback path is unexercised by the minimal drill.** The
  backup-on-advance → restore-on-rollback code paths exist (`install.go` step 4 /
  `rollback.go` step 2) but the minimal v0.1.1 = same-commit drill does not trip
  them (`applied == embedded`). The optional extended drill in §5a notes how to
  exercise them once a real new migration lands; until then this is verified only
  by opsctl's temp-root unit tests, not on the box.
