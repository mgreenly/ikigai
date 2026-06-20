# Phase 9 — The MCP tool surface behind RequireIdentity

*Realizes design Decision 10 (the MCP tool surface `internal/mcp` + identity). Depends on Phase 7 (Service.Ingest/JobStatus + registry/claims/page reads) and Phase 8 (ask.Asker).*

Expose the product surface as MCP and replace Phase 1's placeholder handler with
the real eight-verb surface, gated by identity.

**What gets built (the observable end state):**

- `internal/mcp/`: the JSON-RPC 2.0 transport copied near-verbatim from crm
  (`initialize`, `notifications/initialized`, `tools/list`, `tools/call`; result
  helpers `toolResultText/Err/JSON`), the eight tools declared as hand-coded maps
  dispatched by a `switch`, and `NewHandler(...)`. The route is mounted in
  `cmd/wiki/main.go` as `rt.RequireIdentity(mcp.NewHandler(...))` — nginx is the
  trust boundary; wiki does no token logic. Tool verbs are unprefixed
  (`toolPrefix=""`).
- The eight verbs and their dispatch: `ingest` → `wiki.Service.Ingest` (owner from
  `appkit.IdentityFrom(ctx)` / `X-Owner-Email`); `status` → `JobStatus`; `ask` →
  `ask.Asker.Ask`; `subjects` → registry list filtered by type + name-substring;
  `claims` → claims-by-subject; `page` → page-by-subject; `health` → the appkit
  envelope; `reflection` → `{publishes:[], subscribes:[]}` (empty, for free, since
  the Spec's event fields are all empty). Not-found (`status`/`page`/`claims` for
  an unknown id) is a clean result, never a 500/crash.

**Done when:**

- R-MUQ4-K1JS — `tools/list` returns exactly `{ingest, status, ask, subjects,
  claims, page, health, reflection}` with their input schemas — no more, no fewer.
- R-MVY0-XTAH — `reflection` returns `{publishes:[], subscribes:[]}`.
- R-MX5X-BL16 — `health` renders the envelope reporting the service up, with
  version and service name, behind identity.
- R-MYDT-PCRV — `status`/`page`/`claims` for an unknown id return a clean
  not-found result, not an error or crash.
- R-MZLQ-34IK — `POST /mcp` is mounted behind `RequireIdentity`, and `ingest`
  attributes `owner` from `X-Owner-Email`.
- The MCP surface is driven in-process with a stubbed identity (`tools/list` +
  `tools/call`); the suite is green.
