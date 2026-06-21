# Phase 10 — MCP surface conformance: the full eight-verb surface, correctly named and wired

*Realizes design Decision 10 (the MCP tool surface `internal/mcp` + identity). Depends on Phase 9 (the JSON-RPC transport + the partial surface it shipped), Phase 7 (`wiki.Service.Ingest`/`JobStatus`, `ClaimStore.ListBySubject`, `PageStore.Get`, and a subjects registry-list read), and Phase 8 (`ask.Asker`).*

Phase 9 was marked done but did **not** realize D10: it shipped four verbs with
non-conformant names (`ingest_text`, `job_status` — D10 names them `ingest` and
`status` and explicitly rejects `ingest_text`), hardcoded an `ikigenba_wiki_`
prefix into each tool name (D10 requires `toolPrefix=""`; the dashboard brands
the *server* `ikigenba_wiki`, so the live surface came through double-prefixed as
`mcp__ikigenba_wiki__ikigenba_wiki_health`), omitted four verbs (`subjects`,
`claims`, `page`, `reflection`), and was never wired into the composition root
(`NewHandler` received no domain services), so only `health` was live. This phase
brings the surface into exact conformance with D10.

**What gets built (the observable end state):**

- `internal/mcp/` exposes **exactly** the eight D10 verbs, **bare** (`toolPrefix=""`,
  crm pattern): `ingest`, `status`, `ask`, `subjects`, `claims`, `page`, `health`,
  `reflection`. The Phase-9 names `ingest_text`/`job_status` and the hardcoded
  per-tool `ikigenba_wiki_` prefix are gone.
- The four missing verbs are implemented and dispatched: `subjects` → registry
  list filtered by type + name-substring; `claims` → `ClaimStore.ListBySubject`;
  `page` → `PageStore.Get` (page-by-subject); `reflection` → `{publishes:[],
  subscribes:[]}` (empty, for free — the Spec's event fields are all empty).
  Not-found (`status`/`page`/`claims` for an unknown id) is a clean result, never
  a 500/crash.
- Supporting read: the subjects registry-list read D10 requires (filter by type +
  name-substring, with limit) is added to the `internal/wiki` store surface it
  belongs to (absent after Phase 7).
- The composition root `cmd/wiki/main.go` constructs the domain services
  (`wiki.Service`, `ask.Asker`, the stores) and passes them to
  `mcp.NewHandler(...)`, mounted `rt.RequireIdentity(...)`, so **all eight verbs
  are live** — not the health-only collapse Phase 9 left.

**Done when:**

- R-MUQ4-K1JS — `tools/list` returns exactly `{ingest, status, ask, subjects,
  claims, page, health, reflection}` (bare names) with their input schemas — no
  more, no fewer — asserted as an **exact set**, not a subset/contains check.
- R-MVY0-XTAH — `reflection` returns `{publishes:[], subscribes:[]}`.
- R-MX5X-BL16 — `health` renders the envelope reporting the service up, with
  version and service name, behind identity.
- R-MYDT-PCRV — `status`/`page`/`claims` for an unknown id return a clean
  not-found result, not an error or crash.
- R-MZLQ-34IK — `POST /mcp` is mounted behind `RequireIdentity`, and `ingest`
  attributes `owner` from `X-Owner-Email`.
- A composition-root assertion proves the **wired** handler (as built in
  `cmd/wiki`) exposes all eight verbs — catching the "constructed with no domain
  services" regression that left Phase 9 health-only.
- The suite is green (`go build/vet`, `gofmt -l`, `go test ./...`,
  `bin/check-migrations wiki`).
