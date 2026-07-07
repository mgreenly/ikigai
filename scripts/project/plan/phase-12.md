# Phase 12 — Delete the `internal/db` shim and confirm the doctrine

*Realizes design Decision 14 (structural). Touches `scripts/internal/db/db.go` and
the four test harnesses that called its wrappers, all inside `scripts/`. Depends on
Phases 09–11 (the reference shape lands whole); sequenced last. No schema change.*

Observable end state:

- `scripts/internal/db/db.go` retains only the embedded migration set:
  `//go:embed migrations/*.sql`, the `var FS = migrationsFS` export, and the
  `modernc.org/sqlite` blank import if still needed for the embed package — with the
  `Open` and `Migrate` wrapper functions (and their `appkit/db` + `context` +
  `database/sql` imports) **removed**. `migrations_load_test.go` (which already uses
  `appkitdb.LoadMigrations`) is unchanged and still present.
- The test harnesses that called the shim call appkit's db package directly, exactly
  as crm/notify harnesses do — `appkitdb.Open(path)`,
  `appkitdb.LoadMigrations(db.FS, "migrations")`, `appkitdb.Migrate(ctx, conn, migs)`:
  - `scripts/internal/runner/runner_test.go`
  - `scripts/internal/script/store_test.go`
  - `scripts/internal/mcp/tools_test.go` (already moved to `appkitdb` in Phase 11 —
    verify no `db.Open`/`db.Migrate` reference remains)
  **No test *assertion* changes — harness plumbing only.**

## Delete the wrappers

Remove `func Open(dbPath string) (*sql.DB, error)` and
`func Migrate(ctx context.Context, conn *sql.DB) error` from
`scripts/internal/db/db.go`, and prune the now-unused imports (`context`,
`database/sql`, `appkit/db`). Update the package doc comment to say the package
holds only the embedded migration set (appkit owns DB open + the migration run via
`Spec.Migrations`). Keep the `//go:embed` line and `FS`.

## Migrate the harnesses

In each of the three test files, replace:
```go
conn, err := db.Open(path)
// ...
if err := db.Migrate(ctx, conn); err != nil { ... }
```
with:
```go
conn, err := appkitdb.Open(path)          // import appkitdb "appkit/db"
// ...
migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
// ... (fatal on err)
if err := appkitdb.Migrate(ctx, conn, migs); err != nil { ... }
```
Keep `scripts/internal/db` imported only for `db.FS`. Assertions are untouched.

## Doctrine

scripts has **no root `AGENTS.md`/`CLAUDE.md`** (D5), so there is no service-prose
doctrine file to edit in this phase — the design-doc truth-up (the D1/D2/D3/D6/D8
substrate edits, the `design.md` Conventions/Layout/Scope/Testing edits, and the
regenerated `INDEX.md`) was authored with the design change set, not by the build
loop. This phase's doctrine step is therefore a **verification-only** confirmation:
no scripts prose still asserts a hand-rolled MCP transport, an embedded web surface,
a `db.Open`/`db.Migrate` shim, or hand-rolled consumer `Workers`.

## Done when

The suite is green (design *Conventions* commands, from `scripts/`, plus
`bin/check-migrations scripts` — no migration added) with zero failures, **and**:

- `grep -n "func Open\|func Migrate" scripts/internal/db/db.go` returns no matches,
  while `grep -n "go:embed migrations" scripts/internal/db/db.go` still matches and
  `scripts/internal/db/migrations_load_test.go` still exists.
- `grep -rn "db\.Open\|db\.Migrate" scripts --include=*.go` returns no matches (all
  harnesses call `appkitdb` directly).
- `grep -rn "internal/web\|StaticHandler\|LandingHandler\|writeJSONRPCError\|runConsumer" scripts/cmd scripts/internal --include=*.go`
  returns no matches (the converted shape leaves none of these behind).
