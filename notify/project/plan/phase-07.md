# Phase 7 — Adopt `registry` at the composition root

*Realizes design Decision 9 (adopt `registry`; resolve own + peer loopback ports
by name at startup). Depends on the existing `cmd/notify/main.go` composition root
and `resolveConsumerCfg`. Covers `R-RGSP-4A1K`, `R-RGCF-4B2L`, `R-RGPF-4C3M`,
`R-RGEO-4D4N`. **Read D9 for the exact call sites and rationale.***

notify stops hardcoding loopback port literals and references itself and its peers
**by name** through the shared `registry` library, resolving **once at the
composition root**. This is behavior-preserving: `registry` already carries
notify's current values (`notify=3201`, `crm=3100`, `prompts=3002`), so every
resolved value is byte-identical to the literal it replaces. Env overrides
(`NOTIFY_PORT`, `CRM_FEED_URL`, `PROMPTS_FEED_URL`) are unchanged — `registry`
supplies only the *default* the override falls back to.

**External precondition (assume satisfied; do NOT build it here).** The repo-root
`go.work` carries `use ./registry` and the `registry` module exists and is green.
Both are owned outside `notify/` (repo root / the `registry` project). No step in
this phase edits `../go.work`, `../registry/`, or any sibling module — the executor
runs from `notify/` and cannot reach outside it.

**What gets changed (all inside `notify/`):**

- **`notify/go.mod`** — add `require registry v0.0.0` and a committed
  `replace registry => ../registry`, mirroring the existing `appkit` /
  `eventplane` in-repo replace-siblings. This is the only build-graph change.
- **`notify/cmd/notify/main.go`** — import `registry` and replace the three
  literals per D9:
  - the appkit `Spec.Port` value `3201` → `registry.MustPort("notify")`;
  - in `resolveConsumerCfg`, the `CRM_FEED_URL` fallback
    `"http://127.0.0.1:3100/feed"` → `registry.BaseURL("crm") + "/feed"`;
  - the `PROMPTS_FEED_URL` fallback `"http://127.0.0.1:3002/feed"` →
    `registry.BaseURL("prompts") + "/feed"`.

  Leave the `envOr(getenv, KEY, default)` structure, the ntfy secrets, and
  `NOTIFY_FROM` / `NOTIFY_NTFY_BASE_URL` exactly as they are — only the `default`
  argument of the three sites changes from a literal to a `registry` call.
- **`notify/cmd/notify/main_test.go`** — add / extend genuinely-asserting tests
  (tag each with its id):
  - `// R-RGCF-4B2L` — with `CRM_FEED_URL` unset, `resolveConsumerCfg(getenv)`
    returns `feedURL == registry.BaseURL("crm") + "/feed"` (assert it also equals
    `"http://127.0.0.1:3100/feed"`, and supply the required ntfy secrets via the
    `getenv` stub so `resolveConsumerCfg` does not fail loudly).
  - `// R-RGPF-4C3M` — with `PROMPTS_FEED_URL` unset,
    `promptsFeedURL == registry.BaseURL("prompts") + "/feed"`
    (== `"http://127.0.0.1:3002/feed"`).
  - `// R-RGEO-4D4N` — with `CRM_FEED_URL` and `PROMPTS_FEED_URL` each set to a
    sentinel URL, `resolveConsumerCfg` returns those sentinels (the override wins
    over the registry default).
  - `// R-RGSP-4A1K` — assert the composition root's port is
    `registry.MustPort("notify")` (== `3201`). If the Spec is not directly
    inspectable, this id is satisfied by the manifest drift guard in phase 08
    (`manifest.Emit` with `registry.MustPort("notify")` byte-matches the committed
    `etc/manifest.env`); in that case tag it there and note the delegation here.
- Touch nothing else. **No schema change — no migration.** Do not edit
  `etc/manifest.env` or `etc/nginx.conf` (phase 08 re-points their *tests* at
  `registry`; the files' literals stay).

**Done when:**

- R-RGSP-4A1K — the composition root's listen port is `registry.MustPort("notify")`,
  not a `3201` literal (asserted here or delegated to phase 08's manifest guard).
- R-RGCF-4B2L — a test proves the crm feed default is
  `registry.BaseURL("crm") + "/feed"` when `CRM_FEED_URL` is unset.
- R-RGPF-4C3M — a test proves the prompts feed default is
  `registry.BaseURL("prompts") + "/feed"` when `PROMPTS_FEED_URL` is unset.
- R-RGEO-4D4N — a test proves a set `CRM_FEED_URL` / `PROMPTS_FEED_URL` overrides
  the registry default.
- `notify/go.mod` requires `registry` with a committed `replace registry => ../registry`.
- The suite is green: `cd notify && go build ./...`, `cd notify && go vet ./...`,
  `cd notify && gofmt -l .` (prints nothing), `cd notify && go test ./...`, and
  `bin/check-migrations notify`.
