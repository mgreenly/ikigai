# notify

The **notify** service for the metaspot single-tenant suite. A pure MCP API with
**no UI** and **no token logic**, deployed at `<account>.metaspot.org/srv/notify/`
(e.g. `ai.metaspot.org/srv/notify/`). First demo account: **ai**.

notify is the suite's **first event-plane consumer**. It was duplicated from
`../ledger` (the whoami-only chassis skeleton) and given a domain: it subscribes
to crm's east/west event feed and fires a best-effort ntfy.sh push for every
contact created. The MCP `notify_whoami` tool is retained as the north/south auth
proof; the real work happens in the background consumer loop.

**Read the decisions first — do not re-derive them:**

- `../../docs/event-protocol.md` — the **normative** event-plane wire contract.
  On any conflict it wins over this file. notify is a *consumer* under §10.
- `../../docs/event-plane-decisions.md` — the design rationale for this consumer.
- `../../metaspot/AGENTS.md` — platform spec (Service layer = path routing).
- `../../metaspot/docs/path-routing-architecture.md` — server-side topology + the
  auth contract you live under.
- `../crm` — the producer this consumer reacts to (owns the `contact.created`
  payload shape, §8.6); `../ledger` — the chassis skeleton this was cloned from;
  `../eventplane` — the shared library whose `consumer` package is the engine.

If anything here conflicts with those docs, the docs win — and flag the conflict.

## The two planes notify lives on

- **North/south (external, owner-facing).** nginx terminates TLS, introspects
  every request via `auth_request` against the dashboard, strips the
  `/srv/notify/` prefix, and injects `X-Owner-Email` / `X-Client-Id`. notify
  trusts those headers and does NO token logic. Surface: `POST /mcp`
  (`notify_whoami`) and the unauthenticated RFC 9728 PRM doc. notify is a
  consumer, **not** a producer — it serves **no** `/feed` endpoint, and its nginx
  fragment (`etc/nginx.conf`, dev mirror `../nginx/locations/notify.conf`) has no
  feed block.
- **East/west (internal, service-to-service).** A background goroutine runs
  `eventplane/consumer.Run`, holding one long-lived SSE connection to crm's
  `http://127.0.0.1:3001/feed` (loopback-direct — the event plane bypasses nginx,
  §2). It is unauthenticated and loopback-only by construction.

## What the consumer does

- **Engine, not hand-rolled.** All the hard parts — the SSE client, the
  reconnect/backoff loop, the durable per-upstream cursor, and all four
  connect-time resync reasons — live in `eventplane/consumer`. notify supplies
  only a `Config` and a `Handler`.
- **The effect is best-effort (§11.2).** `internal/push` maps `contact.created`
  → one ntfy POST (`Title: New contact`, body = the contact's `display_name`,
  `Authorization: Bearer <NTFY_API_KEY>`), fired **asynchronously** in a
  timeout-bounded goroutine. The engine commits the cursor regardless of the push
  outcome, so the controlled leg (crm → notify) stays at-least-once while end-user
  delivery is intentionally unreliable. A non-`contact.created` event runs no
  push but **still advances the cursor** (consumer-side filtering, §7.3). There is
  no dedup table — duplicate pushes on reconnect are expected and acceptable.
- **First-subscription = `tail` by default** (`NOTIFY_FROM`), so a fresh notify
  only pushes for contacts created from now on, not the entire backlog.
- **Structural vs transport (decision 11).** A `feed_offset` read/write failure
  (a missing table — a deploy bug) crashes the whole process so systemd
  restart-loops visibly; crm being down is a transport fault the engine retries
  indefinitely without bringing notify down. `cmd/notify` runs the HTTP server and
  the consumer under one context: a structural consumer fault cancels the server
  too — no half-alive (HTTP up / consumer dead) state.

## Secrets

The ntfy **topic** and **key** are deployment secrets (`~/.secrets/NTFY_TOPIC`,
`~/.secrets/NTFY_API_KEY`). They reach the process only via the environment: the
committed `.envrc` injects them locally (run `direnv allow` once); app-config
injects them in prod. notify reads them with `getenv` at its composition root
(`cmd/notify/main.go`) and **fails loudly at boot** if either is absent. Never
read, log, or commit their values (the `secrets` skill's hard rule). The ntfy
**base URL** (`NOTIFY_NTFY_BASE_URL`, default `https://ntfy.sh`) is plain config,
so tests point it at a mock.

## Layout

- **`internal/push`** — the domain: the ntfy `Client` and the `consumer.Handler`
  that filters and pushes. Mirrors how crm's `internal/contacts` owns the producer
  domain.
- **`internal/db`** — SQLite open (WAL, FK, single-writer) + migration runner.
  `001_schema_migrations`, then `002_feed_offset` which applies
  `consumer.SchemaSQL` verbatim (asserted by `migrations_feed_offset_test.go`).
- **`internal/mcp`, `internal/server`, `internal/logging`, `internal/ids`** — the
  carried-over chassis (whoami, PRM, identity gate, security headers, request ids).

## Tests

`go test ./...` (workspace mode via `ikigai/go.work`). The migration-assertion test
guards that `002_feed_offset.sql` stays byte-identical to `consumer.SchemaSQL`.
The §13c e2e (`internal/push`) wires the **real** `outbox.FeedHandler` to a
consumer whose handler points at a **mock** ntfy server, and asserts a
`contact.created` yields exactly one correctly-shaped POST while a
non-`contact.created` event yields none but still advances the cursor. Real
ntfy.sh is never contacted.

## Manifest / deploy

`etc/manifest.env`: `APP=notify`, `MOUNT=/srv/notify/`, `DEFAULT=false`,
`PORT=3003` (loopback), `MCP=true` (so the dashboard inventory lists it). Five
`bin/*` scripts (build/start/stop/setup/deploy); the build wrapper exports the
public consumer config (`NOTIFY_FEED_URL`, `NOTIFY_FROM`, `NOTIFY_NTFY_BASE_URL`)
and leaves the ntfy secrets to app-config. No `plugin/` in this repo. notify is a
consumer with no generation sidecar, so there is no `bin/restore` concern: a
consumer restored from an older snapshot simply replays from its rolled-back
cursor, and best-effort tolerates the duplicates (§11.1).
