# Phase 78 — Composition-root normalization: one inline `Spec` in `cmd/wiki/main.go`

*Realizes design Decision 54 (structural). Depends on Phase 77 (the inline Spec
carries `Port: registry.MustPort("wiki")`). Sequenced before the web (79) and MCP
(80) conversions so those edit **one** composition root, not two. A pure
relocation: the serve behavior — current `internal/web` render path and current
hand-rolled `internal/mcp` — is unchanged by this phase.*

Observable end state:

- `cmd/wiki/main.go`'s `main()` builds a **single** `appkit.Spec` literal (App,
  Mount, `Port: registry.MustPort("wiki")`, `MCP: true`, `WWW` unchanged at this
  phase — it is still `false`/absent until Phase 79 — `ManifestExtras`,
  `Migrations: db.FS`) with the current `buildSpec` serve wiring in its
  `Handlers` closure (config via `wiki.NewConfig(os.Getenv)` fail-loud inside the
  closure; read handle; LLM client + recorder; embedders; extractor/compiler;
  `wiki.NewService`; retrievers; asker; the web handler; the MCP handler) and its
  `Workers`, then calls `appkit.Main(spec)` directly. The shared
  `var svc *wiki.Service` is declared before the literal, assigned in `Handlers`,
  read in `Workers` (the forced capture — D54).
- `wiki.Spec()`, `wiki.Main()`, and the `internal/wiki` `serveCommand` are
  deleted; the `buildSpec` wrapper and the two-phase
  `spec := wiki.Spec(); if serveCommand {…}` `main()` are gone. `internal/wiki`
  keeps every domain export.
- `cmd/wiki/main_test.go` and `internal/wiki/wiki_test.go` re-point at the inline
  spec (or a small serve-spec test helper); no behavioral assertion changes.

**Done when:** the suite is green (`cd wiki && go build ./...`, `go vet ./...`,
`gofmt -l .` empty, `go test ./...`) and:

- `grep -n "func Spec\|func Main" wiki/internal/wiki/wiki.go` returns no matches;
- `grep -n "func buildSpec\|wiki\.Spec()\|wiki\.Main()" wiki/cmd/wiki/main.go`
  returns no matches;
- `grep -c "appkit.Spec{" wiki/cmd/wiki/main.go` reports exactly `1`;
- no behavioral test assertion changed (harness re-pointing only).
