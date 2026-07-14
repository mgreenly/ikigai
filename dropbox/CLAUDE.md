# dropbox

`dropbox` is the ikigenba suite's single-tenant Dropbox-backed filesystem
service. It runs loopback-only behind the dashboard's nginx session gate,
serves MCP to agents and a human landing page at
`<account>.ikigenba.com/srv/dropbox/`, and trusts the identity headers nginx
injects after `auth_request`; it has no token validation of its own.

The private local mirror is the suite's authority for filesystem operations.
Downloads from Dropbox flow down unchanged, while suite writes are committed
locally and flow up asynchronously through the durable `upload_queue` and its
uploader. Uploads use Dropbox's overwrite policy, making Dropbox a replica of
the suite's local state. A single Dropbox app folder (`ikigai-onebox`) and a
suite-level refresh token serve the whole box; there is no per-user OAuth or
tenant partitioning.

## Service model

`cmd/dropbox/main.go` is the composition root. It builds the domain service,
the longpoll download engine, and the upload worker; gives appkit its migrations,
event registry, health reporter, landing assets, MCP handler, loopback routes,
and workers. The chassis owns the HTTP server, `POST /mcp`, health/reflection,
and the producer feed.

The background download engine remains longpoll-driven:

```
bootstrap -> longpoll(cursor) -> continue(cursor) -> apply each delta -> advance cursor
```

Bootstrap enumerates the app folder when no cursor exists. On a reported
change, the engine drains `list_folder/continue` pages and persists the cursor
after each applied page. The uploader separately drains durable local mutations
and retries failures with backoff; it can therefore keep local filesystem work
fast and independent of Dropbox availability.

## MCP surface

The MCP surface has eight tools: six service tools — `list`, `get`, `put`,
`mkdir`, `delete`, and `move` — plus the chassis-owned `health` and
`reflection` tools.

- `list(path?, cursor?, limit?)` recursively enumerates the mirror in path
  order. Entries carry a `kind` of `file` or `dir`; directories are first-class,
  including empty directories. File entries also carry their size, hash,
  revision, and update time.
- `get(path, rev?)` returns one file's bytes and metadata, with bytes in
  `content_base64`. It supports a revision pin and rejects base64 responses over
  25 MiB with `too_large`.
- `put(path, source_url? | content_base64?)` creates or replaces a file. Its
  primary form, `source_url`, is fetched server-side from an allowed loopback
  address on a registry-owned port; that streamed form is uncapped.
  `content_base64` is the convenience form and is capped at 25 MiB decoded.
  Exactly one source is required. An unavailable otherwise-allowed source
  returns `source_unavailable`.
- `mkdir(path)` creates an empty directory; `delete(path)` removes a file or
  directory tree idempotently; `move(from, to)` renames a file or directory in
  one operation.

Structured errors include `not_found`, `conflict`, `validation`, `too_large`,
and `source_unavailable` where applicable.

## Loopback filesystem API

Services sharing the mirror use the loopback filesystem API documented in
[`docs/filesystem-api.md`](docs/filesystem-api.md). Every route is guarded by the
shared chassis loopback guard: any request stamped `X-Forwarded-Proto` (i.e. one
that crossed nginx) gets a bare 404, while loopback callers — including those
asserting `X-Owner-Email`/`X-Client-Id` themselves — are served:

- `GET /content` streams a file, optionally pinned by `rev`.
- `PUT /content` writes file bytes; `DELETE /content` removes a file or
  directory tree.
- `POST /mkdir` creates a directory and `POST /move` renames or moves an entry.
- `GET /stat` returns metadata for one file or directory; `GET /list` pages
  through both file and directory entries.

Successful mutations commit the local mirror and database transaction before
returning, then queue the corresponding Dropbox operation for asynchronous
upload. Mutation callers supply `X-Client-Id`; it becomes the emitted event's
`origin`.

## Events

dropbox is an event-plane producer. Its event kinds are `create`, `modify`, and
`delete`; the event subject is the mirror's display path. Registry event keys
therefore have family-shaped addresses such as
`dropbox:create/bills/aws/2026-06.pdf`.

Each payload is a reference to current bytes rather than bytes themselves. It
has `path`, `rev`, `content_hash`, `size`, `content_url`, `occurred_at`, and
`origin`; there is no `event` discriminator in the payload. `content_url`
contains a URL-encoded path to the loopback content endpoint. `origin` is
`"dropbox"` for a pulled Dropbox change and the writing service's client ID for
a suite-originated mutation. Deletes carry the removed file's last-known
metadata. A folder deletion fans out to one delete event for every indexed file
beneath it.

The files-index change and event append use the same SQLite transaction, and
the outbox is rung only after commit. This makes an event observable exactly
when its mirror state change committed.

## Load-bearing download invariants

- A delete commits the index and its event before unlinking the mirror path;
  replay closes a crash in that window. An already-absent delete still unlinks
  idempotently but emits nothing.
- The cursor advances only after each `continue` page has applied. A permanently
  bad entry is retried up to `DROPBOX_MAX_ENTRY_RETRIES` (default 5), marked as
  an error, then advanced past so it cannot wedge the stream.
- Downloads verify Dropbox block-SHA256 before their atomic rename. A hash
  mismatch is retried, never accepted into the mirror.
- Dropbox paths are case-insensitive and case-preserving while the mirror is
  case-sensitive. The index uses a folded lookup key, and a case-only rename is
  an on-disk rename plus a modify event rather than a second file.
- Longpoll omits the bearer token and uses a client timeout of at least 600
  seconds; Dropbox RPC and content requests carry the bearer token.

## Package and deployment notes

`internal/dropbox` owns the mirror, index, sync engine, uploader, events, and
filesystem handlers. `internal/mcp` owns the six service MCP tools.
`internal/db` embeds the SQLite migrations. `share/www` supplies the landing
page and static assets through `Spec.WWW`.

The service is one appkit binary with the fixed `serve`, `version`, `manifest`,
`migrate`, and `schema` verbs. Its three Dropbox secrets are
`DROPBOX_APP_KEY`, `DROPBOX_APP_SECRET`, and `DROPBOX_REFRESH_TOKEN`; they are
read only at the composition root and never logged. `opsctl setup dropbox`
provisions the service and nginx location fragment, while `bin/ship dropbox`
stages and deploys it.

## Recovery, not backup

dropbox has no custom backup or restore hooks. The mirror and its index can be
reconstructed from Dropbox by wiping local state and re-bootstraping. That
recovery deliberately loses any queued-but-unpushed local mutations held in
`upload_queue`; operators must accept that loss before rebuilding the local
state from Dropbox.
