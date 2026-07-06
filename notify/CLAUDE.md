# notify

The **notify** service for the ikigenba single-tenant suite serves an MCP surface
for agents and a dashboard-session-gated landing page under `/srv/notify/`.
nginx remains the north/south trust boundary: it terminates TLS, performs the
dashboard auth checks, strips the mount prefix, and injects identity headers.
notify does not implement token validation.

notify is an event-plane consumer. It subscribes to the `crm` and `prompts`
feeds and sends best-effort ntfy.sh pushes in reaction to contact creation and
prompt run outcomes. The MCP `send` tool is the proactive path: a connected
agent can push a notification to the owner's device on demand. The chassis
`health` and `reflection` tools are served alongside `send`.

## Planes

- **North/south (external, owner-facing).** Public traffic reaches notify through
  `/srv/notify/`. The exposed app surfaces are `GET /` for the human landing
  page, `GET /static/...` for landing assets, `POST /mcp` for MCP, and the
  unauthenticated RFC 9728 protected resource metadata document. notify is not a
  producer and serves no `/feed` endpoint.
- **East/west (internal, service-to-service).** The appkit chassis runs the
  configured `appkit.Spec.Consumers` entries. Feed URLs and ports are resolved
  through `registry` (`registry.MustPort`, `registry.BaseURL`) using the
  per-source environment convention `NOTIFY_<SRC>_FEED_URL` and
  `NOTIFY_<SRC>_FROM`.

## Composition Root

`cmd/notify/main.go` builds an `appkit.Spec` for the binary:

- `App: "notify"`, `Mount: "/srv/notify/"`, and `Port:
  registry.MustPort("notify")`.
- `MCP: true` and `WWW: true`.
- `Consumers` contains one `crm` consumer using `push.Subscription()` and one
  `prompts` consumer using `push.PromptsSubscriptions()`.
- `Migrations: db.FS`, so the embedded migration set is handed to appkit.
- `Handlers` mounts the landing page through `r.WWW()` at `GET /{$}` and mounts
  `POST /mcp` through `r.RequireIdentity(handler)`.

## Behavior

- **Engine, not hand-rolled.** The SSE client, reconnect/backoff loop, durable
  per-upstream cursor, and connect-time resync behavior live in
  `eventplane/consumer`. notify supplies subscriptions and handlers.
- **Best-effort delivery.** `internal/push` maps events to ntfy POSTs. Contact
  creation uses `Title: New contact` with the contact display name as the body.
  Prompt run events use prompt-oriented notification copy. Push delivery is not a
  transactional leg of event consumption; cursor advancement is owned by the
  consumer engine.
- **Structural failures are loud.** Missing cursor storage or migration problems
  are deployment bugs and should fail the process. Transport failures to a
  producer are retried by the consumer engine.

## Secrets And Config

The ntfy topic and API key are deployment secrets (`NTFY_TOPIC`,
`NTFY_API_KEY`). They reach the process only through the environment. The ntfy
base URL (`NOTIFY_NTFY_BASE_URL`, default `https://ntfy.sh`) is plain config, so
tests point it at a mock server. Do not read, log, or commit secret values.

Source-specific consumer config follows the chassis convention:
`NOTIFY_<SRC>_FEED_URL` selects the producer feed URL for a source and
`NOTIFY_<SRC>_FROM` selects the first-consumption position for a source.

## Layout

- **`internal/push`** — ntfy client code and the event consumer handlers.
  `Client.Send` is the best-effort path used by consumers, while
  `Client.Publish(ctx, Notification) error` is the synchronous path behind the
  MCP `send` verb.
- **`internal/db`** — the embedded migration set (`FS`) and the
  `migrations_feed_offset_test.go` byte-equality guard against
  `consumer.SchemaSQL`. SQLite open and migration execution are appkit concerns
  (`appkit/db`, `appkit.Spec.Migrations`).
- **`internal/mcp`** — `Instructions`, `Tools(client)`, and `NewHandler`, over
  the `appkit/mcp` chassis transport. Local code owns notify's tool definitions,
  not the JSON-RPC transport.
- **`share/www`** — `landing.html` and static assets served by the appkit WWW
  handler through `Spec.WWW` and `r.WWW()`.

## Tests

Run `go test ./...` from this directory. Migration tests guard that the embedded
set loads through `appkit/db` and that `002_feed_offset.sql` stays byte-identical
to `consumer.SchemaSQL`. Push tests use mock HTTP servers and never contact real
ntfy.sh. MCP tests drive the tool surface against a mock push client path and
assert validation, success, and upstream failure behavior.

## Manifest And Deploy

notify is one static appkit binary with the standard `serve`, `version`,
`manifest`, `migrate`, and `schema` verbs. `etc/manifest.env` declares:

```text
APP=notify
MOUNT=/srv/notify/
DEFAULT=false
PORT=3201
MCP=true
CONSUMES=crm,prompts
```

Shipping is the shared repo-root `bin/ship notify` flow. `notify/VERSION` is the
committed version source, and the on-box manifest is regenerated during deploy.
The local `bin/` scripts retained by notify are for systemd start/stop and secret
seeding.
