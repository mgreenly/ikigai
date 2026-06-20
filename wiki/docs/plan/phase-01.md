# Phase 1 — Module wiring + service skeleton that builds prod-shaped and serves

*Realizes design Decision 1 (agentkit dependency wiring) and 2 (service skeleton: package layout, Spec wiring, config/secret composition root). Depends on no earlier phase.*

Stand the `module wiki` up so it compiles, passes a production-shaped build, and
serves on the appkit chassis — failing loud at startup when the secret is absent.

**What gets built (the observable end state):**

- `wiki/go.mod` declares `module wiki` (Go 1.26) with a versioned
  `require github.com/ikigenba/agentkit` and **no `replace`** for it; the only
  `replace` directives are the in-repo `appkit => ../appkit` and
  `eventplane => ../eventplane` siblings. The root `go.work` carries the
  local-dev `replace github.com/ikigenba/agentkit => /home/mgreenly/projects/agentkit`
  and is switched from `use ./wiki.bak` to `use ./wiki`.
- The package directories from D2's layout exist: `cmd/wiki/`, `internal/db/`,
  `internal/wiki/`, `internal/llm/`, `internal/extract/`, `internal/compile/`,
  `internal/retrieve/`, `internal/ask/`, `internal/worker/`, `internal/mcp/`,
  `internal/ids/`. `internal/ids` is copied verbatim from crm (ULID minting).
- `cmd/wiki/main.go` is the composition root: it builds a `wiki.Config`, reads
  `ANTHROPIC_API_KEY` once via `getenv` and **fails loud at startup if it is
  unset or empty** (crash, never serve degraded), constructs the shared agentkit
  Provider and an `internal/llm.Client` shell holding it, and declares
  `appkit.Spec{App:"wiki", Mount:"/srv/wiki/", Port:3006, MCP:true, ...}` with
  all event-plane fields empty and `ManifestExtras` for the non-secret knobs
  (model id, worker concurrency, search default/cap). Secret injection is wired
  via `wiki/.envrc` (`source_up` + `export ANTHROPIC_API_KEY="$(cat ~/.secrets/ANTHROPIC_API_KEY)"`).
- So the skeleton compiles and serves *now*, the wired `mcp.NewHandler` and
  `worker.Run` are **minimal real placeholders** — an MCP handler answering an
  empty `tools/list` and a worker that blocks on `ctx` until cancelled. Phase 07
  (worker) and Phase 09 (MCP) replace these placeholders with the real
  implementations; this is the incremental growth of `main.go`, not a rewrite of
  this phase.

**Done when:**

- R-MWBI-4JY7 — a test/check confirms `wiki/go.mod` has a versioned
  `require github.com/ikigenba/agentkit` and no `replace` for it (only the
  appkit/eventplane siblings).
- R-MV3L-QS7I — a production-shaped build (`GOWORK=off`, no `go.work` in scope,
  the local `~/projects/agentkit` path absent) compiles, resolving agentkit at
  its pinned tag from the module proxy.
- R-6RVX-P1IG — starting `serve` with `ANTHROPIC_API_KEY` unset/empty fails loud
  at startup (non-zero exit / startup error) rather than starting the HTTP server.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .`,
  `go test ./...`, `bin/check-migrations wiki`).
