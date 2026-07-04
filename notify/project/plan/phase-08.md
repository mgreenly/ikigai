# Phase 8 — Prove no loopback-port literal survives, and guard the deploy artifacts against registry drift

*Realizes design Decision 10 (source-scan guard + deploy-artifact drift guard).
Depends on phase 7 (the `registry` adoption must already be wired, and `go.mod`
must already require `registry`). Covers `R-RGNL-4E5P`, `R-RGDR-4F6Q`. **Read D10
for the exact guard shapes and rationale.***

Phase 7 removed the three Go-source loopback literals; this phase **enforces**
that they stay gone and turns the two static deploy artifacts that must still
carry the literal port (`etc/manifest.env`, `etc/nginx.conf`) into drift-caught,
not drift-prone — their notify tests now derive the expected port from `registry`.

**What gets changed (tests only — all inside `notify/`):**

- **Source-scan guard (`R-RGNL-4E5P`)** — add a genuinely-asserting test (a small
  guard file under `cmd/notify` or a dedicated `internal/` guard package) tagged
  `// R-RGNL-4E5P` that:
  - walks every `*.go` file under the `notify/` module root and fails if any
    file's source contains a bare loopback-address literal of the form
    `127.0.0.1:30` + two digits;
  - **assembles the forbidden needle at runtime** (e.g. `"127.0.0.1:" + "30"`)
    rather than embedding the full literal, and **skips its own filename**, so the
    guard can never match itself;
  - passes cleanly after phase 7 (zero such literals remain) and would go red if a
    hardcoded peer URL like `"http://127.0.0.1:3100/feed"` were reintroduced.
- **Manifest drift guard (`R-RGDR-4F6Q`, part 1)** — in
  `notify/cmd/notify/main_test.go`, change the manifest byte-equality test's
  emitted fields from `Port: 3201` to `Port: registry.MustPort("notify")`. Keep
  every other assertion and every other `Fields` value unchanged; the emitted
  `PORT=3201` still byte-matches the committed `etc/manifest.env` today.
- **nginx drift guard (`R-RGDR-4F6Q`, part 2)** — in
  `notify/internal/web/nginx_test.go`, replace the hardcoded
  `proxy_pass http://127.0.0.1:3201/` (and the `/static/` variant) assertions with
  ones built from `registry.BaseURL("notify")`: assert the fragment contains
  `"proxy_pass " + registry.BaseURL("notify") + "/"` and
  `"proxy_pass " + registry.BaseURL("notify") + "/static/"`. Keep the exact-match
  vs prefix distinction, the `auth_request /_session-authn` gate assertion, and the
  PRM/bearer-location survival checks exactly as they are — only the port value
  they compare against becomes a `registry` call. (`internal/web` will need
  `registry` in scope; the module already requires it from phase 7.)
- Touch nothing else. Do **not** edit `etc/manifest.env` or `etc/nginx.conf`
  themselves — their literal `3201` stays; these tests now police it. **No schema
  change — no migration.**

**Done when:**

- R-RGNL-4E5P — a guard test walks notify's `*.go` files (skipping itself, needle
  assembled at runtime) and asserts no bare `127.0.0.1:30xx` loopback-address
  literal remains; it is green after phase 7 and goes red if one is reintroduced.
- R-RGDR-4F6Q — the manifest byte-equality test emits with
  `registry.MustPort("notify")` and matches the committed `etc/manifest.env`, and
  the nginx tests assert the fragment's `proxy_pass` targets against
  `registry.BaseURL("notify")`; a `registry` value differing from the committed
  `3201` would fail them (the intended drift alarm).
- No bare `127.0.0.1:30xx` string literal and no `Port: 3201` integer literal
  remain in notify's Go source (the last were converted here).
- The suite is green: `cd notify && go build ./...`, `cd notify && go vet ./...`,
  `cd notify && gofmt -l .` (prints nothing), `cd notify && go test ./...`, and
  `bin/check-migrations notify`.
