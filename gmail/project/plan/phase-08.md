# Phase 8 — Prove no loopback-port literal survives, and guard the deploy artifacts against registry drift

*Realizes design Decision 12 (source-scan guard + deploy-artifact drift guard).
Depends on phase 7 (the `registry` adoption must already be wired, and `go.mod`
must already require `registry`). Covers `R-9RMC-Y6QU`, `R-9SU9-BYHJ`. **Read D12
for the exact guard shapes and rationale.***

Phase 7 removed the own-port integer literal; this phase **enforces** that no bare
loopback-address string literal survives, and turns the two static deploy
artifacts that must still carry the literal port (`etc/manifest.env`,
`etc/nginx.conf`) into drift-caught, not drift-prone — their gmail tests now derive
the expected port from `registry`.

**What gets changed (tests only — all inside `gmail/`):**

- **Source-scan guard (`R-9RMC-Y6QU`)** — add a genuinely-asserting test (a small
  guard file under `cmd/gmail`, e.g. `loopback_guard_test.go`) tagged
  `// R-9RMC-Y6QU` that:
  - walks every `*.go` file under the `gmail/` module root and fails if any
    file's source contains a bare loopback-address literal of the form
    `127.0.0.1:3` + three digits (the `3000–3999` suite range, which covers
    gmail's own connector-range `3202`);
  - **assembles the forbidden needle at runtime** (e.g. `"127.0.0.1:" + "3"`)
    rather than embedding the full literal, and **skips its own filename**, so the
    guard can never match itself;
  - passes cleanly once the nginx-test literals are re-pointed (below) and would
    go red if a hardcoded loopback URL like `"http://127.0.0.1:3202/feed"` were
    reintroduced. (Note: dynamic forms like `"127.0.0.1:0"` and
    `"127.0.0.1:%d"` in `cmd/consent` and the boot-health test have no `3xxx`
    literal and do not trip it.)
- **Manifest drift guard (`R-9SU9-BYHJ`, part 1)** — the manifest byte-equality
  test in `cmd/gmail/main_test.go` emits with the D11-resolved `spec.Port` (==
  `registry.MustPort("gmail")`) and compares to the committed `etc/manifest.env`.
  Keep every other assertion and every other `Fields` value unchanged; the emitted
  `PORT=3202` still byte-matches the committed file today. If a future `registry`
  renumber changed gmail's port, `manifest.Emit` would produce a different `PORT`
  line and the byte-equality assertion would fail.
- **nginx drift guard (`R-9SU9-BYHJ`, part 2)** — in
  `gmail/internal/web/nginx_test.go`, replace the hardcoded
  `proxy_pass http://127.0.0.1:3202/;` and `proxy_pass http://127.0.0.1:3202/static/;`
  assertions with ones built from `registry.BaseURL("gmail")`: assert the fragment
  contains `"proxy_pass " + registry.BaseURL("gmail") + "/;"` (exact-match landing
  location, `R-NGNX-7F1G`) and `"proxy_pass " + registry.BaseURL("gmail") +
  "/static/;"` (static location, `R-41ZW-HBBA`). Keep the exact-match vs prefix
  distinction, the `auth_request /_session-authn` gate assertions, and the
  PRM/bearer/feed survival checks exactly as they are — only the port value they
  compare against becomes a `registry` call. (`internal/web` needs `registry` in
  scope; the module already requires it from phase 7. These nginx tests still live
  in `internal/web` at this phase; phase 10's WWW conversion relocates them to
  `cmd/gmail` unchanged.)
- Touch nothing else. Do **not** edit `etc/manifest.env` or `etc/nginx.conf`
  themselves — their literal `3202` stays; these tests now police it. **No schema
  change — no migration.**

**Done when:**

- R-9RMC-Y6QU — a guard test walks gmail's `*.go` files (skipping itself, needle
  assembled at runtime) and asserts no bare `127.0.0.1:3xxx` loopback-address
  literal remains; it is green after the nginx-test re-point and goes red if one
  is reintroduced.
- R-9SU9-BYHJ — the manifest byte-equality test emits with
  `registry.MustPort("gmail")` and matches the committed `etc/manifest.env`, and
  the nginx tests assert the fragment's `proxy_pass` targets against
  `registry.BaseURL("gmail")`; a `registry` value differing from the committed
  `3202` would fail them (the intended drift alarm).
- No bare `127.0.0.1:3xxx` string literal remains in gmail's Go source.
- The suite is green: `cd gmail && go build ./...`, `cd gmail && go vet ./...`,
  `cd gmail && gofmt -l .` (prints nothing), `cd gmail && go test ./...`, and
  `bin/check-migrations gmail`.
