# Dropbox Design Plan — one-way folder mirror that ingests Dropbox files into the event plane

Status: **built** (2026-06-04). All phases 0–7 of §10 are implemented, wired,
and verified (`go build`/`go test` green); `dropbox/CLAUDE.md` is the live
maintainer doc. This document remains the authoritative spec; the build follows
it. Scope is confined to the `dropbox/` folder.

## 1. Goal & guiding decision

The **dropbox** service keeps a **local mirror in sync** with a single Dropbox
**app folder** (one-way, download-only) and **emits a file lifecycle event** for
every change. It is fundamentally a **daemon + event-plane producer**, not an
API wrapper — *a tool to get files into our event system.*

The governing decisions, all confirmed in the design pass:

> **One-way, read-only on our side.** Files originate *in* Dropbox; the box
> never writes back. No conflict resolution, no echo/loop problem, no "which
> side wins." The local mirror is a faithful read-only replica.

- **Single box Dropbox (Model A).** One Dropbox account for the whole box
  (app `ikigai-onebox`, **App-folder** scoped → `/Apps/ikigai-onebox/`). The
  refresh token is a **suite-level secret** (SSM `app-config`), not per-user.
  Per-user OAuth (a callback endpoint + per-owner token table) is explicitly
  **out of scope** — a folder-sync daemon has one folder, and the suite is
  single-tenant ("one box is one owner").
- **Event-plane producer.** The lifecycle events ride the same atomic-outbox
  machinery as `ledger`/`crm` (`GET /feed`). A consumer (e.g. `notify`)
  subscribes.
- **Greenfield clone of the `ledger` producer chassis.** Reuse server, mcp
  transport, db + migration runner, `eventplane/outbox`, logging, ids. Replace
  the domain (`internal/ledger` → `internal/dropbox`) and the tool surface.

The test for any future change: *did this add a tool, a sync direction, or a
public surface?* If yes, justify why it was unavoidable.

## 2. The sync engine (the heart)

A single background goroutine, wired in `cmd/dropbox/main.go` the way `ledger`
wires its outbox + retention loop. The loop is **longpoll-driven** (decided:
zero inbound surface, idle ≈ zero CPU — a parked socket read, not a busy poll):

```
                 ┌─────────────────────── steady state ───────────────────────┐
  bootstrap →  longpoll(cursor) ── blocks ~480s, wakes on change or timeout ──┐ │
                 │                                                             │ │
                 └──► continue(cursor) ─► [delta entries] ─► apply each ───────┘ │
                                                                                 │
   apply(entry):  file    → download to mirror, upsert index, emit created|modified
                  deleted → delete the path AND every indexed file beneath it
                            (folder deletes arrive as one entry — see below),
                            unlink each mirror file, emit one file.deleted per row
                  folder  → structural only (mkdir on mirror); no event
```

Mechanics, against Dropbox API v2:

- **Token.** `internal/dropbox` holds a `tokenSource`: caches the ~4h access
  token, refreshes via `POST /oauth2/token` (`grant_type=refresh_token`, app
  key+secret) on expiry or 401. Single source of truth for `Authorization` on
  the RPC (`api.dropboxapi.com`) and content (`content.dropboxapi.com`) hosts —
  **but not longpoll** (next bullet).
- **Cursor.** First boot (no stored cursor): `POST /2/files/list_folder`
  (`recursive:true`) to enumerate the whole app folder, then persist the
  returned `cursor`. Thereafter: `/2/files/list_folder/longpoll` on the cursor
  (`timeout:480`); when it reports `changes:true`, drain
  `/2/files/list_folder/continue` (looping while `has_more`), and persist the
  cursor returned by each `continue` page **after that page's entries are
  applied** (per-page, not only after the full `has_more` drain — so a crash
  mid-drain replays only the unapplied tail).
  - **Longpoll is unauthenticated.** `/2/files/list_folder/longpoll` is the one
    Dropbox endpoint that takes **no** `Authorization` header (the cursor is the
    capability); `client.go` MUST omit the bearer on longpoll, and send it on
    `list_folder`/`continue`/`download`.
  - **Longpoll client timeout.** Dropbox adds up to ~90s of random jitter on top
    of `timeout:480`, so a longpoll can legitimately block ~570s. The outbound
    HTTP client used for longpoll needs a `Timeout` of ≥600s (or 0 with a
    per-call context deadline) — a default/short client would tear the parked
    read down every cycle.
- **Download.** Each `FileMetadata` is fetched from
  `content.dropboxapi.com/2/files/download` (path in the `Dropbox-API-Arg`
  header) and written to the mirror **atomically** (temp file in the same dir →
  `rename`), preserving the relative path under the app folder. Before the
  rename, the temp file's Dropbox **block-SHA256 is recomputed and compared** to
  the metadata `content_hash`; a mismatch (truncated/corrupt download) is treated
  as a download failure and retried, so `content_hash` is a verified integrity
  check, not a stored decoration.
- **Idempotency / restart-safety.** The per-path index stores Dropbox's `rev` +
  `content_hash`. A `file` delta whose `rev` already matches the index is a no-op
  (no re-download, no duplicate event). A `deleted` delta for a path **already
  absent** from the index is likewise a no-op for events, but **still performs an
  idempotent unlink** (closing the crash window in the delete-ordering invariant,
  §6). On restart we resume from the persisted cursor, so anything that changed
  while we were down is delivered exactly once per net change.
- **Created vs modified** is decided by the index: a path absent from the index
  → `file.created`; present with a different `rev` → `file.modified`.
- **Case folding.** Dropbox paths are case-insensitive + case-preserving; the
  local mirror FS (ext4) is case-sensitive. The index matches case-insensitively
  but stores the current **display** path; a case-only rename
  (`report.pdf`→`Report.pdf`) is applied as a `rename` of the on-disk file (and a
  `file.modified`), never a second copy. `/content` resolves its query through
  the index (§4), so disk, index, and `content_url` never diverge on case.
- **Failure handling.** Network/5xx/429 → log `Warn`, honor `Retry-After`, back
  off, retry; never advance the cursor past an unapplied entry (at-least-once
  toward the mirror, dedup by `rev`/absent-path makes events effectively once-
  per-change). A download failure for one entry doesn't poison the rest of the
  page — the others apply and only the failed entry holds the cursor. **Poison-
  entry bound:** because the cursor can't advance past an unapplied entry, a
  permanently-undownloadable entry (disk full, a persistent 409/410 on download)
  would otherwise wedge *all* sync. After `DROPBOX_MAX_ENTRY_RETRIES` (default 5)
  failed passes on the same entry, the engine logs `Error`, marks the index row
  `failed` (a nullable `error TEXT` column on `files`, surfaced in
  `dropbox_health`), and advances past it so the rest of the feed keeps flowing;
  the failure is visible rather than silent.

## 3. The MCP surface (2 tools — it's a daemon)

Service-side is read-only; there are no write verbs. MCP is thin and exists for
the auth proof + the dashboard inventory (`MCP=true`).

| Tool | Shape | Role |
|---|---|---|
| `dropbox_whoami` | `()` | Unchanged identity probe (owner email + client id). The end-to-end auth proof. **Kept**, but slated to be *replaced* by `dropbox_health` later. |
| `dropbox_health` | `()` | The forward-looking health/status tool. **v1 returns the same identity content as `whoami`**, plus disk telemetry: `mirror_bytes` (sum of the index `size` column — indexed logical size, no directory walk), `disk_free_bytes` / `disk_total_bytes` (a `statfs` on the mirror path), and `failed_files` (count of index rows with a non-null `error` — the poison entries the engine advanced past, §2/§6). Grows later into full sync status (cursor age, file count, last event) and **supersedes `whoami`**. |

Both return identical identity fields in v1 so the eventual `whoami`→`health`
migration is additive, never a break.

## 4. The loopback content endpoint (how consumers get bytes)

Decided **Option B**: the mirror stays **private** (`0750 dropbox:dropbox`,
untouched from the chassis default); consumers fetch bytes over a loopback HTTP
route, reusing the exact trust model `/feed` already uses.

- **`GET /content?path=<url-encoded-dropbox-path>`** — loopback-only,
  **unauthenticated**, deliberately NOT behind `requireIdentityHeaders` (same as
  `/feed`: one box is one owner, the perimeter is "it's on 127.0.0.1").
  - **The handler is the primary guard, exactly mirroring `/feed`.** Per
    `eventplane/outbox/feed.go`, the public side is kept out **by the handler**,
    not by nginx: it returns `404` whenever it sees an nginx-injected identity
    header — `X-Owner-Email` **or** `X-Forwarded-Proto` (nginx always sets the
    latter). `/content` reuses that exact check. The nginx `404` block (§9) is
    optional defence-in-depth layered on top, **not** the mechanism — `/feed` has
    no such nginx block today and is safe purely on the handler check.
- The `path` query value is **URL-encoded** (it is also URL-encoded in the event
  `content_url`, §5); the handler decodes it, then resolves it **through the
  files index** (case-insensitive, §2 case-folding) to the canonical stored
  display path, and serves that on-disk file. Resolving via the index — not by
  concatenating the raw query onto the mirror root — is what keeps a
  case-mismatched query from 404-ing a file that exists.
- Serves the **current** bytes via `http.ServeContent` on the confined file
  (Range, HEAD, `Content-Type`, `Content-Length`, conditional requests for free).
  404 if the path isn't in the index (e.g. after a delete — correct).
- **Optional exact-bytes contract.** `/content` accepts an optional `rev` (or
  `hash`) query param; if present and it does not match the current indexed
  `rev`, the handler returns **409** rather than silently serving newer bytes.
  This lets a consumer demand "the exact bytes this event referenced, or a clear
  *moved-on* signal." Omitting it preserves the v1 "current bytes" behaviour.
- **Path safety.** The decoded path is cleaned and confined under the mirror
  root; any `..`/escape attempt → 400. The mirror root is the only servable tree.

## 5. Events (event-plane producer)

Producer-only. Same atomic-outbox discipline as `ledger`: the event row is
appended on the **same SQLite transaction** as the index update, and `Ring()`
fires after commit, so an event is emitted **iff** the mirror state changed.

**Three events**, payload = **reference, never bytes** (consumers fetch via §4):

| Event | When |
|---|---|
| `file.created` | a path not previously in the index now exists |
| `file.modified` | a known path's `rev` changed (includes a case-only rename) |
| `file.deleted` | a known path is gone — **one event per indexed file removed**, including every file beneath a deleted folder (§2: a folder delete arrives as a single `DeletedMetadata`; we fan it out over the index subtree) |

Payload fields (the `path` inside `content_url` is **URL-encoded**; the bare
`path` field is the literal Dropbox path):

```jsonc
{
  "event":       "file.created",           // | file.modified | file.deleted
  "path":        "/inbox/report.pdf",       // literal path within the app folder
  "rev":         "0123456789abcdef",        // Dropbox content revision (last-known on delete)
  "content_hash":"<dropbox block sha256>",  // verified at download; for race detection
  "size":        91234,                      // bytes (last-known on delete)
  "content_url": "http://127.0.0.1:3005/content?path=%2Finbox%2Freport.pdf",
  "occurred_at": "2026-06-04T15:07:15.000000000Z"
}
```

The `file.deleted` payload carries the file's **last-known** `rev`/`content_hash`/
`size`, which the builder reads from the index row **before** the in-tx delete
removes it.

- **Race honesty.** `content_url` resolves to *current* bytes (mirror holds
  current state only — no historical revisions). A consumer that fetches and
  finds `content_hash` differs knows the file already moved on and a newer event
  is in flight; it can re-handle on that one. This is the deliberate, documented
  trade for not retaining old versions.
- **First boot emits `file.created` for every existing file** (don't silently
  baseline — the point is "don't miss files"). The `ikigai-onebox` folder starts
  empty in v1, so this is moot today; flagged here because a pre-populated folder
  would emit a `created` burst.
  - **Future — serialize the bootstrap.** v1 persists the cursor only *after* the
    initial enumeration completes, so an interrupted first boot of a large folder
    restarts the whole bootstrap. A later version should **checkpoint progress
    during** the initial enumeration (e.g. persist the intermediate
    `list_folder/continue` cursor + per-path index as it pages) so bootstrap is
    resumable. Out of scope for v1 (empty folder), explicitly deferred.
- **Folders** are structural only (mkdir on the mirror); no folder events in v1.
- **Deletes** remove the mirror file + index row and emit `file.deleted` (its
  `content_url` then 404s, correctly). A folder delete arrives from Dropbox as a
  **single** `DeletedMetadata` for the folder path — it does *not* enumerate
  descendants — so `apply(deleted)` selects every indexed file whose path is at
  or beneath that prefix, unlinks each, deletes each row, and emits one
  `file.deleted` per row, all on the same tx. Missing this would silently leak a
  whole subtree into the mirror and the index.

## 6. Data model / migrations

SQLite via the chassis `internal/db` (WAL, FK, single-writer). Greenfield; the
dev DB is deleted/recreated.

- **Keep** `001_schema_migrations.sql` (chassis) byte-identical.
- **Add** `002_dropbox.sql`:
  - `sync_state(id INTEGER PRIMARY KEY CHECK(id=1), cursor TEXT, updated_at TEXT)`
    — a single-row table holding the current Dropbox `list_folder` cursor.
  - `files(path TEXT PRIMARY KEY, rev TEXT NOT NULL, content_hash TEXT NOT NULL,
    size INTEGER NOT NULL, updated_at TEXT NOT NULL, error TEXT)` — the per-path
    mirror index; drives created-vs-modified, dedup, delete, and `mirror_bytes`
    (`SELECT SUM(size)`). `path` stores the current **display** path; a
    `path_lower` is matched for Dropbox's case-insensitive semantics (store
    display, match folded — §2 case-folding). The nullable `error` column holds
    the last failure for a poison entry the engine advanced past (§2 poison-entry
    bound); `dropbox_health` surfaces the count of non-null `error` rows.
- **Add** `003_outbox.sql` — byte-identical to `outbox.SchemaSQL`, with the
  carried-over byte-equality test (same discipline as `ledger`).

**Service invariant (crash ordering).** An event is written on the **same tx** as
the `files` upsert/delete; the mirror file write (download/unlink) is ordered so
disk and index never diverge across a crash, and the cursor advances only after a
page's entries are all applied (§2). Per direction:

- **create / modify:** write the file first (atomic temp+rename, hash-verified),
  **then** commit {index upsert + event}, **then** `Ring()`. Crash before the
  commit → cursor replays the delta → re-download is an idempotent rename and the
  index upsert/event re-runs; crash after commit but before cursor advance →
  replay sees a matching `rev` → no-op, no duplicate event.
- **delete:** read the row's last-known fields, commit {row delete + event},
  **then** unlink the mirror file. The crash window (committed, not yet unlinked)
  is closed on restart because the cursor has not advanced, so the delete delta
  **replays**; §2 makes a delete of an **already-absent** path perform the
  idempotent unlink **without** re-emitting — so the orphan file is removed and no
  duplicate `file.deleted` is produced. (This is why "delete of absent path =
  silent no-op for events, but still unlink" is load-bearing, not a nicety.)

`mirror_bytes` is `SUM(size)` over the index — the **indexed logical size**, not a
`du` of the directory; the two coincide except transiently across the delete
crash window above.

**Tenancy:** single box, single account; no owner column. `Identity` is consulted
only by `dropbox_whoami`/`dropbox_health`.

## 7. Keep / replace / delete (exact)

**Keep untouched (platform scaffolding):**
- `internal/db/db.go` (migration runner), `internal/ids`, `internal/logging`.
- `internal/server/*` — routing, PRM well-known, `requireIdentityHeaders`,
  whoami, security headers, graceful shutdown, `GET /feed`. **Add** the
  loopback `GET /content` route (§4).
- `internal/mcp/mcp.go` — the JSON-RPC transport (rename serverInfo to
  "Dropbox"; swap the injected service type).

**Replace:**
- `internal/mcp/tools.go` → the 2-tool surface (`dropbox_whoami`,
  `dropbox_health`) + dispatch.
- `cmd/ledger/main.go` → `cmd/dropbox/main.go`: read the three secrets + paths
  from env, build the `tokenSource`, open db, wire outbox, start the sync engine
  goroutine, mount the content handler.

**Add:**
- `internal/db/migrations/002_dropbox.sql`, `003_outbox.sql` (§6).
- `internal/dropbox/` domain package (§8).

**Delete (ledger-specific):** the 8-verb ledger domain, its migrations, its
billing example.

## 8. Package architecture

Same chassis layering as `ledger`/`crm` (one file per concern inside one
package; `internal/mcp/tools.go` stays the sole MCP dispatcher):

```
internal/dropbox/
  types.go     shared structs (FileMeta, DeltaEntry, HealthInfo), error sentinels
  client.go    Dropbox API v2 client: tokenSource (refresh→access cache),
               list_folder / longpoll / continue, download (+ block-SHA256
               verify). The only HTTP-to-Dropbox site. Sends the bearer on
               rpc/content hosts, OMITS it on longpoll; longpoll uses the
               long-timeout (~600s) client. No disk, no db.
  store.go     SQL-only data layer (*sql.Tx methods): sync_state get/set cursor,
               files upsert/delete/get (folded path lookup), subtree-delete by
               prefix, SUM(size), mark-error, shared scanning.
  mirror.go    the private local mirror: atomic write (temp+rename), delete,
               rename (case-only changes), mkdir, path-confinement, statfs for
               health. No db, no HTTP.
  sync.go      the engine: the longpoll→continue→apply loop, created/modified/
               deleted decision (incl. folder-delete subtree fan-out and
               absent-path no-op), per-page cursor advance, poison-entry bound.
               Orchestrates client+mirror+store+outbox under Service.
  service.go   the Service type: holds store + mirror + client + outbox; owns the
               tx that commits {files index change + outbox event} atomically;
               exposes Content(path, rev?) for the endpoint and Health() for the
               tool.
  events.go    event payloads + builders (file.created/modified/deleted), with
               URL-encoded content_url. Mirrors ledger's producer seam: an
               `EventSink` interface (`AppendFileEvent(tx, …)` + `Ring()`) the
               Service appends to inside the index tx, with a concrete
               `outboxProducer` wrapping `outbox.Append`/`Ring`. The interface
               lets the engine run with emission disabled (Outbox == nil) in unit
               tests without importing the library — exactly as
               `ledger/internal/ledger/{service.go,events.go}` does.
  health.go    HealthInfo assembly (identity + mirror_bytes + disk telemetry +
               failed-row count).
```

`internal/mcp/tools.go` holds the 2 descriptors, dispatches into
`dropbox.Service` (`Whoami`/`Health`), translating any sentinel to MCP tool-error
text.

## 9. Config, secrets, manifest, deploy

- **`etc/manifest.env`:** `APP=dropbox`, `MOUNT=/srv/dropbox/`, `DEFAULT=false`,
  `PORT=3005`, `MCP=true`.
- **Secrets (unlike ledger, dropbox HAS them).** Three values reach the process
  env only: `DROPBOX_APP_KEY`, `DROPBOX_APP_SECRET`, `DROPBOX_REFRESH_TOKEN`.
  - **Dev:** `.envrc` → `export DROPBOX_*="$(cat ~/.secrets/DROPBOX_*)"`
    (already provisioned in §"setup done").
  - **Box:** SSM `/ikigenba/<account>/app-config` key `dropbox`, injected by
    `ikigenba-launch`. New `bin/secrets` does a non-destructive read-modify-write
    of only the `dropbox` key from `~/.secrets/DROPBOX_*` (masked summary, never
    printed).
- **Non-secret config** resolved at the `main.go` boundary: `DROPBOX_MIRROR_PATH`
  (box `/opt/dropbox/data/mirror`, dev `./tmp/mirror`), `DROPBOX_DB_PATH`,
  `DROPBOX_GENERATION_PATH`, `DROPBOX_RESOURCE_ID`, `DROPBOX_AUTH_SERVER`,
  `DROPBOX_LONGPOLL_TIMEOUT` (default 480), `DROPBOX_MAX_ENTRY_RETRIES` (default
  5, the §2 poison-entry bound), `DROPBOX_APP_FOLDER_ROOT` (default `""` = the app
  folder root). The `bin/build` wrapper sets the non-secret public config from
  `IKIGENBA_DOMAIN`; the launcher adds the secrets.
- **`bin/*`:** `build deploy setup start stop secrets` (six — ledger's five plus
  `secrets`). `bin/setup` creates the `dropbox` user + `/opt/dropbox` tree (data
  `0750`, **mirror is a private subdir of data**), enables the systemd unit, drops
  the nginx fragment. `bin/build` copies the shared `../bin/registry`. The
  `LEDGER_*`→`DROPBOX_*` rename in the wrapper is surgical: the shared event-plane
  knobs `OUTBOX_RETENTION_DAYS` / `OUTBOX_RETENTION_MAX_ROWS` (read in `main.go`)
  keep their `OUTBOX_` prefix — they are **not** `LEDGER_*` and must not be swept.
- **No `bin/backup` / `bin/restore`.** No service ships these — S3 backup/restore
  is opsctl-owned suite-wide (D07). dropbox additionally needs no state snapshot
  at all: the mirror is a download-only replica of Dropbox and the SQLite
  state (cursor + `files` index) is fully reconstructible by re-bootstrapping —
  **Dropbox is the source of truth.** Recovery on data loss is "delete the DB +
  mirror, restart, re-enumerate" (which re-emits `file.created` for every file,
  §5), not a snapshot restore. The generation sidecar still lives outside the DB,
  so a wipe mints a fresh epoch and consumers resync cleanly.
- **nginx fragment** (`etc/nginx.conf`): the standard PRM-open + `auth_request`-
  gated `/srv/dropbox/` block. The **dev mirror is generated, not committed** —
  `nginx/run` regenerates `nginx/locations/<svc>.conf` from each service's
  `etc/nginx.conf` on every run, iterating a **hardcoded service list**
  (`for svc in crm ledger notify prompts`); **`dropbox` must be added to that loop**
  or it never appears on the dev front door (:8080). Public protection of
  `/content` is the **handler's** identity-header check (§4), exactly as `/feed`
  is protected today (ledger ships **no** nginx block for `/feed`). An exact-match
  `location = /srv/dropbox/content { return 404; }` is added as optional defence-
  in-depth — belt to the handler's braces — not as the mechanism.
- **Dashboard registration (no code edit — derived from the manifest).** The
  dashboard no longer hardcodes a resource list: `dashboard/bin/build` exports
  `DASHBOARD_MANIFEST_ROOT=/opt` and the AS **derives** its `/srv/<svc>/mcp`
  resource set at startup from each deployed service's `etc/manifest.env`
  (`MCP=true`). So registering dropbox is just: `bin/deploy` (which lands
  `/opt/dropbox/etc/manifest.env`) → **restart the dashboard** to re-read the
  manifests. No `dashboard/bin/build` edit. Until that restart, the `/srv/dropbox/
  mcp` 401 challenge omits `resource_metadata`. (The root `CLAUDE.md`'s
  "add to `DASHBOARD_RESOURCES`" note is itself stale on this point.)

## 10. Build phases & verification

Dependency-ordered and **strictly sequential** — every phase is a self-contained
hand-off to **one** sub-agent (no phase runs in parallel with another, and no
phase fans out internally). Each sub-agent starts from a green `go build ./...` /
`go test ./...`, touches only the files its phase **Owns**, ends at its **Gate**,
and leaves the tree green for the next. Every sub-agent reads §1–§9 of this plan
first; the **Context** line names the chassis files it should read before writing.

Because Go does not error on an unused exported function, the leaf phases (Client,
Store, Mirror) compile green even though nothing calls them yet — the engine phase
(Phase 4) is where they get wired and exercised end to end.

1. **Phase 0 — Scaffold.**
   - *Owns:* the whole `dropbox/` tree except `PLAN.md`. Clone `ledger/` →
     `dropbox/` (preserve `dropbox/PLAN.md`); rename Go module `ledger`→`dropbox`,
     `cmd/ledger`→`cmd/dropbox`, every import path, the `LEDGER_*` env prefix →
     `DROPBOX_*` **leaving `OUTBOX_*` untouched** (§9), `serverInfo` "Ledger"→
     "Dropbox". Delete the 8-verb ledger domain (`internal/ledger/*`) and replace
     with a stub `internal/dropbox` (`types.go` + a `Service` stub exposing
     `Whoami`); rewrite `internal/mcp/tools.go` to the **2-tool** surface
     (`dropbox_whoami` now; `dropbox_health` descriptor returning identity-only as
     a stub) + dispatch; drop the ledger domain tests, keep & adapt
     `internal/server/*_test.go` (whoami/feed/identity) and `internal/db` tests.
     Replace migrations: keep `001_schema_migrations.sql` byte-identical, write
     `002_dropbox.sql` (§6: `sync_state` + `files` incl. `path_lower` + nullable
     `error`), keep `003_outbox.sql` byte-identical to `outbox.SchemaSQL` and carry
     `migrations_outbox_test.go`. Update `etc/manifest.env` (§9), `etc/nginx.conf`,
     `etc/deploy.env`, `bin/*`. Add `dropbox` to root `go.work` **and** to the
     `for svc in …` loop in `nginx/run` (§9).
   - *Context:* `ledger/` in full (esp. `cmd/ledger/main.go`,
     `internal/{server,mcp,db}/`), `ledger/go.mod`, `go.work`, `nginx/run`.
   - *Gate:* `go build ./...` and `go test ./...` green across the workspace;
     running `cmd/dropbox` on :3005 answers a loopback `POST /mcp`
     `tools/call dropbox_whoami` (with an injected `X-Owner-Email` header) and
     lists exactly the two tools.

2. **Phase 1 — Dropbox API client.**
   - *Owns:* `internal/dropbox/client.go` (+ client-facing types in `types.go`).
     `tokenSource` (refresh→access cache, refresh on expiry/401);
     `list_folder`/`longpoll`/`continue`/`download`; block-SHA256 verify of
     downloaded bytes vs `content_hash`. Bearer on rpc/content hosts, **omitted on
     longpoll**; the longpoll path uses a **≥600s-timeout** HTTP client (§2). No
     disk, no db.
   - *Context:* §2 mechanics; Dropbox API v2 docs (confirm longpoll-no-auth, the
     jitter timeout, the block-hash algorithm, the folder-delete delta shape — the
     plan states these from design knowledge, verify them here). Reads the three
     `DROPBOX_*` secrets from env (already in `.envrc`); never prints their values.
   - *Gate:* an integration probe against the live `ikigai-onebox` credential
     refreshes a token and enumerates the (empty) app folder, returning a cursor.

3. **Phase 2 — Store (SQL data layer).**
   - *Owns:* `internal/dropbox/store.go` — `*sql.Tx` methods only: `sync_state`
     get/set cursor; `files` upsert / get (folded `path_lower` lookup) / delete;
     **subtree-delete by prefix**; `SUM(size)`; mark-`error`; shared row scanning.
     Finalizes `002_dropbox.sql` if the stub schema needs columns.
   - *Context:* §6 data model; `ledger/internal/ledger/store.go` for the `*sql.Tx`
     method style; `internal/db/db.go` (WAL, single-writer).
   - *Gate:* a unit test round-trips upsert→get (case-insensitive hit on a
     case-mismatched query), a prefix subtree-delete removes exactly the rows
     at/under the prefix, and `SUM(size)` matches.

4. **Phase 3 — Mirror (filesystem).**
   - *Owns:* `internal/dropbox/mirror.go` — the private local mirror: atomic write
     (temp+rename in the same dir), delete, case-only rename, `mkdir`, path
     **confinement** under the mirror root, `statfs` for health. No db, no HTTP.
   - *Context:* §4 path safety; §6 crash-ordering (the mirror write/unlink order is
     driven by the engine, but the primitives live here).
   - *Gate:* a unit test writes+reads a file atomically, a confinement-escape path
     (`..`) is rejected, a case-only rename renames on disk, and `statfs` returns
     plausible free/total bytes.

5. **Phase 4 — Sync engine + Service + Events (the integration heart).**
   - *Owns:* `internal/dropbox/{sync.go,service.go,events.go}`. `service.go`: the
     `Service` holding store+mirror+client+`EventSink`, owning the tx that commits
     `{files change + outbox event}` atomically and exposing `Content(path,rev?)` +
     `Health()`. `events.go`: the three payload builders + the `EventSink`/
     `outboxProducer` seam (§8). `sync.go`: the longpoll→continue→apply loop, the
     created/modified/deleted decision incl. **folder-delete subtree fan-out** and
     **absent-path idempotent-unlink-emit-nothing**, **per-continue-page** cursor
     advance, restart-safety, and the **poison-entry bound** (§2). Wire the engine
     goroutine + `003_outbox` producer into `cmd/dropbox/main.go`.
   - *Context:* §2/§5/§6 (the load-bearing correctness rules);
     `ledger/internal/ledger/{service.go,events.go}` (the `persist`→`Append`→`Ring`
     atomic-outbox seam); `eventplane/outbox/outbox.go` (`Append`/`Ring`);
     `agent/internal/runner` for the background-goroutine lifecycle (start in
     `main`, clean shutdown on `ctx` cancel).
   - *Gate (end-to-end against live Dropbox):* drop a file in `ikigai-onebox` →
     mirror file appears + `file.created` on `/feed`; modify → `file.modified`;
     delete → mirror file gone + `file.deleted`; **replaying a delete delta on an
     already-absent path emits no duplicate**; **deleting a folder removes every
     file beneath it with one `file.deleted` each**; each event's `content_url`
     resolves.

6. **Phase 5 — Content endpoint + Health.**
   - *Owns:* the loopback `GET /content` route (new `Content http.Handler` field on
     `server.Options`, threaded like `Feed`) — confined, **index-resolved** (case-
     fold, §4), `http.ServeContent`, optional `rev`→409, and the **handler
     identity-header guard** copied from `eventplane/outbox/feed.go:50`
     (`X-Owner-Email` || `X-Forwarded-Proto` → 404). Plus `internal/dropbox/
     health.go` and the real `dropbox_health` tool body (identity + `mirror_bytes`
     `SUM(size)` + `statfs` disk numbers + `failed_files` count).
   - *Context:* §3, §4; `ledger/internal/server/{server.go,handlers.go}` (route
     wiring, the `Feed` field pattern); `eventplane/outbox/feed.go:50`.
   - *Gate:* `curl 127.0.0.1:3005/content?path=…` returns bytes for a URL-encoded
     path (incl. one containing a space); a stale `rev` → 409; a request carrying
     `X-Forwarded-Proto` → 404; `dropbox_health` returns identity + disk numbers +
     `failed_files`; the optional public `/srv/dropbox/content` nginx block 404s.

7. **Phase 6 — Deploy & config wiring.**
   - *Owns:* finalize `etc/{manifest.env,nginx.conf,deploy.env}`, the `bin/build`
     wrapper env (`DROPBOX_*` public config from `IKIGENBA_DOMAIN`; `OUTBOX_*`
     left alone), `bin/secrets` (read-modify-write only the `dropbox` key from
     `~/.secrets/DROPBOX_*`, modeled on `notify`/`prompts` `bin/secrets`), the
     optional `/srv/dropbox/content`→404 nginx block, and confirm the `nginx/run`
     loop + `go.work` entries from Phase 0.
   - *Context:* §9; `notify/bin/secrets`, `prompts/bin/secrets`; `ledger/bin/{build,
     setup,deploy}`; `dashboard/bin/build` (the `DASHBOARD_MANIFEST_ROOT=/opt`
     derivation — **no edit needed there**, registration is a dashboard *restart*).
   - *Gate:* `nginx -t` passes in dev with the dropbox fragment present; `bin/build`
     produces `build/dropbox`, `build/dropbox.bin`, `build/registry`;
     `bin/secrets` round-trips the `dropbox` key (masked) preserving siblings (dry
     run / against a test param). *(Actual box `setup`/`deploy` + dashboard restart
     is an operator step, gated on a live SSO session — out of band for the
     sub-agent.)*

8. **Phase 7 — Docs. [DONE]** Wrote `dropbox/CLAUDE.md` (mirroring
   `ledger/CLAUDE.md`'s shape: what it is, the daemon+producer model, the 2-tool
   surface, the package layout, manifest/deploy, the no-backup decision); set this
   plan's status to **built** (see top). *Gate:* docs land; `go build ./...` /
   `go test ./...` still green. ✓

Verification throughout: `go build ./...`, `go test ./...`, drive `/mcp` over
loopback, and the end-to-end check — *create a file in the `ikigai-onebox`
Dropbox app folder → observe the mirror file, the `/feed` event, and a
`/content` fetch.*

## 11. Decisions ledger (design pass, 2026-06-04)

| # | Decision |
|---|---|
| Account model | Single box Dropbox (Model A); app `ikigai-onebox`, App-folder; refresh token in SSM `app-config`. Per-user OAuth out of scope. |
| Direction | One-way ↓, service-side read-only; an ingest tool into the event plane. |
| Change detection | Longpoll on a cursor (`timeout:480`); zero inbound surface; idle ≈ zero CPU. Webhooks rejected (public surface, missed-event semantics). Longpoll is **unauthenticated** and uses a **~600s-timeout** client (480 + ~90s Dropbox jitter); rpc/content calls carry the bearer. |
| Byte delivery | Loopback `GET /content` (Option B); mirror stays private `0750`; URL-encoded `content_url` by path, **resolved through the index** (case-fold). Public protection is the **handler's identity-header check** (mirrors `/feed`; nginx 404 is optional defence-in-depth). Optional `rev`→409 exact-bytes contract. Shared-filesystem (Option A) rejected — it would invent a cross-service coupling the platform avoids. |
| Events | `file.created` / `file.modified` / `file.deleted`; payload = reference, not bytes; emitted iff mirror state changed (atomic with index). A **folder delete fans out** to one `file.deleted` per indexed file beneath it. |
| Crash/replay | Create: file→commit{index+event}→cursor (replay dedups by `rev`). Delete: commit{row+event}→unlink; a replay of a delete on an **absent path** unlinks idempotently and **emits nothing** (no duplicate `file.deleted`, no orphan file). Cursor advances **per continue-page**. |
| Integrity | Downloaded bytes are **hash-verified** (recompute Dropbox block-SHA256 vs metadata) before the atomic rename; mismatch = retry. |
| Liveness | Poison-entry bound: after `DROPBOX_MAX_ENTRY_RETRIES` (5) failed passes, mark the row `error` and advance so one bad entry can't wedge all sync; surfaced as `failed_files` in health. |
| Case | Dropbox case-insensitive + case-preserving vs case-sensitive FS: index stores display path, matches folded; case-only rename = on-disk rename + `file.modified`. |
| First boot | Emit `file.created` for all existing files (no silent baseline). |
| Mirror | Current state only; no historical revisions. Atomic temp+rename writes. |
| MCP surface | `dropbox_whoami` (kept) + `dropbox_health` (identity now + `mirror_bytes`/`disk_free`/`disk_total`/`failed_files`; supersedes whoami later). |
| Identity | `APP=dropbox`, `/srv/dropbox/`, `PORT=3005`, `MCP=true`; HAS secrets → `bin/secrets`. |
| Chassis | Clone `ledger` producer chassis (server+mcp+db+outbox+/feed); add `internal/dropbox` engine + content endpoint. |
