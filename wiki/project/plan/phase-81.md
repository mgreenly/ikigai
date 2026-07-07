# Phase 81 — Delete the `internal/db` shim and true up the docs

*Realizes design Decision 55 (structural). Depends on Phases 78–80 (the doc
truth-up states the fully converted shape). Sequenced last so the reference shape
lands whole.*

Observable end state:

- `wiki/internal/db/db.go` retains only the embedded migration set (`FS` via
  `//go:embed migrations/*.sql`), with the `Open`/`Migrate` wrapper functions
  removed; test harnesses that called them call appkit's db package directly (no
  assertion changes — harness plumbing only).
- `wiki/internal/db/open_read.go` is **kept unchanged** — `OpenRead` and `Path`
  are wiki's genuine second, query-only read handle (D17), not a chassis shim.
- `wiki/AGENTS.md` (via the `CLAUDE.md` symlink invariant — one file, edit
  `AGENTS.md`) states the converted truth: the read surface serves from
  `share/www` through `Spec.WWW`/`rt.WWW()` with `internal/web` as the router
  (not `//go:embed` assets), and `internal/mcp` is a tool table over the
  `appkit/mcp` transport (not a hand-rolled JSON-RPC handler) — no archaeology.
  (The design spine was trued up in the authoring pass.)

**Done when:** the suite is green — `cd wiki && go build ./...`,
`cd wiki && go vet ./...`, `cd wiki && gofmt -l .` (no output), and
`cd wiki && go test ./...` all succeed — and:

- `grep -n "func Open\|func Migrate" wiki/internal/db/db.go` returns no matches,
  while `grep -n "go:embed migrations" wiki/internal/db/db.go` still matches;
- `grep -n "func OpenRead\|func Path" wiki/internal/db/open_read.go` still
  matches (the read handle is retained);
- `grep -rn "go:embed" wiki/internal/web/` returns nothing;
- `grep -in "JSON-RPC\|writeJSONRPCError\|hand-rolled\|go:embed" wiki/AGENTS.md`
  returns nothing.
