# dropbox

The **dropbox** service for the metaspot single-tenant suite. A loopback-only
**daemon + event-plane producer** (not an API wrapper) with **no UI** and **no
token logic of its own**, deployed at `<account>.metaspot.org/srv/dropbox/`
(e.g. `ai.metaspot.org/srv/dropbox/`). First demo account: **ai**.

It keeps a **private local mirror in sync** with a single Dropbox **app folder**
(one-way, **download-only**) and **emits a file-lifecycle event** for every
change. One Dropbox account for the whole box (app `ikigai-onebox`, App-folder
scoped → `/Apps/ikigai-onebox/`); the refresh token is a **suite-level secret**.
Files originate *in* Dropbox; the box never writes back — no conflict resolution,
no echo loop. It is an event-plane **producer** (emits `file.created` /
`file.modified` / `file.deleted` to an outbox at `GET /feed`, mirroring
`../crm`/`../ledger`), cloned from the **ledger producer chassis** (server, mcp
transport, db + migration runner, `eventplane/outbox`, logging, ids — renamed).

**Read the decisions first — do not re-derive them:**

- `../../metaspot/AGENTS.md` — platform spec (Service layer = path routing).
  *(From a service dir the path is `../../metaspot`; the root `CLAUDE.md`'s
  `../metaspot` is relative to the ikigai root, not here.)*
- `../../metaspot/docs/path-routing-architecture.md` — server-side topology + the
  auth contract you live under.
- `../../metaspot/docs/connector-and-install.md` — the suite plugin + install
  layer. A service's connector skills live in the `dashboard`'s `plugin/`, not
  here.
- `PLAN.md` — the full dropbox design (the sync engine, crash/replay ordering,
  case-folding, the content endpoint, the events, the no-backup decision).
- `../crm` / `../ledger` — the sibling producers that share this chassis
  (`internal/<domain>` → `/feed` outbox).

If anything here conflicts with those docs, the docs win — and flag the conflict.

## What this app is

A loopback-only domain service on **port 3005**, mounted at **`/srv/dropbox/`**.
nginx (owned by the dashboard) terminates TLS, introspects every `/srv/dropbox/`
request via `auth_request` against the dashboard, and injects `X-Owner-Email` /
`X-Client-Id`; this service **trusts those headers** and does no token validation
of its own. nginx strips the `/srv/dropbox/` prefix, so internal routes stay bare
(`/mcp`, `/whoami`, `/feed`, `/content`, `/.well-known/...`).

**Single box, single account.** One Dropbox app folder, one owner; no
owner/tenant column. `Identity` (the injected headers) is consulted only by
`dropbox_whoami` / `dropbox_health`. Per-user OAuth is explicitly out of scope —
a folder-sync daemon has one folder.

## The daemon + producer model

A single **background sync goroutine** (wired in `cmd/dropbox/main.go`, started in
`main`, clean shutdown on `ctx` cancel) drives the whole thing. The loop is
**longpoll-driven** — zero inbound surface, idle ≈ zero CPU (a parked socket
read, not a busy poll):

```
bootstrap → longpoll(cursor) → continue(cursor) → apply each delta → advance cursor
```

- **Bootstrap:** first boot (no stored cursor) enumerates the whole app folder via
  `list_folder` (`recursive:true`) and emits `file.created` for every existing
  file (no silent baseline). `ikigai-onebox` starts empty in v1, so this is moot
  today, but a pre-populated folder would emit a `created` burst.
- **Steady state:** `list_folder/longpoll` blocks ~480s (Dropbox adds up to ~90s
  jitter → can park ~570s); on `changes:true`, drain `list_folder/continue` while
  `has_more`, apply each page's entries, persist the cursor.
- **The atomic seam:** the event row is appended on the **same SQLite
  transaction** as the `files` index change, and `Ring()` fires after commit — so
  an event is emitted **iff** the mirror state actually changed (same outbox
  discipline as ledger).
- **`GET /feed`** — the outbox SSE handler (unauthenticated, loopback-only, the
  perimeter is nginx). Producer-only.
- **Bytes served over loopback `GET /content`** — the mirror stays **private**
  (`0750`, a subdir of `data/`); consumers fetch current bytes here, never the
  raw files. **Never public** — guarded by the handler's identity-header check
  (see below), not by nginx.

## The MCP surface (2 tools — it's a daemon)

The service side is read-only; there are no write verbs. MCP is thin and exists
for the auth proof + the dashboard inventory (`MCP=true`).

- **`dropbox_whoami`** — `()` identity probe (owner email + client id); the
  end-to-end auth proof. Kept, slated to be superseded by `dropbox_health` later.
- **`dropbox_health`** — `()` identity (same fields as whoami, so the eventual
  migration is additive) **plus** disk telemetry: `mirror_bytes` (`SUM(size)` over
  the index — indexed logical size, no directory walk), `disk_free_bytes` /
  `disk_total_bytes` (a `statfs` on the mirror path), and `failed_files` (count of
  index rows with a non-null `error` — the poison entries the engine advanced
  past).

## Package layout

Same chassis layering as ledger/crm (one file per concern within one package;
`internal/mcp/tools.go` is the sole MCP dispatcher).

- **`cmd/dropbox/main.go`** — reads the three secrets + non-secret paths/knobs
  from env, builds the `tokenSource`, opens the db, wires the `outbox` producer,
  starts the sync goroutine, mounts `/content`.
- **`internal/dropbox/`** — the domain package:
  - `types.go` — shared structs (`FileMeta`, delta entries, `HealthInfo`), error
    sentinels.
  - `client.go` — the **only** HTTP-to-Dropbox site: `tokenSource` (refresh→access
    cache, refresh on expiry/401), `list_folder` / `longpoll` / `continue` /
    `download` (+ block-SHA256 verify). Sends the bearer on rpc/content hosts,
    **omits it on longpoll**; the longpoll path uses a ≥600s-timeout client.
  - `store.go` — SQL-only (`*sql.Tx` methods): `sync_state` cursor get/set; `files`
    upsert/get (folded `path_lower` lookup)/delete; subtree-delete by prefix;
    `SUM(size)`; mark-`error`.
  - `mirror.go` — the private local mirror: atomic write (temp+rename), delete,
    case-only rename, mkdir, path confinement, `statfs`. No db, no HTTP.
  - `sync.go` — the engine: longpoll→continue→apply loop, the
    created/modified/deleted decision, per-page cursor advance, poison-entry bound.
  - `service.go` — the `Service` type: holds store+mirror+client+`EventSink`; owns
    the tx that commits `{files change + outbox event}`; exposes `Content(path,
    rev?)` and `Health()`.
  - `events.go` — the three payload builders + the `EventSink`/`outboxProducer`
    seam (lets the engine run with emission disabled in unit tests, like ledger).
  - `content.go` — the loopback `GET /content` handler.
  - `health.go` — `HealthInfo` assembly.
- **`internal/mcp`** — JSON-RPC 2.0 transport. `tools.go` holds the 2 descriptors,
  dispatches into `dropbox.Service` (`Whoami`/`Health`), translates sentinels to
  tool-error text. `mcp.go` is the transport, unchanged.
- **`internal/server`** — routing, the RFC 9728 protected-resource metadata
  document, `requireIdentityHeaders`, `/whoami`, the unauthenticated `GET /feed`
  and `GET /content` routes, security headers, graceful shutdown.
- **`internal/db`** — SQLite open (WAL, FK, single-writer) + embedded migration
  runner. Migrations: `001_schema_migrations` (chassis, byte-identical),
  `002_dropbox.sql` (`sync_state` single-row cursor table; `files` per-path index
  with `path_lower` + nullable `error`), `003_outbox.sql` (byte-identical to
  `outbox.SchemaSQL`, with the equality test).
- **`internal/logging`, `internal/ids`** — structured slog + request-id, ULID,
  carried unchanged.
- **`eventplane`** — the shared outbox library, consumed via a committed
  `replace eventplane => ../eventplane` (so the build needs the in-repo
  `eventplane/` tree but no network/`go.work`).

## Events (event-plane producer)

Producer-only. Payload is a **reference, never bytes** (consumers fetch via
`/content`): `event`, literal `path`, `rev`, `content_hash`, `size`, a
**URL-encoded** `content_url` (`http://127.0.0.1:3005/content?path=…`), and
`occurred_at`.

- **`file.created`** — a path not previously in the index now exists.
- **`file.modified`** — a known path's `rev` changed (includes a case-only
  rename).
- **`file.deleted`** — a known path is gone; **one event per indexed file
  removed**, including every file beneath a deleted folder. The payload carries the
  file's **last-known** `rev`/`content_hash`/`size`, read from the index row before
  the in-tx delete removes it.

`content_url` resolves to *current* bytes (the mirror holds current state only, no
historical revisions); a consumer that fetches and finds `content_hash` differs
knows a newer event is in flight. The optional `rev`→**409** contract on `/content`
lets a consumer demand "the exact bytes this event referenced, or a clear
moved-on signal."

## Load-bearing correctness rules (don't break these)

These are the invariants the engine is built on. A future maintainer who breaks
one silently corrupts the mirror or the event stream:

- **Folder-delete subtree fan-out.** A folder delete arrives from Dropbox as a
  **single** `DeletedMetadata` — it does *not* enumerate descendants.
  `apply(deleted)` selects every indexed file at/under that prefix, unlinks each,
  deletes each row, and emits one `file.deleted` per row, all on the same tx.
  Missing this leaks a whole subtree into the mirror and index.
- **Delete crash ordering.** For a delete: read the last-known fields, **commit
  {row delete + event}, THEN unlink** the mirror file. A crash in that window is
  closed on restart because the cursor hasn't advanced → the delta replays; a
  delete of an **already-absent** path performs the idempotent unlink but **emits
  nothing** (no duplicate `file.deleted`, no orphan). This "absent-path delete =
  silent for events, but still unlink" is load-bearing, not a nicety.
- **Per-continue-page cursor advance.** The cursor is persisted after each
  `continue` page's entries are applied — not only after the full `has_more`
  drain — so a crash mid-drain replays only the unapplied tail.
- **Poison-entry bound.** The cursor never advances past an unapplied entry, so a
  permanently-undownloadable entry would wedge *all* sync. After
  `DROPBOX_MAX_ENTRY_RETRIES` (default **5**) failed passes on the same entry, mark
  the index row `error`, advance past it, and surface it in `failed_files` — the
  failure becomes visible rather than silent.
- **Download hash-verify.** Before the atomic rename, the temp file's Dropbox
  **block-SHA256 is recomputed and compared** to the metadata `content_hash`; a
  mismatch is treated as a download failure and retried. `content_hash` is a
  verified integrity check, not a stored decoration.
- **Case folding.** Dropbox is case-insensitive + case-preserving; the ext4 mirror
  is case-sensitive. The index stores the **display** path and matches on
  `path_lower`; a case-only rename is an **on-disk rename + `file.modified`**,
  never a second copy. `/content` resolves its query through the index, so disk,
  index, and `content_url` never diverge on case.
- **Longpoll is UNAUTHENTICATED** and needs a **≥600s** client (480 + ~90s
  jitter). `client.go` MUST omit the bearer on `longpoll` and send it on
  `list_folder`/`continue`/`download`.
- **`/content` is private.** The handler returns **404** whenever it sees an
  nginx-injected identity header (`X-Owner-Email` **or** `X-Forwarded-Proto`) —
  the same handler-level guard `/feed` uses. The decoded path is cleaned and
  confined under the mirror root (`..`/escape → 400).

## nginx fragment (not a vhost)

`opsctl setup dropbox` writes only `/etc/nginx/conf.d/locations/dropbox.conf` (its
`location /srv/dropbox/` + the PRM well-known location) and reloads nginx — it
installs no server block and issues no TLS cert (the dashboard owns both, via
`opsctl init-box`). An
exact-match `location = /srv/dropbox/content { return 404; }` is added as
**optional defence-in-depth** — belt to the handler's braces — not the mechanism.
A dev mirror of the fragment is **generated, not committed**: `nginx/run`
regenerates `nginx/locations/dropbox.conf` from `etc/nginx.conf` on every run,
iterating a hardcoded service loop that **must include `dropbox`** or it never
appears on the dev front door (:8080).

## Manifest / deploy

dropbox is one static appkit binary (the `appkit.Main(appkit.Spec{…})` contract,
producer + `Workers` sync engine): `<app>` serve + the fixed `version`/`manifest`/
`migrate`/`schema`/`backup`/`restore` verbs, no `run` wrapper, no bundled
`registry`. `etc/manifest.env` (`APP=dropbox`, `MOUNT=/srv/dropbox/`,
`DEFAULT=false`, `PORT=3005`, `MCP=true` so the dashboard inventory lists it;
producer, so it also round-trips `FEED=/feed` + the `OUTBOX_RETENTION_*` config)
is emitted by `dropbox manifest` and regenerated on the box by `opsctl deploy` on
every swap. Shipping is the shared repo-root `bin/ship dropbox` (no version arg;
version is the committed `dropbox/VERSION`, advanced by `bin/bump dropbox <field>`)
→ `opsctl stage` + `opsctl deploy`; provisioning is `opsctl setup dropbox` (the `dropbox`
`--system` user + `/opt/dropbox` tree — data `0750`, **mirror is a private subdir
of data** — the enabled systemd unit, the nginx fragment).

- **Secrets — dropbox HAS them (unlike ledger).** Three values reach the process
  env only: `DROPBOX_APP_KEY`, `DROPBOX_APP_SECRET`, `DROPBOX_REFRESH_TOKEN`. On
  the box they live in SSM `/metaspot/<account>/app-config` under the `dropbox`
  key, injected by `metaspot-launch`. `bin/secrets` (operator-side, no opsctl verb)
  does a non-destructive read-modify-write of **only** the `dropbox` key, pulling
  values from `~/.secrets/DROPBOX_*` (masked summary, never printed; siblings
  preserved). In dev, `.envrc` exports them from `~/.secrets/DROPBOX_*`.
- **Non-secret config** (resolved in-binary at the composition root):
  `DROPBOX_MIRROR_PATH` (box `/opt/dropbox/data/mirror`, dev `./tmp/mirror`),
  `DROPBOX_DB_PATH`, `DROPBOX_GENERATION_PATH`, `RESOURCE_ID`/`AUTH_SERVER` (now
  composed in-binary from `METASPOT_DOMAIN`+`MOUNT`, not a wrapper),
  `DROPBOX_LONGPOLL_TIMEOUT` (default 480), `DROPBOX_MAX_ENTRY_RETRIES` (default 5),
  `DROPBOX_APP_FOLDER_ROOT` (default `""` = app folder root). Plus the shared
  `OUTBOX_RETENTION_DAYS` / `OUTBOX_RETENTION_MAX_ROWS`.
- **Dashboard registration — no code edit, derived from the manifest.** The
  dashboard derives its `/srv/<svc>/mcp` resource set at startup from each deployed
  service's `etc/manifest.env` (`MCP=true`), via `DASHBOARD_MANIFEST_ROOT=/opt`. So
  registering dropbox is just deploying it (which lands `/opt/dropbox/etc/
  manifest.env`) → **restart the dashboard** to re-read the manifests. There is no
  hardcoded resource list to edit. Until that restart, the `/srv/dropbox/mcp` 401
  challenge omits `resource_metadata`.

## The no-backup decision (intentional)

dropbox declares no custom backup/restore hooks — by design. The mirror is a
download-only replica and the SQLite state (cursor +
`files` index) is **fully reconstructible from Dropbox, the source of truth**.
Recovery on data loss is not a snapshot restore but: **wipe the DB + mirror,
restart, re-bootstrap** — which re-enumerates the app folder and re-emits
`file.created` for every file. The generation sidecar lives outside the DB, so a
wipe mints a fresh epoch and consumers resync cleanly.
