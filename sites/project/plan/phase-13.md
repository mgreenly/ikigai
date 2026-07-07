# Phase 13 — Delete the `internal/db` shim, normalize the composition root, and true up the doctrine doc

*Realizes design Decision 14 (structural). Depends on Phases 11–12 (the doctrine
doc states the fully converted truth, and the `Handlers` closure being relocated
carries the D12 landing render + D13 `POST /mcp` wiring); sequenced last so the
reference shape lands whole.*

Observable end state: `sites/internal/db` retains only the embedded migration set
(`FS`) and its load guard (`migrations_load_test.go`), with the `Open`/`Migrate`
wrapper functions removed from `fs.go` and its now-unused imports
(`context`, `database/sql`, `appkit/db`, the `modernc.org/sqlite` blank import)
dropped — only `embed` remains. The three domain test harnesses
(`internal/mcp/tools_test.go`'s `newTestHandler`, `internal/sites/store_test.go`,
`internal/sites/publish_test.go`) call `appkit/db` directly via a local helper
apiece (`appkitdb.Open` + `appkitdb.LoadMigrations(db.FS, "migrations")` +
`appkitdb.Migrate`), with no test assertion changes — harness plumbing only.

The composition root is normalized to the reference shape: the `Handlers` closure
moves **inside** `sitesSpec()` so it returns a fully-formed `Spec`, and `main()`
shrinks to `appkit.Main(sitesSpec())` — no post-construction `spec.Handlers = …`
mutation remains. The `sitesSpec()` builder is **kept** (its
manifest/registry/WWW/MCP callers in `cmd/sites/main_test.go` are unchanged); the
relocated closure body (landing render, `POST /mcp` mount, layout/store/mirror
wiring) is byte-identical — a pure relocation, no behavior change.

`sites/AGENTS.md` (via the `CLAUDE.md` symlink invariant — one file, edit
`AGENTS.md`), if present, describes the converted service: the `share/www` web
surface through `Spec.WWW`/`rt.WWW()`, `internal/mcp` as the fourteen-domain-tool
table over `appkit/mcp` with chassis-supplied `health`/`reflection`, and
`internal/db` as the embedded migration set only — with the falsified claims
(embedded landing/`internal/web`, the `internal/mcp` JSON-RPC transport, the
`internal/db` `Open`/`Migrate` wrappers, hardcoded ports) purged, no archaeology.

**Done when:** the suite is green — `cd sites && go build ./...`,
`cd sites && go vet ./...`, `cd sites && gofmt -l .` (no output), and
`cd sites && go test ./...` all succeed with zero failures — and:

- `grep -n "func Open\|func Migrate" sites/internal/db/*.go` returns no matches,
  while `grep -n "go:embed migrations" sites/internal/db/fs.go` still matches and
  `sites/internal/db/migrations_load_test.go` still exists;
- `grep -rn "db.Open\|db.Migrate" sites --include=*.go | grep -v internal/db/`
  returns no matches (every domain test harness moved to `appkit/db` directly);
- `grep -nE "\.Handlers\s*=|\.Workers\s*=" sites/cmd/sites/main.go` returns no
  matches (no post-construction field mutation), while
  `grep -n "func sitesSpec()" sites/cmd/sites/main.go` still matches and
  `grep -n "Handlers:" sites/cmd/sites/main.go` matches (the closure relocated
  into the `sitesSpec()` literal); `main()`'s body is `appkit.Main(sitesSpec())`;
- if `sites/AGENTS.md` exists:
  `grep -n "go:embed\|internal/web\|StaticHandler\|LandingHandler\|JSON-RPC" sites/AGENTS.md`
  returns no matches and `grep -c "share/www\|appkit/mcp" sites/AGENTS.md` is ≥ 1
  (if `sites/AGENTS.md` does not exist, these two checks are vacuously satisfied
  and the phase adds no doc — the code-side truth carries).
