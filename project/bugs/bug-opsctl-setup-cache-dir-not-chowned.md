# Bug — `opsctl setup` leaves `/opt/<app>/cache` root-owned, so a brand-new service crash-loops on first deploy

**Status: informational, non-contractual.** Records an observed bug and the code
path behind it, for whoever authors the fix (opsctl design + plan). Nothing
downstream consumes this doc. Rewrite in place as understanding evolves. Filed at
the repo root because it is a **suite-level provisioning bug in `opsctl`** that
bites the bring-up of *any* new service, not one service's domain logic. The fix
lives in `opsctl/` (`opsctl/internal/opsctl/setup.go`, and see §6 re `deploy.go`).

---

## 1. Symptom (reproduced live during the github v0.1.0 deploy)

A brand-new service (`github`) was set up and deployed to `int.ikigenba.com` with
the normal flow:

```
opsctl setup github            # no --fragment (D01 symlink mode)
bin/ship github                # v0.1.0+ed66f81
opsctl stage  github v0.1.0+ed66f81 --artifact /tmp/github-v0.1.0+ed66f81.tar.gz
opsctl deploy github v0.1.0+ed66f81
```

`stage`/`deploy` all reported success, but the unit never came up — it
crash-looped and hit `start-limit`:

```
systemctl is-active github            → failed
opsctl status                         → github  v0.1.0+ed66f81  failed
curl http://127.0.0.1:3203/health     → HTTP 000 (no listener)
```

`journalctl -u github` showed the same line on every restart, then the
start-limit trip:

```
ikigenba-launch[…]: github: generation sidecar: open /opt/github/cache/github.db.generation: permission denied
github.service: Main process exited, code=exited, status=1/FAILURE
…
github.service: Start request repeated too quickly.
```

The tree on the box confirmed the cause — `state/` was handed to the service
user but `cache/` was left `root:root`:

```
/opt/github/state    drwx--x--x  github github     ← deploy chowned this
/opt/github/cache    drwxr-xr-x  root   root        ← never chowned  ✗
```

Every already-working service has `cache/` owned by its own user, e.g.

```
/opt/gmail/cache             drwxr-xr-x  gmail gmail
/opt/gmail/cache/gmail.db.generation  -rw-r-----  gmail gmail
/opt/cron/cache/cron.db.generation    -rw-r-----  cron  cron
```

(those `cache/` dirs are dated Jun 30, i.e. they were chowned by an earlier
provisioning; the current `opsctl` that set `github` up on Jul 4 did not chown
it). So the working services mask the bug; a genuinely fresh `setup` exposes it.

**Immediate workaround applied to unblock the deploy** (do not rely on this as
the fix — it just patches the one box):

```
sudo chown github:github /opt/github/cache
sudo systemctl reset-failed github
sudo systemctl start github          → active, /health = 200 {"github_auth":"ok"}
```

## 2. Expected behavior

`opsctl setup <app>` provisions the whole `/opt/<app>` tree so the service — which
runs as the unprivileged `<app>` user (systemd `User=<app>`) — can write
everything it owns at runtime **without any manual chown**. The service writes
its generation sidecar under `cache/`, so `cache/` must be owned by (or at least
writable by) `<app>`, exactly as `state/` already is. A brand-new service should
deploy and come up `active` on the first try.

## 3. Root cause

`opsctl setup` creates `cache/` as part of the tree but never chowns it to the
service user; only `state/` (and the sites `www/` tree) get handed over.

- The generation-sidecar path is `$IKIGENBA_ROOT/<app>/cache/<app>.db.generation`
  — `appkit/config/config.go:104-105` (`composeDataPaths`, active whenever
  `IKIGENBA_ROOT` is set, which it is on the box: `/etc/ikigenba/env` →
  `IKIGENBA_ROOT=/opt`).
- Every service writes it on serve, **unconditionally and before the producer
  branch**: `appkit/verbs.go:178` calls `outbox.EnsureGeneration(cfg.GenerationPath)`
  and wraps failures as `generation sidecar: %w`. This is *not* producer-gated, so
  it hits even a non-producer connector like `github`.
- `opsctl setup` (`opsctl/internal/opsctl/setup.go`):
  - creates the tree **including `l.CacheDir()`** — passed into the mkdir set at
    `setup.go:110` (path-routed), `:126` (worker), `:152` (sites); the dirs are
    made `0o755` **as root** (`opsctl` runs under `sudo`) via the helper at
    `setup.go:248` / `mkdirAllMode` at `:255`.
  - then chowns **only** `state/` and the DB (and, for sites, `www/`):
    `ChownTree(app, app, l.StateDir())` at `setup.go:139`,
    `ChownTree(app, app, l.DBPath())` at `:142`,
    `ChownTree(app, "web", l.WWWDir())` at `:145`.
  - There is **no** `ChownTree(app, app, l.CacheDir())`. So `cache/` stays
    `root:root`, and the `<app>`-user service cannot create the sidecar inside it.

`Layout.CacheDir()` is `/opt/<app>/cache` (`opsctl/internal/opsctl/layout.go:141-142`);
`Layout.GenerationPath()` (`:144-147`) even `MkdirAll`s the cache dir `0o755`, but
that runs in whatever process touches it and does not change ownership.

## 4. Why this is a gap, not a one-off

`opsctl` already knows the pattern — it hands `state/` to `<app>:<app>` in both
`setup` (`setup.go:139`) *and* `deploy` (`ChownTree(app, app, l.StateDir())` at
`opsctl/internal/opsctl/deploy.go:295-297`, with the comment "Hand the whole state
tree back to `<app>:<app>` … else the unit crash-loops"). `cache/` is the same
kind of app-writable runtime directory and needs the same treatment, but it was
simply never added to either the `setup` chown set or the `deploy` re-chown. The
generation sidecar is a chassis-wide feature (every appkit service writes one),
so the omission affects **every** future service bring-up, not just `github`.

## 5. Scope caveat (read before fixing)

Confirm ownership/mode intent against the live `opsctl/project/design/DNN`
Decisions. The working services show `cache/` as `0755 <app>:<app>` with the
sidecar file `0640 <app>:<app>`; matching that is the safe target. `cache/` is
non-secret, non-state (it is regenerable and, unlike `state/`, is not part of the
S3 backup/restore contract), so `0755 <app>:<app>` is reasonable — but if a
Decision deliberately bounds who may traverse `/opt/<app>`, honor it rather than
copying `state/`'s stricter `0711`/`0750` modes blindly.

## 6. Fix direction (sketch — not a committed design)

Smallest correct change, on the existing pattern:

1. **`setup` chowns `cache/`.** In `opsctl/internal/opsctl/setup.go`, alongside the
   existing `ChownTree(app, app, l.StateDir())` (`:139`), add
   `ChownTree(app, app, l.CacheDir())` for the path-routed and worker branches
   (the two that provision a runtime service user). This fixes fresh setups.

2. **`deploy` re-chowns `cache/` too (recommended, self-healing).** In
   `deploy.go`, next to the `state/` re-chown at `:295-297`, add a `cache/`
   re-chown. This makes an *already-broken* box (like any set up by the current
   opsctl) self-heal on the next deploy, without a manual `chown` — mirroring why
   `deploy` re-chowns `state/` even though `setup` already did.

Recommendation: do **both**. (1) is the root fix; (2) is cheap insurance that
recovers existing boxes and is consistent with how `state/` is already handled in
both verbs.

Also update the `setup.go:55` tree-creation comment / any provisioning test
(`opsctl/internal/opsctl/provision_test.go`) to assert `cache/` ends up
`<app>`-owned, so the guarantee is mechanically enforced rather than tribal.

## 7. Verification once fixed

- Fresh service on a clean tree: `opsctl setup <new> && … && opsctl deploy <new>`
  brings the unit up `active` on the first try, **no manual chown**; the journal
  shows a clean `starting <new>` line and `/health` returns `200`.
- On the box, `/opt/<new>/cache` is `<new>:<new>` and
  `/opt/<new>/cache/<new>.db.generation` is created `<new>:<new>`.
- Regression guard (fix (2)): take a box whose `/opt/<app>/cache` is `root:root`,
  run `opsctl deploy <app>`, and confirm `cache/` is chowned back to `<app>:<app>`
  and the unit stays healthy.
- `opsctl` stays green: `GOWORK=off go build ./...` and `GOWORK=off go test ./...`
  from `opsctl/`, including the provisioning test asserting `cache/` ownership.

## 8. Live context (for reproduction/audit)

- Observed deploying `github v0.1.0+ed66f81` to `int.ikigenba.com` on 2026-07-04.
- `github` is a stateless connector (chassis DB, **no** producer/outbox), which
  proves the sidecar write is unconditional — it is not gated on being a producer.
- Box env: `/etc/ikigenba/env` sets `IKIGENBA_ROOT=/opt`, so the composed
  generation path is `/opt/github/cache/github.db.generation`.
