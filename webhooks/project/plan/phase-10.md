# Phase 10 — Adopt `registry`: own port by name + source-scan & drift guards

*Realizes design Decision 10. Depends only on the existing `cmd/webhooks/main.go`
composition root and the two static deploy artifacts. Independent of the WWW/MCP
conversions and sequenced first, like notify's registry phase. Covers
`R-0D7X-9EB6`, `R-0EFT-N61V`, `R-0FNQ-0XSK`. **Read D10 for the exact call sites
and rationale.***

webhooks stops hardcoding its loopback port and references itself **by name**
through the shared `registry` library, resolving **once at the composition root**.
This is behavior-preserving: `registry` already carries `webhooks=3006`, so every
resolved value is byte-identical to the literal it replaces.

**External precondition (assume satisfied; do NOT build it here).** The repo-root
`go.work` already carries `use ./registry` and the `registry` module exists and is
green (verified: `registry.Services` lists `{"webhooks", 3006, Core}`). No step in
this phase edits `../go.work`, `../registry/`, or any sibling module — the executor
runs from `webhooks/` and cannot reach outside it.

**What gets changed (all inside `webhooks/`):**

- **`webhooks/go.mod`** — add `require registry v0.0.0` and a committed
  `replace registry => ../registry`, mirroring the existing `appkit` /
  `eventplane` in-repo replace-siblings. This is the only build-graph change.
- **`webhooks/cmd/webhooks/main.go`** — import `registry` and change the
  `webhooksSpec()` `Port` field from the literal `3006` to
  `registry.MustPort("webhooks")`. Nothing else in `main.go` changes this phase.
- **`webhooks/cmd/webhooks/loopback_guard_test.go`** (new) — the **source-scan
  guard** (`// R-0EFT-N61V`): walk every `*.go` file under the `webhooks/` module
  root and fail if any file's source contains a bare loopback-address literal of
  the form `127.0.0.1:30` + two digits. Assemble the forbidden needle at runtime
  (e.g. `"127.0.0.1:" + "30"`), skip the guard's own filename, and scan real Go
  source (a prose port mention with no address literal does not trip it). Green
  after this phase's rewrites; red if a hardcoded loopback URL is reintroduced.
- **`webhooks/cmd/webhooks/main_test.go`** — the manifest byte-equality test
  (`TestManifestLibraryByteEqualsCommittedFile`) already emits from
  `webhooksSpec()`; because `Port` is now `registry.MustPort("webhooks")`, the
  emitted `PORT=3006` still byte-matches the committed `etc/manifest.env`. Add/keep
  the `// R-0FNQ-0XSK` tag on this test (manifest half of the drift guard). Keep
  every other assertion unchanged.
- **`webhooks/internal/web/nginx_test.go`** — replace the hardcoded
  `proxy_pass http://127.0.0.1:3006/;` and `…/static/;` assertions with ones built
  from `registry.BaseURL("webhooks")`: assert the fragment contains
  `"proxy_pass " + registry.BaseURL("webhooks") + "/;"` and
  `"proxy_pass " + registry.BaseURL("webhooks") + "/static/;"`, tagged
  `// R-0FNQ-0XSK` (nginx half). Keep the exact-match vs prefix distinction, the
  `auth_request /_session-authn` gate, the bearer-`/mcp` and `/feed` shield
  survival checks, and the catch-all check exactly as they are — only the port
  value they compare against becomes a `registry` call. (`internal/web` gains a
  `registry` import; the module requires it from `go.mod` above. This file is
  relocated to `cmd/webhooks` in phase 11 — leave it here for now.)
- **`webhooks/internal/e2e/e2e_test.go`** — rebuild the `loopback` value from
  `registry.BaseURL("webhooks")` (it is a package-level `const` today; make it a
  package `var` or inline the call at its use sites) so the last
  `127.0.0.1:30xx` string literal leaves the source. Keep the `frontDoor` literal
  and every assertion unchanged.
- Touch nothing else. Do **not** edit `etc/manifest.env` or `etc/nginx.conf`
  themselves — their literal `3006` stays; these tests now police it. **No schema
  change — no migration.**

**Done when:** the suite is green — `cd webhooks && go build ./...`,
`cd webhooks && go vet ./...`, `cd webhooks && gofmt -l .` (no output), and
`cd webhooks && go test ./...` all succeed with zero failures — and:

- R-0D7X-9EB6 — the composition root's listen port is
  `registry.MustPort("webhooks")`, not a `3006` integer literal; pinned together
  with the source-scan (R-0EFT-N61V) and the manifest drift guard (R-0FNQ-0XSK).
- R-0EFT-N61V — a guard test walks webhooks's `*.go` files (skipping itself,
  needle assembled at runtime) and asserts no bare `127.0.0.1:30xx`
  loopback-address literal remains; green after this phase, red if one is
  reintroduced. Confirm mechanically:
  `grep -rn "127.0.0.1:30" webhooks --include=*.go` returns only the guard's own
  runtime-assembled fragments (none as a whole literal).
- R-0FNQ-0XSK — the manifest byte-equality test emits with
  `registry.MustPort("webhooks")` and matches the committed `etc/manifest.env`, and
  the nginx tests assert the fragment's `proxy_pass` targets against
  `registry.BaseURL("webhooks")`; a `registry` value differing from the committed
  `3006` would fail them.
- `webhooks/go.mod` requires `registry` with a committed
  `replace registry => ../registry`.
