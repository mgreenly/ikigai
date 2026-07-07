# Phase 10 — Delete the `internal/db` shims and true up the doctrine header

*Realizes design Decision 12 (structural). Depends on Phases 06–09 (the doctrine
header states the fully converted truth); sequenced last so the reference shape
lands whole.*

Observable end state:

- `cron/internal/db/db.go` retains only the embedded migration set (`FS`/
  `migrationsFS`), with the `Open` and `Migrate` wrapper functions removed (and the
  now-unused `context`/`database/sql`/`appkit/db` imports dropped). The guard tests
  `migrations_load_test.go` and `migrations_outbox_test.go` stay.
- The test harnesses that called the wrappers call `appkit/db` directly:
  `internal/db/db_test.go` gains a local `migrateCron` helper over
  `appkitdb.LoadMigrations(FS, "migrations")` + `appkitdb.Migrate` and opens via
  `appkitdb.Open`; the cross-package harnesses in `internal/crontab`,
  `internal/event`, and `internal/tick` open via `appkitdb.Open` and migrate via
  `appkitdb.LoadMigrations` + `appkitdb.Migrate`. No test **assertion** changes —
  only open/migrate plumbing. (`internal/mcp` was already moved off the wrappers in
  Phase 08.)
- `cron/cmd/cron/main.go`'s `package main` doc-comment header states the converted
  truth: the `appkit.Spec` is declared inline in `cmd/cron/main.go`, the port
  resolves via `registry.MustPort("cron")`, the landing page is served from the
  on-disk `share/www` tree through the chassis (`Spec.WWW`/`rt.WWW()`), and the MCP
  surface is the `internal/mcp` tool table over the `appkit/mcp` transport. The
  dynamic `Publishes` provider, the tick `Producer`/`Workers`, "no token logic,"
  and the loopback/manifest facts remain. No archaeology, no "previously…".

**Done when:** the suite is green — `cd cron && go build ./...`,
`cd cron && go vet ./...`, `cd cron && gofmt -l .` (no output),
`cd cron && go test ./...`, and `bin/check-migrations cron` all succeed with zero
failures — and:

- `grep -n "func Open\|func Migrate" cron/internal/db/db.go` returns no matches,
  while `grep -n "go:embed migrations" cron/internal/db/db.go` still matches and
  both `cron/internal/db/migrations_outbox_test.go` and
  `cron/internal/db/migrations_load_test.go` still exist;
- `grep -rnE "\bdb\.Open|\bdb\.Migrate" cron --include=*.go` returns no matches
  (the harnesses call `appkitdb.*`, which the `\b` boundary excludes);
- `cron/cmd/cron/main.go`'s package-doc header mentions the `share/www` web
  surface and the `appkit/mcp` tool table, and `grep -rni "no UI" cron/` continues
  to find nothing.
