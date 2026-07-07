# Phase 21 — Delete the chassis shims and true up the doctrine doc

*Realizes design Decision 18 (structural). Depends on Phases 18–20 (the package
doc comment states the fully converted truth); sequenced last so the reference
shape lands whole. No `R-XXXX-XXXX` ids — the behaviors are pinned by the
existing domain/MCP/migration suites and the D15–D17 ids.*

`prompts/internal/db` still carries `Open`/`Migrate` wrappers that only delegate
to `appkit/db`; appkit owns DB opening and the migration run
(`Spec.Migrations: db.FS`). This phase deletes the wrappers, repoints the test
harnesses at `appkit/db` directly, and trues up the `cmd/prompts/main.go` package
doc comment (prompts' surface-posture doctrine — it has no `AGENTS.md`).

## Steps

In **`prompts/internal/db/db.go`**:

- Delete `func Open` and `func Migrate`. What remains is the `//go:embed
  migrations/*.sql var migrationsFS embed.FS` and `var FS = migrationsFS`.
- Remove the now-unused imports (`context`, `database/sql`, `appkit/db`, and the
  blank `modernc.org/sqlite` driver import — `appkit/db.Open` registers the
  driver now); keep `embed`.
- Update the package doc comment to drop the "thin `Open`/`Migrate` helper"
  language; state that the package holds only prompts' embedded migration set and
  its app-side migration guard tests (SQLite open + the forward-only runner are
  `appkit/db`).

In **`prompts/internal/db/db_test.go`**: delete `TestOpenAndMigrate`,
`TestMigrate_IsIdempotent`, and `TestMigrate_RefusesDowngrade` (they exercise the
deleted wrappers; appkit/db owns those tests). If the file is then empty, delete
it.

**Repoint the test harnesses** that stood up a schema through the wrappers to call
`appkit/db` directly — `appkitdb.Open(path)` and
`appkitdb.Migrate(ctx, conn, appkitdb.LoadMigrations(db.FS, "migrations"))`,
importing `appkitdb "appkit/db"` and keeping `prompts/internal/db` for `db.FS`.
**No test assertion changes** — harness plumbing only. Files:
`internal/consume/smoke_test.go`,
`internal/db/migrations_provider_model_aliases_test.go`,
`internal/db/redesign_migration_test.go`,
`internal/prompt/service_test.go`, `internal/prompt/outcome_test.go`,
`internal/prompt/store_test.go`, `internal/mcp/mcp_test.go`,
`internal/runner/runner_test.go`. (The app-side guard tests
`prompts_schema_test.go`, `migrations_feed_offset_test.go`,
`migrations_outbox_test.go` keep their assertions; adjust only their DB-stand-up
plumbing if they used the wrappers.)

In **`cmd/prompts/main.go`** — true up the package doc comment to the converted
truth (no archaeology), purging the falsified claims:

- Replace **"It is neither an event-plane producer nor a consumer — no `/feed`,
  no consumer loop, no background worker; …"** with the truth: prompts **is** an
  event-plane producer (`Feed: "/feed"`, emitting `run.succeeded`/`run.failed`
  via the outbox on the run's terminal write) **and** a consumer of six upstreams
  (`cron`, `crm`, `ledger`, `dropbox`, `scripts`, and its own feed for
  self-chaining) declared as `Spec.Consumers` and run by the chassis.
- Replace **"the bare MCP surface"** with the `appkit/mcp` tool table
  (`internal/mcp` declares the sixteen domain tools; the chassis serves the
  transport plus `health`/`reflection`).
- Restate the landing-page clause as the `share/www` surface served through the
  chassis `Spec.WWW` / `rt.WWW()` (still nginx-gated; in-process ungated).

Keep the "performs no token logic of its own" / "nginx is the trust boundary"
posture — it stays true.

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) with no test
assertion changes, and:

- `grep -n "func Open\|func Migrate" prompts/internal/db/db.go` returns no
  matches, while `grep -n "go:embed migrations" prompts/internal/db/db.go` still
  matches and `prompts/internal/db/prompts_schema_test.go` still exists;
- `grep -n "neither an event-plane producer nor a consumer" prompts/cmd/prompts/main.go`
  and `grep -n "the bare MCP surface" prompts/cmd/prompts/main.go` both return no
  matches, and `grep -c "Consumers" prompts/cmd/prompts/main.go` reports at least
  1;
- no caller references the deleted `prompts/internal/db` wrappers — proven by the
  green build itself: with `Open`/`Migrate` removed from the package, any
  remaining `db.Open`/`db.Migrate` call against `prompts/internal/db` fails to
  compile, so a green `go build ./...` is the deterministic guard.
