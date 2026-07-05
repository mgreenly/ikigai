# Deploying — bump → ship → stage → deploy

> ⚠️ **`int.ikigenba.com` is the live account.** Do not run any step below, or
> otherwise `ssh int` / invoke `opsctl` against the box, unless you have been
> explicitly told to deploy. The default workflow is local-only (`bin/start`);
> deploying is a separate operator action.

We deploy to **`int.ikigenba.com`** (the first and only account, `int`). Your
`~/.ssh/config` should already have a `Host int.ikigenba.com` entry, usually with
alias `int`, that pins the right key. The commands below assume that alias.

The live box model is:

- `bin/bump` updates the committed service `VERSION` file.
- `bin/ship` builds current `main` and copies a versioned **tar.gz bundle** to
  the box `/tmp`.
- `opsctl stage` unpacks that bundle into versioned on-box slots.
- `opsctl deploy` takes an **unconditional** S3 pre-deploy backup, migrates,
  performs the **three-symlink swap**, reloads nginx through `etc/current`,
  restarts the unit, and prunes old retained versions.

There is no `manifest`-verb / on-box manifest-generation step in the operator
flow. The bundle already carries the authored `manifest.env` and `nginx.conf`
from the source tree.

## One-Time Box Prerequisite

The suite's on-box paths come from `IKIGENBA_ROOT`, and the DEFAULT/apex app's
deploy-time apex render (see step 4) needs the box's apex domain in
`IKIGENBA_DOMAIN`. For the integration account, the Terraform-seeded
`/etc/ikigenba/env` must set:

```sh
IKIGENBA_ROOT=/opt
IKIGENBA_DOMAIN=int.ikigenba.com
```

opsctl reads `/etc/ikigenba/env` automatically at startup, so plain
`sudo opsctl <verb> …` sees these — no shell sourcing needed. A DEFAULT-app
deploy with `IKIGENBA_DOMAIN` missing fails loudly (the apex block needs it for
`server_name` and the cert path), so set it before deploying the apex app.

That file is managed out of repo in `~/projects/metaspot`, so setting it is a
manual operator prerequisite, not something this repo's green gate can verify.

## Deploying opsctl Itself

`opsctl` is the on-box CLI, **not** one of the release-versioned apps: it has no
`VERSION`, no `bin/bump`/`bin/ship` path, and no `stage`/`deploy`. It is a raw
static `linux/amd64` binary hand-installed to `/usr/local/bin/opsctl`. Deploy the
box's opsctl **before** any app deploy that relies on new opsctl behavior — e.g.
a `setup`/`deploy` flag or apex-render the box's current opsctl lacks — since the
box runs whatever opsctl is installed, not the one in your working tree.

```sh
# 1. build static linux/amd64 from opsctl/ (same flags as bin/ship, minus the
#    appkit.version stamp — opsctl carries no version)
cd opsctl && CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off \
  go build -trimpath -buildvcs=false -o /tmp/opsctl ./cmd/opsctl

# 2. copy to the box
scp /tmp/opsctl int:/tmp/opsctl

# 3. install over the live binary (atomic replace; inert until next invocation)
ssh int 'sudo install -m 0755 /tmp/opsctl /usr/local/bin/opsctl'

# 4. verify the expected new surface is present, e.g.
ssh int 'opsctl setup --help 2>&1 | grep -- --default'
```

`install` replaces the file atomically and the binary is inert until invoked, so
there is no downtime. There is no opsctl rollback verb: to revert, rebuild from an
older commit and reinstall (roll-forward only). Build green first —
`GOWORK=off go build ./...` and `GOWORK=off go test ./...` from `opsctl/`.

## Pre-Flight

> 🚨 **Deploy does NOT require workstation AWS SSO.** The `stage`/`deploy` steps
> run **on the box**, where `opsctl` authenticates to AWS through the EC2
> **instance role** — S3 pre-deploy backups and SSM secret reads all happen
> under that role, never your laptop's credentials. A failing
> `aws sts get-caller-identity --profile int` is **not** a blocker for bump →
> ship → stage → deploy. Do **not** stop a deploy to run `aws sso login`.
>
> Workstation AWS creds are needed for **exactly one** optional step: seeding
> secrets from your laptop via `<svc>/bin/secrets` (step 1), which does a
> workstation-side SSM read-modify-write. That's the only place SSO matters, and
> only when an app's secrets are new or changed.

Before starting, confirm the channels a deploy actually needs are live:

- **Box reachable:** `ssh int true`. (This is the real gate — deploy work
  happens over SSH, and opsctl does the AWS work on-box.)
- **SSO (only if seeding secrets in step 1):**
  `aws sts get-caller-identity --profile int`; if it fails, run
  `aws sso login --profile int` interactively. Not needed otherwise.

Run `git status --short` and make sure you are in the standing `main` worktree.
`bin/ship` refuses to run from another branch because it builds the current
`main` checkout.

## 0. Determine What Needs Deploying

A bump-and-ship loop only makes sense for apps whose code changed since what is
live. Use:

```sh
ssh int sudo opsctl status
git diff <box-sha> HEAD -- <app>/
```

`opsctl status` prints the live version and git SHA. Matching versions do not
prove matching code, because `main` can advance without a version bump. If code
changed and the service `VERSION` did not, bump first; staged releases are
immutable and `opsctl stage` rejects a conflicting already-present version.

Skip apps whose only changes are docs or other files that do not alter the
service binary or shipped bundle.

## 1. Seed Secrets When Needed

Secret-bearing apps read their keys from the shared SSM SecureString
`/ikigenba/<account>/app-config` under the app's own object key. Seed before
deploying an app whose secrets are new, changed, or missing:

```sh
<svc>/bin/secrets
```

The script does a non-destructive read-modify-write for that app only and prints
masked summaries. Apps with no `bin/secrets` do not need this step.

## 2. bump → ship → stage → deploy

Run this sequence per app:

1. **`bin/bump <app> <major|minor|patch>`** — advances and commits the
   v-prefixed SemVer in `<app>/VERSION`, then pushes it. Skip only if the
   committed version is already the one you intend to deploy.
2. **`bin/ship <app>`** — builds current `main` as a static linux/amd64 binary,
   bundles the executable with `nginx.conf`, `manifest.env`, and optional
   `share/`, copies `/tmp/<app>-v<ver>+<sha>.tar.gz` to the box, prints the two
   box commands below, and stops.
3. **`ssh int sudo opsctl stage <app> v<ver>+<sha> --artifact /tmp/<app>-v<ver>+<sha>.tar.gz`** —
   unpacks the shipped bundle and stages the versioned binary/config/static
   assets without changing what is live.
4. **`ssh int sudo opsctl deploy <app> v<ver>+<sha>`** — backs up the current
   state to S3 unconditionally, runs migrations against `state/<svc>.db`, swaps
   `bin/run`, `etc/current`, and `share/current` atomically, reloads the nginx fragment through `etc/current`,
   restarts the unit, and prunes retained versions. For the **DEFAULT/apex app**
   (dashboard), deploy also re-renders the apex block from the just-swapped
   `etc/current/nginx.conf` (substituting `IKIGENBA_DOMAIN`; the loopback port is
   already a literal) into `/etc/nginx/conf.d/<app>.conf` and validates it with `nginx -t`
   **before** the reload — so apex routing moves atomically with the binary. A
   normal service has no apex block; its `/srv/<svc>/` fragment re-applies through
   the `etc/current` symlink alone.

Deploy services first and the dashboard last. The dashboard derives its
authorization-server resources from every service manifest when it starts, so
making it last lets its restart observe all newly deployed service manifests.

A brand-new **service** needs `opsctl setup <app>` first (with `--fragment` for a
path-routed service, or no fragment for a worker). A brand-new **DEFAULT/apex
app** (dashboard) uses `opsctl setup <app> --default` instead: it provisions the
tree, user, and unit but writes **no** `conf.d/locations/<app>.conf` — the apex
app's route is the apex block (`conf.d/<app>.conf`), owned by `init-box` (first
render + cert) and re-rendered by every deploy (step 4). A brand-new box needs
`opsctl init-box` once before any service setup.

## 3. Verify

For each app you touched:

```sh
ssh int 'sudo systemctl is-active <app>'
ssh int 'sudo opsctl status'
ssh int 'curl -s -m5 -o /dev/null -w "%{http_code}" http://127.0.0.1:<port>/health'
```

Expected results:

- systemd reports `active`.
- `opsctl status` shows the new version and SHA.
- loopback health returns `200`.
- public nginx checks return `200` for public routes and `401` for protected MCP
  routes; a `502` or `503` means the loopback service is not healthy through the
  proxy.
- producer/consumer services show reconnect or delivery activity in a bounded
  journal read:

```sh
ssh int 'sudo journalctl -u <app> -n 30 --no-pager'
```

## Rollback

Rollback is S3-snapshot based and selected by recency:

```sh
ssh int sudo opsctl rollback <app>
ssh int sudo opsctl rollback <app> -N
```

With no offset, rollback restores the latest S3 snapshot for the app. With
`-N`, rollback restores the Nth most recent snapshot, where `-0` is the latest,
`-1` is the previous snapshot, and so on. The snapshot key records the release
version, so rollback restores that state archive, checks schema compatibility,
then repoints the same three live symlinks.

Explicit target-version rollback is no longer the recovery model. Use S3
snapshot recency instead.

## Inspection And Logs

- **Status:** `ssh int sudo opsctl status`
- **Retained releases:** `ssh int sudo opsctl releases <app>`
- **Bounded logs:** `ssh int 'sudo journalctl -u <app> -n N --no-pager'`

`opsctl tail <app>` follows the stream and does not exit on its own, so reserve it
for interactive sessions.

## Fresh-State Recovery

If an operator intentionally discards a service's live SQLite state before
deploying, the next deploy migrates from an empty DB at `state/<svc>.db`. Stop
the unit first, move the DB and SQLite sidecars out of the service `state/`
directory, then deploy the already-staged version.

This discards data and rollback can only restore snapshots that were already
uploaded to S3. Do it only when explicitly instructed that the live state is not
needed.
