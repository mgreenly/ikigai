# Deploy Phase-1 wiki to the `ai` box — operator runbook (Task 5.4)

Ship `wiki` to `int.ikigenba.com` as a path-routed MCP service at `/srv/wiki/`
(loopback port **3006**), in the ikigenba `bin/*` order. This runbook is the
exact command sequence for a human operator to run **after** an interactive SSO
login. Nothing here was executed during preparation — the scaffolding was
audited read-only and the offline build was verified; the actual deploy is
blocked on the human SSO step.

> All commands assume your shell is at the repo root
> `/mnt/projects/ikigai/add_wiki` unless a `cd` is shown.

---

## Prereqs

1. **Interactive SSO login (HUMAN STEP — only a person can do this).** The
   token expires; `bin/secrets` and every AWS/SSM call below need a live session:

   ```
   aws sso login --profile int
   ```

   Verify (non-interactive, does NOT log in):

   ```
   aws sts get-caller-identity --profile int
   ```

   A JSON identity block ⇒ session active. An `Error … SSO session … expired`
   ⇒ run `aws sso login --profile int` again.

2. **Operator shell direnv (local).** wiki's committed `.envrc` injects the
   ingest config / `ANTHROPIC_API_KEY` for any *local* run. One-time per shell:

   ```
   direnv allow wiki/
   ```

   (Not needed for the box deploy itself — the box gets the key from SSM — but
   keeps your workstation shell consistent.)

3. **`~/.secrets/ANTHROPIC_API_KEY` present.** `bin/secrets` reads the value
   from this file (never printed). Confirm presence only — do **not** cat it:

   ```
   test -r ~/.secrets/ANTHROPIC_API_KEY && echo present || echo MISSING
   ```

4. **`WIKI_OWNER` supplied (Phase 6.1 — see the section below).** Required as of
   Phase 6.1 so `bin/secrets` can seed the event-plane consumer's owner. Supply it
   either as `~/.secrets/WIKI_OWNER` or inline (`WIKI_OWNER=<email> ./bin/secrets`).

---

## Steps (run in this exact order)

### 1. Seed wiki's secret into SSM app-config — `bin/secrets`

After SSO. Non-destructive read-modify-write of **only** wiki's key in the
shared SecureString `/ikigenba/ai/app-config`; every other app's key is
preserved. Seeds `ANTHROPIC_API_KEY` from `~/.secrets/ANTHROPIC_API_KEY`.

```
cd wiki && ./bin/secrets
```

Expect: a summary showing `profile/region : ai / us-east-2`, a **masked**
`ANTHROPIC_API_KEY` (`xxxx…xxxx`), and the preserved sibling keys (crm, notify,
dashboard, …). It prompts `Type "yes" to write:` — type `yes`. On success:
`>> .wiki written to /ikigenba/ai/app-config`. (Must run **before** first start:
`ikigenba-launch` hard-fails if the key is missing at boot.)

### 2. One-time box provision — `bin/setup`

Creates the `--system` `wiki` user + `/opt/wiki/{bin,etc,data}` tree, writes &
**enables (not starts)** the `wiki.service` systemd unit
(`ExecStart=/usr/local/bin/ikigenba-launch wiki`), drops the nginx fragment to
`/etc/nginx/conf.d/locations/wiki.conf`, runs `nginx -t`, reloads nginx.

```
./bin/setup
```

Expect: `nginx: active` and `wiki.service: enabled / inactive` (inactive is
correct — setup does not start it). Requires the dashboard's apex setup
(server block, cert, `/_authn`, `conf.d/locations/` dir) to already exist —
it does (dashboard is live on the box).

### 3. Build, ship, and start — `bin/deploy`

Builds off-box (deterministic, no network/go.work needed), then
`systemctl stop wiki` → rsync `build/wiki`→`/opt/wiki/bin/run`,
`build/registry`→`/opt/wiki/bin/registry`,
`build/wiki.bin`→`/opt/wiki/bin/wiki.bin`,
`etc/manifest.env`→`/opt/wiki/etc/manifest.env` → `chown` → `systemctl start
wiki` → `is-active`. Never touches `/opt/wiki/data/wiki.db` — the DB is created
on first start and **migrations run on start**.

```
./bin/deploy
```

Expect: each `>> rsync …` line, then `active`, then `>> deploy complete.`

### 4. Restart the dashboard so its inventory picks up wiki

The dashboard derives its AS resource list from the per-service manifests under
`/opt` at startup (`DASHBOARD_MANIFEST_ROOT=/opt`, `inventory.Read`). wiki's
`manifest.env` carries `MCP=true`, so a dashboard **restart** registers
`https://int.ikigenba.com/srv/wiki/mcp` as a known resource — **no
`DASHBOARD_RESOURCES` edit needed** (that env var no longer exists; the CLAUDE.md
"Registering a new MCP service" note is stale on this point).

```
cd ../dashboard && ./bin/deploy
```

(Or, if you don't want to re-ship the dashboard artifact, a bare restart on the
box: `ssh -i ~/.ssh/id_ed25519_int_ikigenba_com ec2-user@int.ikigenba.com "sudo
systemctl restart dashboard"`.) Either way the restart **briefly drops
`/internal/authn` box-wide for a few seconds** — expected; every service's
`auth_request` is unavailable during the restart.

### 5. Verify on the box

(Per CLAUDE.md "Verify on the box". `<key>` = `~/.ssh/id_ed25519_int_ikigenba_com`,
`<box>` = `ec2-user@int.ikigenba.com`.)

```
# a. service up
ssh -i <key> <box> "systemctl is-active wiki"            # → active

# b. clean boot + migration lines (no Warn/Error)
ssh -i <key> <box> "journalctl -u wiki -n 50 --no-pager"
#    look for migration apply lines and the listen line; ANTHROPIC_API_KEY
#    absent would log a Warn and DISABLE ingest — confirm it is NOT warning.

# c. loopback whoami (services trust injected identity headers, so drive directly)
ssh -i <key> <box> \
  "curl -s -H 'X-Owner-Email: mgreenly@gmail.com' -H 'X-Client-Id: smoke' \
        http://127.0.0.1:3006/whoami"
#    → JSON echoing the injected owner/client.

# d. PRM well-known → 200 (public, unauthenticated)
curl -s -o /dev/null -w '%{http_code}\n' \
  https://int.ikigenba.com/srv/wiki/.well-known/oauth-protected-resource   # → 200

# e. the /srv/wiki/mcp 401 challenge MUST carry resource_metadata
curl -s -D - -o /dev/null https://int.ikigenba.com/srv/wiki/mcp | grep -i www-authenticate
#    → WWW-Authenticate: Bearer resource_metadata="…/srv/wiki/mcp/.well-known/oauth-protected-resource"
```

**End-to-end (the real proof):**

1. **Connector OAuth round-trip + `wiki_whoami`.** In a Claude client, add the
   connector URL `https://int.ikigenba.com/srv/wiki/mcp`, authorize through the
   dashboard OAuth AS, then call `wiki_whoami` → returns your owner email +
   client id. (This exercises plugin/connector → dashboard OAuth → wiki.)
2. **`wiki_ingest_text` → `wiki_search` round-trip.** Call `wiki_ingest_text`
   with a short distinctive body (returns a `job_id`; poll `wiki_job_status`
   until the async ingest+integration completes), then `wiki_search` for a term
   from that text → the integrated page is returned. (Confirms the ingest agent
   has its `ANTHROPIC_API_KEY` from SSM and the BM25 index is live.)

---

## Phase 6.1 — consumer enablement (dropbox → wiki event plane)

Phase 6.1 turns on wiki's event-plane consumer: it subscribes to dropbox's
file-lifecycle `/feed` and autonomously ingests files dropped in the hardcoded
`wiki/ingest` folder. The consumer boots **DISABLED** unless **both** the
upstream feed URL and the box owner are present in wiki's environment:

- **`DROPBOX_FEED_URL`** is resolved on the box by the `bin/build` wrapper via
  `registry feed-url dropbox` (the same by-name mechanism notify uses for crm).
  No operator action — it is baked into `/opt/wiki/bin/run` at build.
- **`WIKI_OWNER`** is supplied per-box through SSM app-config by `bin/secrets`.
  Dropbox is single-owner and its events carry no owner, so the owner is service
  config. **It must EXACTLY match the `X-Owner-Email` the dashboard injects for
  the authenticated user on this box** — otherwise autonomously-ingested dropbox
  content would land under a different owner than the one the user's MCP session
  reads. Use the correct per-box owner email; do **not** guess.

### Before `bin/secrets` (Step 1): supply `WIKI_OWNER`

Provide the value either way (it is an email, not a secret, but is still kept
per-box and never committed — `bin/secrets` resolves it from a file or env, like
`ANTHROPIC_API_KEY`):

```
# option A — drop it in ~/.secrets (parallel to ANTHROPIC_API_KEY)
printf '%s' '<owner-email>' > ~/.secrets/WIKI_OWNER && ./bin/secrets

# option B — inline for this one invocation
WIKI_OWNER='<owner-email>' ./bin/secrets
```

`bin/secrets` now writes **both** `ANTHROPIC_API_KEY` and `WIKI_OWNER` into the
`.wiki` object (siblings preserved), shows `WIKI_OWNER` in the summary (not
masked — it is an email), and **fails loudly** if `WIKI_OWNER` resolves empty.
Then continue with Steps 2–4 unchanged (`bin/setup`, `bin/deploy`, dashboard
restart).

### Verify the consumer is enabled (extends Step 5b)

```
ssh -i <key> <box> "journalctl -u wiki -n 50 --no-pager"
```

Look for the `starting wiki` line showing `consumer_enabled=true` and
`consumer_owner=<email>`, and confirm there is **no**
`event-plane consumer DISABLED: no WIKI_OWNER` Warn (nor the `no DROPBOX_FEED_URL`
/ ingest-off Warns). End-to-end: drop a file into the box's `wiki/ingest` dropbox
folder and confirm wiki ingests it (it appears via `wiki_search` under the
configured owner).

---

## Findings (audit results — read-only prep, 2026-06-04)

All wiki deploy scaffolding is **correct and wiki-specific**. No ledger / clone
leftovers were found in any file — **nothing needed fixing**.

| file | verdict |
|---|---|
| `etc/deploy.env` | Correct. `ACCOUNT=int`, `SSH_USER=ec2-user`, `SSH_KEY=~/.ssh/id_ed25519_int_ikigenba_com`; `HOST` defaults to `${ACCOUNT}.ikigenba.com` in each script. Identical to notify's. |
| `etc/manifest.env` | Correct. `APP=wiki`, `MOUNT=/srv/wiki/`, `DEFAULT=false`, `PORT=3006`, `MCP=true`, plus non-secret ingest config (`WIKI_INGEST_MODEL`, `WIKI_INGEST_MAX_TOKENS`). No ledger port (3002) / mount. |
| `etc/nginx.conf` | Correct. Two wiki location blocks (open PRM exact-match + gated `/srv/wiki/` prefix), `__PORT__` templated, 429-faithful `@wiki_authn_500` error path. Structurally identical to notify's, all `wiki`-named. |
| `bin/secrets` | Correct. Non-destructive read-modify-write of only `.wiki` in `/ikigenba/ai/app-config` under `--profile int --region us-east-2`; seeds the single `ANTHROPIC_API_KEY` from `~/.secrets/` (resolved, masked, never printed); siblings preserved; mirrors notify's structure. |
| `bin/setup` | Correct. `--system` user + `/opt/wiki` tree; writes & **enables (not starts)** `wiki.service` with `ExecStart=/usr/local/bin/ikigenba-launch wiki`; drops fragment to `/etc/nginx/conf.d/locations/wiki.conf`; `nginx -t`; reload. No ledger leftovers. |
| `bin/deploy` | Correct. stop → rsync wrapper/registry/`wiki.bin`/`manifest.env` → chown → start → `is-active`. **Never touches `/opt/wiki/data/wiki.db`** (migrations run on start). |
| `bin/build` | Correct & deterministic offline. |

**Offline build (verified):** `(cd wiki && GOPROXY=off ./bin/build)` produced
`wiki/build/wiki` + `wiki/build/wiki.bin` + `wiki/build/registry` with **no
network**. `file build/wiki.bin` → `ELF 64-bit LSB executable, x86-64,
statically linked, stripped`; `ldd` → `not a dynamic executable`. The build is
go.work-independent — `wiki/go.mod` carries committed `replace eventplane =>
../eventplane` and `replace agentkit => ../agentkit`.

**SSO status (at prep time):** `aws sts get-caller-identity --profile int`
returned a valid identity — a session was **active** during preparation. SSO
tokens expire, so the operator should re-verify (and `aws sso login --profile int`
if expired) immediately before Step 1.

**Dashboard inventory (confirmed, no env edit):** `dashboard/internal/inventory`
globs `/opt/*/etc/manifest.env` and includes every service with `MCP=true`;
`dashboard/bin/build` sets `DASHBOARD_MANIFEST_ROOT=/opt` and derives the AS
resource list at startup. No `DASHBOARD_RESOURCES` variable exists anymore. wiki
(`MCP=true`) is therefore picked up on a plain dashboard restart — Step 4 is a
restart, not an edit.

---

## Status

**Phase-1 wiki is build-ready; deploy is BLOCKED on interactive `aws sso login
--profile int` (human step). Run the steps above to complete.**
