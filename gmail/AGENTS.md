# gmail

gmail is a loopback-only Gmail connector. nginx routes `/srv/gmail/` to the
service and remains the sole trust boundary for both served surfaces: a bearer-
gated MCP surface for agents and a session-cookie-gated human web landing page.

The service accepts nginx-provided identity headers as trusted input and runs no
token logic. Its domain work stays in the normal-mailbox MCP tool surface, the
Gmail History API poll daemon in `Workers`, and the `mail.*` event-plane
producer exposed through `Producer`/`Feed`. The `cmd/consent` one-time OAuth CLI
is unchanged.

`GMAIL_CLIENT_ID`, `GMAIL_CLIENT_SECRET`, `GMAIL_REFRESH_TOKEN`, and
`GMAIL_POLL_INTERVAL` reach the process only through the environment. appkit
does not read or log those values.

## Composition Root

`cmd/gmail/main.go` builds an `appkit.Spec` for the binary in `gmailSpec()`:

- `App: "gmail"`, `Mount: "/srv/gmail/"`, and `Port: registry.MustPort("gmail")`.
- `MCP: true` and `WWW: true`.
- `Migrations: db.FS`, so gmail's migration set is handed to appkit.
- `Events: mcp.Events` is the static published-event registry backing the
  outbox's Append-time validation and the reflection tool.
- `Handlers` builds the Gmail client and producer `Engine` over appkit's shared
  DB handle, mounts the landing page through `rt.WWW()` at `GET /{$}`, and
  mounts `POST /mcp` through `rt.RequireIdentity(handler)`.
- `Producer` attaches the outbox as the engine's `EventSink` so derived
  `mail.*` events append atomically with the cursor advance.
- `Workers` runs the engine's Gmail History API poll loop on the serve context.

## Surfaces

The human web surface is served from `share/www` through `Spec.WWW` and
`rt.WWW()`. nginx protects it with the session-cookie gate.

`internal/mcp` declares the ten Gmail tools with `Instructions`, `Tools(client)`,
and `NewHandler` over the `appkit/mcp` chassis transport. The `health` and
`reflection` tools are chassis-owned.

`internal/db` now holds only `FS`, the migration-set load guard, and the outbox
DDL guard. SQLite open and migration execution are appkit concerns.

## Local Work

Run package checks from this service directory:

```sh
go build ./...
go vet ./...
gofmt -l .
go test ./...
```
