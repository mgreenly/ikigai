# Phase 17 — Adopt the shared `registry` for all loopback addressing

*Realizes design Decision 14. Touches `prompts/go.mod` and `cmd/prompts/main.go`
(and scrubs a stale literal comment in `internal/prompt/dropbox.go`). Adds one
new dependency: the zero-dependency shared `registry` module via a committed
`replace`. Independent of all other open work; the only ordering inside the phase
is go.mod wiring before the `registry` import compiles.*

prompts hardcodes loopback port numbers in three places in `cmd/prompts/main.go`:
the peer `feedDefaults` map, its own `appkit.Spec{Port: 3002}`, and the
`DROPBOX_BASE_URL` fallback. This phase moves all three to resolve **by service
name** from the shared `registry` table at startup, so the number lives in exactly
one place (the registry) and can no longer drift silently. It is
**behavior-preserving**: the registry pins the same numbers prompts uses today, so
every resolved address is byte-identical.

**External preconditions (assumed satisfied — do NOT create or edit these).** The
`registry` module exists at `../registry` and the repo-root `go.work` has
`use ./registry`. Both live outside `prompts/` and are owned at the repo root; the
committed `replace` below is what makes the build correct under `GOWORK=off`
regardless. If `../registry` is somehow absent the build will fail loudly, which is
the correct signal, not something this phase works around.

## Steps

In **`prompts/go.mod`** — wire the module exactly like the existing `eventplane`
replace (a sibling source tree, never tagged):
- Add `registry v0.0.0` to the `require` block.
- Add `replace registry => ../registry`.
- No `go.sum` change is expected: `registry` is a zero-dependency leaf resolved
  through a local path replace.

In **`cmd/prompts/main.go`**:
- Add `import "registry"`.
- **Own port:** replace `Port: 3002` in the `appkit.Spec` literal with
  `Port: registry.MustPort("prompts")`. `MustPort` panics on an unknown name,
  which at the composition root is the correct loud failure.
- **Peer feed defaults:** replace the literal `feedDefaults` map with a value
  derived once (package init) from `registry.BaseURL` over `sources`:
  ```go
  // feedDefaults is each upstream's loopback dev fallback (A11), derived once from
  // the shared registry: registry owns the address, prompts appends the /feed path.
  var feedDefaults = func() map[string]string {
      m := make(map[string]string, len(sources))
      for _, src := range sources {
          m[src] = registry.BaseURL(src) + "/feed"
      }
      return m
  }()
  ```
  This keeps `feedDefaults` keyed exactly by `sources` (they can no longer drift)
  and leaves `runConsumer`'s override read unchanged:
  `config.EnvOr(os.Getenv, feedURLEnv(source), feedDefaults[source])` — so
  `PROMPTS_<SRC>_FEED_URL` still wins where set.
- **Dropbox base default:** replace
  `config.EnvOr(os.Getenv, "DROPBOX_BASE_URL", "http://127.0.0.1:3200")` with
  `config.EnvOr(os.Getenv, "DROPBOX_BASE_URL", registry.BaseURL("dropbox"))`. The
  `DROPBOX_BASE_URL` override keeps identical semantics.
- **Scrub the doc-comment literals:** the package doc / `feedDefaults` comment
  block still names `http://127.0.0.1:3002/feed` etc.; rewrite those comments so no
  `127.0.0.1:30xx` literal remains (say "prompts' own `/feed` via the registry"
  instead of quoting the number). Keep the A11/A12 semantics the comments explain.

In **`internal/prompt/dropbox.go`** — the two comments referencing
`http://127.0.0.1:3200` (the `base` field doc and the composition-root note) are
the only other non-test `127.0.0.1:30xx` occurrences; reword them to reference the
registry-sourced default (e.g. "default `registry.BaseURL("dropbox")`") so the
guard test is honest. No logic changes in this file.

## Tests to add

Add a small test file in `package main` (e.g. `cmd/prompts/registry_test.go`)
for the behavior ids, plus the source-guard walk for R-RG04-NLIT. Import
`registry` directly to assert the resolved values.

## Done when

The suite is green (per design *Conventions*: from the `prompts/` directory,
`go build ./...` compiles all packages without error, `gofmt -l .` emits no
output, and `go test ./...` passes — "the suite is green" means every test passes
and no race detector violations appear, `-race` implicit in CI) and these ids are
covered by clearly-named tests:

- **R-RG01-PORT** — a test asserts `registry.MustPort("prompts") == 3002` (the
  value now handed to `appkit.Spec.Port`), and that the composition root sources
  its port from `registry.MustPort("prompts")` rather than a literal (the literal
  `3002` port is gone, cross-checked by R-RG04-NLIT). *(unit test importing
  `registry`)*
- **R-RG02-FEED** — a test asserts that for every name in `sources`,
  `feedDefaults[name] == registry.BaseURL(name) + "/feed"`, and pins the concrete
  expected values: `cron`→`http://127.0.0.1:3005/feed`, `crm`→`…:3100/feed`,
  `ledger`→`…:3101/feed`, `dropbox`→`…:3200/feed`, `scripts`→`…:3003/feed`,
  `prompts`→`…:3002/feed`. Also asserts `feedDefaults` has exactly `len(sources)`
  entries (no drift from `sources`). *(unit test over the package-level
  `feedDefaults`)*
- **R-RG03-DBOX** — a test asserts that with `DROPBOX_BASE_URL` unset the dropbox
  import base default equals `registry.BaseURL("dropbox")` ==
  `"http://127.0.0.1:3200"`, and that a set `DROPBOX_BASE_URL` still overrides it.
  *(unit test exercising the `config.EnvOr` default; assert
  `registry.BaseURL("dropbox")` equals the expected origin with no trailing path)*
- **R-RG04-NLIT** — a guard test walks prompts's module `.go` files, **excluding**
  `_test.go`, and fails if any file matches `127\.0\.0\.1:30` or contains a bare
  own-port literal `3002` used as an address/port. Proves the registry table is the
  only place a prompts loopback port is written. *(source-walk test; the
  `internal/web/web_test.go` nginx-fixture literal is out of scope because it is a
  `_test.go` file asserting on committed `etc/nginx.conf`, per D14)*
