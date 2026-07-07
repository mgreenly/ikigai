# Phase 13 — Delete the `internal/db` shim, normalize the composition root, and true up the doctrine

*Realizes design Decision 13 (structural). Depends on Phases 10–12 (the composition
root states the fully converted truth); sequenced last so the reference chassis
shape lands whole. **Read D13 for the exact deletions and the normalized Spec.***

Observable end state:

- `webhooks/internal/db` retains only its legitimate content: the embedded
  migration set (`//go:embed migrations/*.sql` → exported `FS`), the domain
  `Store` (`store.go`), and the guards (`migrations_outbox_test.go`,
  `store_test.go`). The `Open` and `Migrate` wrapper functions and the blank
  `_ "modernc.org/sqlite"` driver import are removed from `internal/db/db.go`, and
  the package comment is trued up (embed set + domain store + outbox guard; no
  "Open"/"thin migrate helper" language).
- Every former `db.Open`/`db.Migrate` caller calls `appkit/db` directly
  (`appkitdb.Open(path)` + `appkitdb.LoadMigrations(db.FS, "migrations")` +
  `appkitdb.Migrate(ctx, conn, migs)`) with **no test assertion changes** — harness
  plumbing only: `internal/db/store_test.go` (in-package, uses the local `FS`),
  `internal/mcp/tools_test.go`, `internal/webhooks/events_test.go`,
  `internal/webhooks/secret_test.go`, `internal/webhooks/ingress_test.go`, and
  `internal/e2e/e2e_test.go`.
- `cmd/webhooks/main.go` is normalized to one fully-formed inline `appkit.Spec`
  returned by `webhooksSpec()`: the shared `var svc *webhooks.Service` moves inside
  `webhooksSpec()` (captured by both the `Handlers` and `Producer` closures), and
  the `spec := webhooksSpec(); spec.Handlers = …; spec.Producer = …`
  post-construction mutation in `main()` is gone — `main()` is just
  `appkit.Main(webhooksSpec())`. `Handlers` and `Producer` are Spec **fields**,
  not post-construction assignments. The `main.go` package doc comment is trued up:
  no claim of a local MCP JSON-RPC transport or embedded web assets; the surface is
  the `appkit/mcp` tool table (D12) and the `share/www` web surface (D11).
- No `webhooks/AGENTS.md` is authored — none exists to true up; the "docs state the
  converted truth" doctrine is discharged by the in-place D1/D6/D9 rewrites
  (phases 10–12) and this phase's `main.go` comment.

**Done when:** the suite is green — `cd webhooks && go build ./...`,
`cd webhooks && go vet ./...`, `cd webhooks && gofmt -l .` (no output), and
`cd webhooks && go test ./...` all succeed with zero failures — and:

- `grep -n "func Open\|func Migrate" webhooks/internal/db/db.go` returns no
  matches, while `grep -n "go:embed migrations" webhooks/internal/db/db.go` still
  matches, `grep -n "func NewStore" webhooks/internal/db/store.go` matches, and
  `webhooks/internal/db/migrations_outbox_test.go` still exists.
- `grep -rn "db.Open\|db.Migrate(" webhooks --include=*.go` returns no matches
  (all callers now use `appkitdb.Open`/`appkitdb.Migrate`).
- `grep -n "spec.Handlers =\|spec.Producer =" webhooks/cmd/webhooks/main.go`
  returns no matches, while `grep -n "Producer:" webhooks/cmd/webhooks/main.go`
  matches (the producer seam stays a Spec field) and `main()` contains a single
  `appkit.Main(webhooksSpec())` call.
- No test assertion changed — only harness DB-standup plumbing and composition
  shape moved.
