# Phase 77 — Adopt `registry` for wiki's own port

*Realizes design Decision 51. Independent of the other open conversion phases
(78–81) and sequenced first, like notify's registry phase; depends only on the
committed in-repo `registry` module (consumed through a `replace` sibling).*

Observable end state:

- `wiki/go.mod` has `require registry v0.0.0` and `replace registry =>
  ../registry` (the exact form `notify/go.mod` uses).
- wiki's Spec resolves `Port: registry.MustPort("wiki")` (which returns `3001`);
  the `Port = 3001` const is removed from `internal/wiki/wiki.go`. The emitted
  manifest `PORT` stays `3001` byte-identical.
- No `127.0.0.1:30xx` and no bare `3001` loopback-port literal remains in wiki's
  non-test Go source.
- The port assertion (`internal/wiki/wiki_test.go`) and the manifest
  byte-equality test (`cmd/wiki/main_test.go`) compare against
  `registry.MustPort("wiki")`, so a registry renumber fails a wiki test.

The nginx fragment (`etc/nginx.conf`) and the committed `etc/manifest.env` keep
their literal `3001` — they are shipped verbatim config, not Go source, and are
out of scope.

**Done when:** the suite is green — `cd wiki && go build ./...`,
`cd wiki && go vet ./...`, `cd wiki && gofmt -l .` (no output), and
`cd wiki && go test ./...` all succeed — and these ids are covered by
clearly-named tests:

- **R-JDBC-V0EV** — a test asserts `Spec.Port == registry.MustPort("wiki")`
  (the value `3001`, resolved through the registry, not a literal).
- **R-JEJ9-8S5K** — a source-scan guard asserts wiki's non-test Go source
  contains no `127.0.0.1:30` occurrence and no bare `3001` port literal.
- **R-JFR5-MJW9** — the manifest byte-equality test (`Spec` → `manifest.Emit`
  vs committed `etc/manifest.env`) passes with the port sourced from
  `registry.MustPort("wiki")`.

and these scoped greps hold:

- `grep -n "registry" wiki/go.mod` shows both the `require` and the `replace`.
- `grep -rn "127.0.0.1:30\|[^0-9]3001[^0-9]" wiki --include=*.go | grep -v _test.go | grep -v /project/`
  returns nothing.
