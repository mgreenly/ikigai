# Phase 10 — Delete the chassis shims and true up the doctrine doc

*Realizes design Decision 12 (structural). Depends on Phases 07–09 (the doctrine
doc states the fully converted truth); sequenced last so the reference shape lands
whole. **Read D12 for the exact remains and the falsified doc claims.***

Observable end state: `ledger/internal/db` retains only the embedded migration set
(`FS`) and the guard tests (`migrations_load_test.go`, `migrations_outbox_test.go`),
with the `Open`/`Migrate` wrapper functions removed. The three test harnesses that
built a schema through `db.Migrate` call appkit's db package directly with **no
test assertion changes** (harness plumbing only):

- `internal/db/db_test.go` — `Open` → `appkitdb.Open`, `Migrate` → a local
  two-line helper over `appkitdb.LoadMigrations(FS, "migrations")` +
  `appkitdb.Migrate` (the notify `migrateNotify` pattern); `TestOpenAndMigrate` and
  `TestMigrate_IsIdempotent` keep their assertions;
- `internal/ledger/ledger_test.go` (the `openDB` helper) and
  `internal/mcp/tools_test.go` (its handler-setup helper) — the
  `db.Migrate(ctx, conn)` call becomes the same `appkitdb.LoadMigrations` +
  `appkitdb.Migrate` two lines over `db.FS`; every domain/tool assertion untouched.

`ledger/AGENTS.md` (via the `CLAUDE.md` symlink invariant — one file, edit
`AGENTS.md`) describes the converted service: `internal/mcp` as the seven-domain-tool
declaration over `appkit/mcp` (not a local JSON-RPC transport); `internal/db` as the
embedded migration set + guards only (SQLite open and the runner are `appkit/db`);
the `share/www` web surface served through `Spec.WWW`/`rt.WWW()` (not an embedded
`internal/web`); the seven-verb domain surface with chassis `health`/`reflection`
(nine wire tools) in place of the "eight fixed verbs" framing; and the port resolved
via `registry.MustPort("ledger")` — with the falsified claims purged (the local MCP
transport, the DB open/runner, the non-existent `internal/server`/`internal/logging`
layout entries, the embedded-assets description), no archaeology.

**Done when:** the suite is green — `cd ledger && go build ./...`,
`cd ledger && go vet ./...`, `cd ledger && gofmt -l .` (no output), and
`cd ledger && go test ./...` all succeed with zero failures — and:

- `grep -n "func Open\|func Migrate" ledger/internal/db/db.go` returns no matches,
  while `grep -n "go:embed migrations" ledger/internal/db/db.go` still matches and
  both `ledger/internal/db/migrations_load_test.go` and
  `ledger/internal/db/migrations_outbox_test.go` still exist;
- `grep -n "internal/server\|internal/logging\|JSON-RPC" ledger/AGENTS.md` returns
  no matches, and `grep -c "share/www\|Spec.WWW\|appkit/mcp" ledger/AGENTS.md`
  reports at least 1.
