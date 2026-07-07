# Phase 12 — Delete the chassis shims and true up the doctrine doc

*Realizes design Decision 14 (structural). Depends on phases 9–11 (the doctrine
doc states the fully converted truth); sequenced last so the reference shape lands
whole.*

Observable end state: `gmail/internal/db` retains only the embedded migration set
(`FS`) and the guard tests (`migrations_outbox_test.go` byte-equality against
`outbox.SchemaSQL`, `migrations_load_test.go`), with the `Open`/`Migrate` wrapper
functions removed. The one caller — `internal/gmail/sync_test.go`'s `openTestDB` —
calls appkit's db package directly (`migs, _ := appkitdb.LoadMigrations(db.FS,
"migrations")` then `appkitdb.Migrate(ctx, conn, migs)`), a harness plumbing
change only with no assertion change. `gmail/AGENTS.md` (via the `CLAUDE.md`
symlink invariant — one file, edit `AGENTS.md`) describes the converted service:
the `share/www` web surface served through `Spec.WWW`/`rt.WWW()`, `internal/mcp`
as the ten-tool declaration over `appkit/mcp` (health/reflection chassis-owned),
`internal/db` holding only the migrations embed + guards, the `appkit.Spec`
declared inline at `cmd/gmail/main.go` as `gmailSpec()`, and the loopback port
resolved via `registry.MustPort("gmail")` — with the falsified claims (embedded
assets/template, a local JSON-RPC MCP transport, the `internal/db` open/migrate
wrappers, the `internal/gmailapp` composition, a hardcoded port) purged, no
archaeology. The standing facts stay: loopback-only Gmail connector, nginx the
sole trust boundary, no token logic, the History-API poll daemon (`Workers`), the
`mail.*` producer (`Producer`/`Feed`), the `cmd/consent` one-time OAuth CLI, and
that the three `GMAIL_*` secrets + `GMAIL_POLL_INTERVAL` reach the process only
via the environment and are never read or logged by appkit.

**Done when:** the suite is green — `cd gmail && go build ./...`,
`cd gmail && go vet ./...`, `cd gmail && gofmt -l .` (no output), and
`cd gmail && go test ./...` all succeed with zero failures — and:

- `grep -n "func Open\|func Migrate" gmail/internal/db/db.go` returns no matches,
  while `grep -n "go:embed migrations" gmail/internal/db/db.go` still matches and
  `gmail/internal/db/migrations_outbox_test.go` still exists;
- `grep -rniE "go:embed|embedded (asset|template|landing)|json-rpc|jsonRPC|internal/gmailapp" gmail/AGENTS.md`
  returns no matches, and
  `grep -cE "share/www|registry|appkit/mcp" gmail/AGENTS.md` reports at least 1;
- `grep -rn "db\.Open\|db\.Migrate" gmail --include=*.go` returns no matches
  (the `sync_test.go` harness now calls `appkitdb` directly).
