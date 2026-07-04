# Phase 08 — Adopt `registry`: resolve scripts' own port and peer addresses by name

*Realizes design Decision 10. Touches the composition root `cmd/scripts/main.go`
(own port, peer feed defaults, dropbox base), the two loopback-literal doc
comments in `internal/script/dropbox.go`, `scripts/go.mod` (add the `registry`
require + committed replace), and the tests that pin these
(`cmd/scripts/main_test.go`, `internal/web/web_test.go`,
`internal/web/nginx_test.go`) plus a new guardrail test. No schema, no
`internal/web` landing markup change, no observable-behavior change (ports, URLs,
and `etc/manifest.env` are byte-identical). Independent of all other phases
(orthogonal to the D1–D9 series). **Everything is inside `scripts/`.***

scripts hardcodes loopback port literals at its composition root: its **own**
port (`Port: 3003` in `scriptsSpec()` and the `3003` arg to `config.Resolve` in
`registerRoutes`), its **peer** feed URLs (the `feedDefaults` map:
cron/crm/ledger/dropbox/prompts → `http://127.0.0.1:30xx/feed`), and its
**dropbox content base** (`DROPBOX_BASE_URL` fallback `"http://127.0.0.1:3200"`,
echoed in `dropbox.go` comments). These duplicate numbers the suite writes in ~5
places each and drift silently — a renumber only surfaces at deploy. The shared
`registry` module (repo root, `module registry`) is now the one authoritative
`name → port` table. This phase makes scripts reference itself and its peers **by
name** and ask registry for the address, resolving **once at startup**, with the
existing env overrides kept as the override layer. The registry table carries the
current numbers verbatim (`scripts 3003`, `cron 3005`, `crm 3100`, `ledger 3101`,
`dropbox 3200`, `prompts 3002`), so this is **behavior-preserving**.

**External preconditions (assume satisfied — do NOT build; they are outside
`scripts/`):** the `registry` module itself already exists at `../registry`, and
the repo-root `go.work use ./registry` entry is repo-root-owned wiring (like the
existing `eventplane` entry). If a local `go build` cannot resolve `registry`,
that `go.work` entry is missing and is an operator/root task — **not** a change
this phase may make. This phase edits only files under `scripts/`.

## `scripts/go.mod` — add the registry replace-sibling

Add, mirroring the committed `appkit`/`eventplane` pattern already in the file:

- a `require registry v0.0.0` entry, and
- a `replace registry => ../registry` directive.

Keep the existing `require`/`replace` blocks and comments intact. After this,
`go build`/`go vet` resolve `registry` via the committed replace (deterministic
under `GOWORK=off`; local dev additionally uses the external `go.work` entry).

## `cmd/scripts/main.go` — resolve by name at the composition root

Add `"registry"` to the import block. Then:

- **Own port (two sites).** In `scriptsSpec()`:
  ```go
  Port: registry.MustPort("scripts"),
  ```
  and in `registerRoutes`, the `config.Resolve` call:
  ```go
  cfg, err := config.Resolve("scripts", "/srv/scripts/", registry.MustPort("scripts"), os.Getenv)
  ```
  `MustPort` (panics on an unknown name) is correct: `"scripts"` is a compile-time
  constant in the table, so a miss is a boot-time programming error, never a silent
  zero. The value is `3003`, so `manifest.Emit` still emits `PORT=3003` and the
  committed-manifest byte-equality test stays green.

- **Peer feed defaults.** Remove the package-level `feedDefaults` map and replace
  it with a pure helper:
  ```go
  // feedDefault is a source's loopback dev fallback: the registry-owned address
  // plus the /feed route. The event plane bypasses nginx, so this is a direct
  // 127.0.0.1 address; production overrides it via SCRIPTS_<SRC>_FEED_URL.
  func feedDefault(source string) string {
      return registry.BaseURL(source) + "/feed"
  }
  ```
  and change the `runConsumer` fallback to use it:
  ```go
  feedURL := config.EnvOr(os.Getenv, feedURLEnv(source), feedDefault(source))
  ```
  `runConsumer` runs once per upstream at startup (one `Workers` entry per source),
  so this resolves at boot and the value is captured in `consumer.Config` and
  reused by the loop — not re-looked-up per event.

- **Dropbox base.** Change the `DROPBOX_BASE_URL` fallback in `registerRoutes`:
  ```go
  dropboxBase := config.EnvOr(os.Getenv, "DROPBOX_BASE_URL", registry.BaseURL("dropbox"))
  svc.Fetcher = script.NewHTTPFetcher(dropboxBase)
  ```
  `registry.BaseURL("dropbox")` == `http://127.0.0.1:3200`, so the default is
  unchanged and the `DROPBOX_BASE_URL` override still wins.

- **Scrub literal-bearing comments.** Reword any `main.go` comment that still
  quotes a `127.0.0.1:30xx` literal so no bare loopback literal survives in
  source — in particular the self-chaining TODO comment near `sources` (which
  quotes `http://127.0.0.1:3003/feed`): reword it to name
  `registry.BaseURL("scripts") + "/feed"` instead of the literal. Keep the TODO's
  intent.

## `internal/script/dropbox.go` — reword the literal-bearing comments

Two doc comments quote the dropbox literal (`base string // DROPBOX_BASE_URL, e.g.
http://127.0.0.1:3200` and `base is DROPBOX_BASE_URL (default
http://127.0.0.1:3200), read at main's composition root`). Reword both to name
`registry.BaseURL("dropbox")` as the default rather than the bare literal. No code
change in this file — `httpFetcher`/`NewHTTPFetcher` still take the resolved `base`
string; only the comments change so the guardrail test (below) sees no literal.

## Update the tests that pin the old composition root

- **`internal/web/web_test.go` `TestCompositionRootAdoptsNewScriptsLayout`** (id
  `R-4LKF-FB23`, retained). Its required-substrings slice pins
  `` `cfg, err := config.Resolve("scripts", "/srv/scripts/", 3003, os.Getenv)` ``.
  Change that expected substring to
  `` `cfg, err := config.Resolve("scripts", "/srv/scripts/", registry.MustPort("scripts"), os.Getenv)` ``.
  The other pinned substrings (`rootDir := scriptsRuntimeRoot(cfg.GenerationPath)`,
  `runsDir := filepath.Join(rootDir, "runs")`, `recreateRunsDir(runsDir)`,
  `run := runner.New(store, rootDir, runTTL)`,
  `svc := script.NewService(store, runsDir, run)`) are unchanged. **This edit is
  mandatory** — without it the suite goes red, since the literal `3003` no longer
  appears in that call.

- **`internal/web/nginx_test.go`** — three assertions expect
  `proxy_pass http://127.0.0.1:3003/;` / `.../static/;` against the shipped
  `etc/nginx.conf`. Derive the expected port from the registry so the test pins the
  fragment to the authoritative table (anti-drift), instead of a literal: import
  `registry`, compute `port := registry.MustPort("scripts")`, and build the
  expected `proxy_pass http://127.0.0.1:<port>/;` (and `/static/;`) strings with
  `fmt.Sprintf` (or `strconv.Itoa`). Behavior is identical today (port `3003`);
  the value now comes from the table. **`etc/nginx.conf` itself is NOT edited** —
  it is a shipped deploy artifact (nginx/root layer), not Go source registry can
  reach; the test now guards it against a future renumber.

## Add the D10 verification coverage (new tests, id-tagged)

Put the new behavioral tests in `cmd/scripts/main_test.go` (package `main`, where
`scriptsSpec`/`feedDefault` live) and the source-walk guard where noted:

- **R-RGST-SELF** — `spec := scriptsSpec()`; assert `spec.Port ==
  registry.MustPort("scripts")` **and** `spec.Port == 3003`. Add a source
  assertion (read `main.go`) that it contains `registry.MustPort("scripts")` as
  the `config.Resolve` port argument and no bare `Port: 3003` literal. Tag the
  test `// R-RGST-SELF`.
- **R-RGST-PEER** — a table test over `feedDefault`: `feedDefault("cron") ==
  "http://127.0.0.1:3005/feed"`, `crm → :3100/feed`, `ledger → :3101/feed`,
  `dropbox → :3200/feed`, `prompts → :3002/feed`. Tag `// R-RGST-PEER`.
- **R-RGST-DBOX** — assert `registry.BaseURL("dropbox") ==
  "http://127.0.0.1:3200"`; plus a source assertion that `main.go` uses
  `registry.BaseURL("dropbox")` as the `DROPBOX_BASE_URL` `EnvOr` fallback and that
  neither `main.go` nor `dropbox.go` still contains a `"http://127.0.0.1:3200"`
  literal. Tag `// R-RGST-DBOX`.
- **R-RGST-NLIT** — the guardrail. A test that walks every non-`_test.go` `.go`
  file under the `scripts` module root (resolve it as the dir containing `go.mod`;
  from `cmd/scripts` that is `../..`) and fails if any file matches
  `127\.0\.0\.1:3\d\d\d` or contains a standalone `3003` token. Exclude `*_test.go`
  files (they legitimately reference ports) and do not descend into `../registry`
  or other sibling modules — walk only scripts's own tree. Tag `// R-RGST-NLIT`.
  This is the "no `30xx` literal remains" proof.
- **R-RGST-GMOD** — read `../../go.mod`; assert it contains a `registry` require
  and the exact directive `replace registry => ../registry`. (The build/vet going
  green is the behavioral half — an unresolved `registry` import cannot compile.)
  Tag `// R-RGST-GMOD`.

## Done when

The suite is green (per design *Conventions*: `cd scripts && go build ./...`,
`cd scripts && go vet ./...`, `cd scripts && gofmt -l .` (no output),
`cd scripts && go test ./...`, and `bin/check-migrations scripts` all succeed with
zero failures — note `check-migrations` is trivially green since this phase adds no
migration), **and** these ids are each covered by a clearly-named, genuinely-
asserting test:

- **R-RGST-SELF** — `scriptsSpec().Port == registry.MustPort("scripts") == 3003`,
  and `config.Resolve` takes the registry port, not a literal `3003`.
- **R-RGST-PEER** — `feedDefault(src)` == `registry.BaseURL(src)+"/feed"` for all
  five sources, matching the prior literals.
- **R-RGST-DBOX** — the `DROPBOX_BASE_URL` default is `registry.BaseURL("dropbox")`
  == `http://127.0.0.1:3200`, with the override preserved.
- **R-RGST-NLIT** — no `127.0.0.1:3xxx` URL and no bare `3003` remains in scripts'
  non-test Go source.
- **R-RGST-GMOD** — `scripts/go.mod` requires `registry` and carries
  `replace registry => ../registry`; the module builds green with it linked.

And, as regression guards (retained ids, must stay green): the committed-manifest
byte-equality test (`R-8IAN-FB87`) still passes (`PORT=3003` unchanged), and
`TestCompositionRootAdoptsNewScriptsLayout` (`R-4LKF-FB23`) passes with its updated
`config.Resolve` expected substring.
