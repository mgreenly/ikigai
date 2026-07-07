# Phase 7 — Adopt `registry`: resolve ledger's own port by name, and guard the port literals

*Realizes design Decision 9. Covers `R-4VDW-DRQH`, `R-4WLS-RJH6`, `R-4XTP-5B7V`,
`R-4Z1L-J2YK`. No dependency on the other conversion phases — sequenced first, like
notify. **Read D9 for the exact call sites and rationale.***

ledger stops hardcoding the loopback port literal `3101` and references itself
**by name** through the shared `registry` library, resolving **once at the
composition root**. This is behavior-preserving: `registry` already carries
ledger's current value (`ledger=3101`), so every resolved value is byte-identical
to the literal it replaces. ledger is a **producer** and resolves no peer feed
URLs, so this is only the own-port change plus its guards.

**External precondition (assume satisfied; do NOT build it here).** The repo-root
`go.work` carries `use ./registry` and the `registry` module exists and is green.
Both are owned outside `ledger/`. No step in this phase edits `../go.work`,
`../registry/`, `../bin/`, or any sibling module — the executor runs from `ledger/`
and cannot reach outside it.

**What gets changed (all inside `ledger/`):**

- **`ledger/go.mod`** — add `require registry v0.0.0` and a committed
  `replace registry => ../registry`, mirroring the existing `appkit` / `eventplane`
  in-repo replace-siblings. This is the only build-graph change.
- **`ledger/cmd/ledger/main.go`** — import `registry` and change the appkit
  `Spec.Port` value `3101` → `registry.MustPort("ledger")`. Touch nothing else in
  the Spec (Producer/Handlers hooks, `ManifestExtras`, outbox wiring all unchanged).
- **`ledger/cmd/ledger/main_test.go`** — in the manifest byte-equality test
  (currently `Port: 3101`), change the emitted field to
  `Port: registry.MustPort("ledger")`; keep every other `Fields` value and every
  other assertion unchanged (the emitted `PORT=3101` still byte-matches the
  committed `etc/manifest.env`). Tag this test `// R-4WLS-RJH6`. It also concretely
  proves `R-4VDW-DRQH` (the Spec's own port derives from `registry`); tag it
  `// R-4VDW-DRQH` as well and note the delegation (the inline Spec is not directly
  inspectable).
- **`ledger/internal/web/nginx_test.go`** — replace the hardcoded
  `proxy_pass http://127.0.0.1:3101/;` and `proxy_pass http://127.0.0.1:3101/static/;`
  expectations with ones built from `registry.BaseURL("ledger")`: assert the exact
  landing block contains `"proxy_pass " + registry.BaseURL("ledger") + "/;"` and the
  static block contains `"proxy_pass " + registry.BaseURL("ledger") + "/static/;"`.
  Keep the exact-match vs prefix distinction, the `auth_request /_session-authn`
  gates, and the PRM/bearer/`@ledger_authn_500` survival checks exactly as they are
  (the existing `R-NGNX-*` / `R-7GZI-1L4P` assertions stay). Add a
  `// R-4XTP-5B7V` tag on the registry-derived proxy-target assertions. (`internal/web`
  now imports `registry`; the module requires it from this phase. The file relocates
  to `cmd/ledger` in Phase 08 — leave that move to Phase 08.)
- **Source-scan guard (`R-4Z1L-J2YK`)** — add a genuinely-asserting test (a small
  guard file under `cmd/ledger`) tagged `// R-4Z1L-J2YK` that walks every `*.go`
  file under the `ledger/` module root and fails if any file's source contains a
  bare loopback-address literal of the form `127.0.0.1:30` + two digits. It
  **assembles the forbidden needle at runtime** (e.g. `"127.0.0.1:" + "30"`) and
  **skips its own filename**, so it can never match itself; it passes cleanly after
  the changes above (zero such literals remain) and goes red if a hardcoded loopback
  URL is reintroduced.
- Touch nothing else. Do **not** edit `etc/manifest.env` or `etc/nginx.conf`
  themselves — their literal `3101` stays; these tests now police it. **No schema
  change — no migration.**

**Done when:** the suite is green — `cd ledger && go build ./...`,
`cd ledger && go vet ./...`, `cd ledger && gofmt -l .` (no output), and
`cd ledger && go test ./...` all succeed with zero failures — and:

- R-4VDW-DRQH — the composition root's listen port is `registry.MustPort("ledger")`,
  proven by the manifest guard (delegated, tagged in `main_test.go`).
- R-4WLS-RJH6 — the manifest byte-equality test emits with
  `registry.MustPort("ledger")` and byte-matches the committed `etc/manifest.env`.
- R-4XTP-5B7V — the nginx fragment tests assert the exact-landing and static
  `proxy_pass` targets against `registry.BaseURL("ledger")`.
- R-4Z1L-J2YK — the source-scan guard (skipping itself, needle assembled at
  runtime) asserts no bare `127.0.0.1:30xx` literal remains in ledger's Go source.
- `ledger/go.mod` requires `registry` with a committed `replace registry => ../registry`.
- `grep -rn "127\.0\.0\.1:310" ledger --include=*.go | grep -v project/` returns no
  matches (the last Go-source loopback literals are gone; the guard file assembles
  its needle at runtime so it is not a match).
