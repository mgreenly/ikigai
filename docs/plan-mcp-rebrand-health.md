# Execution plan — MCP rebrand (`ikigenba_*`) + `whoami` → `health`

Derived from `DECISIONS.md`. This is the **sequential** implementation plan: each
step is a self-contained unit a single subagent can execute end-to-end, and the
steps run **in order** — every step assumes its predecessors have landed and
compile/test green. No step is parallel-safe with another; the appkit foundation
(Step 1) is a hard prerequisite for all service work, and the service steps share
the helper pattern Step 1 establishes.

## Ground rules for every step

- Work from repo root; use relative paths; never `cd`.
- The local-dev build uses `go.work`; the **gate after every step** is
  `go build ./...` then `go test ./...` for the module(s) the step touched
  (each top-level dir is its own module — run the gate inside that module, e.g.
  `cd appkit && go test ./...` is forbidden by the no-`cd` rule, so use
  `go test appkit/...` / `go -C` is also `cd`-shaped — instead run
  `go test ./...` from within the workspace via the module path:
  `go test ./...` at root resolves the whole workspace). Prefer the
  workspace-wide `go build ./...` / `go test ./...` at repo root as the gate.
- HTTP route **paths are not branded** — `/srv/<svc>/...` and the internal
  `/health` path stay bare. Only **MCP tool names** carry the `ikigenba_` prefix.
- A step is "done" only when build + the touched tests are green. If a step
  cannot go green, stop and report — do not paper over with a skip.

---

## Step 1 — appkit foundation (the single envelope builder + `health` route + `Spec.Health`)

**Everything else depends on this. It must land first and alone.**

### 1a. Exported envelope builder (the one builder both transports render through)

In `appkit/server` add the single builder:

```go
// Envelope is the fixed health envelope rendered by BOTH the HTTP /health route
// and every service's ikigenba_<svc>_health MCP tool, so the two cannot diverge
// (DECISIONS §4). Required top-level keys are appkit-owned and identical for
// every service; per-service telemetry is namespaced under details, which is
// ALWAYS present (empty {} when a service supplies no reporter) so consumers
// never branch on it. Required keys are reserved — a reporter contributes only
// to details.
func Envelope(version, service string, details map[string]any) map[string]any {
    if details == nil {
        details = map[string]any{}
    }
    return map[string]any{
        "status":  "ok",
        "version": version,
        "service": service,
        "details": details,
    }
}
```

Re-export it from the `appkit` package (alongside the existing
`type Router = server.Router` / `var IdentityFrom = server.IdentityFrom` aliases)
so service MCP packages call `appkit.Envelope(...)` without importing
`appkit/server` directly:

```go
// in appkit/appkit.go
var Envelope = server.Envelope
```

### 1b. `Spec.Health` reporter hook

In `appkit/appkit.go`, add to `Spec` (place it next to `Producer`/`Workers`,
following the exact optional-hook pattern):

```go
// Health is the optional per-service telemetry reporter. When set, appkit calls
// it to populate the `details` object of the health envelope on BOTH the HTTP
// /health route and the ikigenba_<svc>_health MCP tool. Nil → details is {}.
// The reporter contributes ONLY to details; the required top-level keys
// (status/version/service) are appkit-owned and reserved (DECISIONS §3).
Health func(ctx context.Context) (map[string]any, error)
```

(Confirm `context` is already imported in appkit.go; it is used by `Workers`.)

### 1c. Thread version / service / reporter into `server.Options`

In `appkit/server/server.go`, add to `Options`:

```go
Version string // build-stamped version string for the health envelope (required for the standard table)
Service string // service name (spec.App) for the health envelope
Health  func(ctx context.Context) (map[string]any, error) // optional per-service details reporter
```

Carry `Version`/`Service`/`Health` onto the `appHandler` struct (it already
holds `logger`/`resourceID`/`authServer`) and into `New` where `a` is built.

Also expose them on the `Router` so the service can wire its MCP health tool from
the same source (mirrors the existing `ResourceID()`/`AuthServer()`/`DB()`
accessors):

```go
func (rt *Router) Version() string { return rt.app.version }
func (rt *Router) Service() string { return rt.app.service }
func (rt *Router) Health() func(context.Context) (map[string]any, error) { return rt.app.health }
```

### 1d. `GET /whoami` → ungated `GET /health`

In `appkit/server/handlers.go`: replace `handleWhoami` with `handleHealth`. It
runs the reporter (if any) and renders the shared envelope. **No identity** —
this is liveness, and it is ungated, so it must not read identity from context:

```go
// handleHealth is the ungated liveness route (DECISIONS §5): a 200 OK is the
// dashboard's "service up" signal, and it survives an auth outage because it
// joins PRM and /feed as an unauthenticated route. Body: {status,version,
// service,details} — NO identity. Renders through the shared server.Envelope so
// it cannot drift from the MCP health tool.
func (a *appHandler) handleHealth() http.HandlerFunc {
    return func(w http.ResponseWriter, r *http.Request) {
        details := map[string]any{}
        if a.health != nil {
            d, err := a.health(r.Context())
            if err != nil {
                // a reporter failure is a degraded signal, not a dead one: still
                // 200 (liveness), surface the failure inside details.
                details = map[string]any{"error": err.Error()}
            } else if d != nil {
                details = d
            }
        }
        writeJSON(w, http.StatusOK, Envelope(a.version, a.service, details))
    }
}
```

In `appkit/server/server.go` route table (the `!opts.Apex` branch): replace

```go
mux.Handle("GET /whoami", a.requireIdentityHeaders(a.handleWhoami()))
```

with the **ungated** health route (drop the `requireIdentityHeaders` wrapper —
this is the join-PRM-and-/feed decision, §5):

```go
mux.Handle("GET /health", a.handleHealth())
```

Update the package doc comment and the `// standard route table (PRM + whoami …)`
prose in server.go to say `health` instead of `whoami`, and note it is ungated.

### 1e. Pass the new Options from verbs.go

In `appkit/verbs.go` `runServe`, the `server.New(server.Options{…})` call gains:

```go
Version: versionString(),
Service: spec.App,
Health:  spec.Health,
```

(`versionString()` and `spec.App` are already in scope in `runServe`.)

### 1f. Update `appkit/server/server_test.go`

- The two existing `GET /whoami` cases (≈ lines 92, 116) become `GET /health`.
- The previously-gated-whoami test must flip: `/health` is now **ungated** —
  assert it returns 200 **without** identity headers.
- Add an assertion that the body is the envelope `{status:"ok", version, service,
  details:{}}` when no `Health` reporter is set (details present and empty).
- Add a case with an `Options.Health` reporter returning a non-empty map and
  assert it lands under `details` (and that it does NOT splat at the top level).
- Provide `Version`/`Service` in the test Options so the envelope has values.

**Gate:** `go build ./...` && `go test ./...` (root). Do not proceed until green.

---

## Steps 2–7 — per-service MCP rebrand + `whoami` → `health` (one step per service, in this order)

Each service is its **own step and its own subagent**. Run them **sequentially**
(crm → ledger → notify → wiki → ralph → dropbox) so the shared `tool()`/
`toolPrefix` pattern is applied identically and any pattern correction in the
first service propagates to the rest by example. Dropbox is **last** because it
also folds two tools into one (Step 7 specifics).

### The pattern every service step applies

Files: `<svc>/internal/mcp/tools.go` (+ any `*.go` in that package carrying tool
names in descriptions, e.g. `mcp.go`, `describe.go`) and the package's
`*_test.go`.

1. **Add the prefix seam — defined once per service** (DECISIONS §1):

   ```go
   // toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
   // ikigenba + the service name; HTTP route paths are NOT branded.
   const toolPrefix = "ikigenba_<svc>_"

   // tool returns the branded, fully-qualified MCP tool name. Used by BOTH
   // toolDescriptors and dispatchTool so the two sites cannot drift.
   func tool(verb string) string { return toolPrefix + verb }
   ```

   `<svc>` ∈ {crm, ledger, notify, wiki, ralph, dropbox}.

2. **Descriptor list** — change each `desc("<svc>_<verb>", …)` to
   `desc(tool("<verb>"), …)`. The `<svc>_whoami` descriptor becomes
   `tool("health")` (see §3 below for the description rewrite).

3. **Dispatch switch** — change each `case "<svc>_<verb>":` to
   `case tool("<verb>"):` so it matches the same branded name the descriptor
   advertised. (A Go `case` on a function call is legal — these are not constant
   cases.) `case "<svc>_whoami":` becomes `case tool("health"):`.

4. **`whoami` → `health`** (DECISIONS §2, §6): rename the tool verb from
   `whoami` to `health`. The MCP tool is **gated and carries identity**, so its
   payload is the shared envelope **plus** the two identity fields:
   `{status, version, service, owner_email, client_id, details}`. Replace the
   old `toolWhoami(id)` body with a health implementation that renders through
   the shared builder and then adds identity (and details from the reporter):

   ```go
   func (h *Handler) toolHealth(ctx context.Context, id Identity) (map[string]any, error) {
       details := map[string]any{}
       if h.health != nil {
           d, err := h.health(ctx)
           if err != nil {
               details = map[string]any{"error": err.Error()}
           } else if d != nil {
               details = d
           }
       }
       env := appkit.Envelope(h.version, h.service, details) // status/version/service/details
       env["owner_email"] = id.OwnerEmail
       env["client_id"] = id.ClientID
       return toolResultJSON(env)
   }
   ```

   The description rewrite for the health descriptor (replaces the old whoami
   "auth proof" text — the auth-chain proof now lives here, §6):

   > "Health + diagnostics for the <svc> service. Returns the fixed envelope
   > (status, version, service, details) plus the authenticated caller's identity
   > (owner_email, client_id) as established by the platform's auth gate — the
   > end-to-end auth-chain proof. Takes no inputs."

5. **Wire version/service/reporter into the MCP Handler.** Extend the service's
   `mcp.Handler` struct and `NewHandler` to accept the envelope inputs, and pass
   them from `<svc>/cmd/<svc>/main.go`'s `Handlers` hook using the new Router
   accessors:

   ```go
   // in <svc>/internal/mcp/mcp.go
   type Handler struct {
       svc     *<svc>.Service
       version string
       service string
       health  func(context.Context) (map[string]any, error)
   }
   func NewHandler(s *<svc>.Service, version, service string,
       health func(context.Context) (map[string]any, error)) *Handler { … }
   ```

   ```go
   // in <svc>/cmd/<svc>/main.go Handlers hook
   rt.Handle("POST /mcp", rt.RequireIdentity(
       mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())))
   ```

   For services that have **no telemetry yet** (crm, ledger, notify, wiki,
   ralph), `Spec.Health` stays nil, so `rt.Health()` is nil and `details` renders
   `{}` on both transports — correct per §3. Do **not** invent telemetry for them
   in this pass.

6. **In-description cross-references** (DECISIONS §1, third bullet): update every
   agent-facing mention of a sibling tool in a description/prose string to the new
   full name. Known sites to fix (grep each service for the bare names):
   - crm: `use crm_log` (tools.go saveDescription) → `use ikigenba_crm_log`.
   - wiki: `poll with wiki_job_status`, `findable via wiki_search`, `Unlike
     wiki_search`, `Prefer wiki_search … use wiki_ask` (tools.go + mcp.go) →
     branded names.
   - ledger: `call ledger_describe`, `like ledger_balance`, `ledger_balance +
     ledger_register` (tools.go + the `bad_root` message in mcp.go) → branded.
   - ralph: `ralph_whoami proves the auth chain` (describe.go) → rewrite to
     `ikigenba_ralph_health proves the auth chain` (also reflects whoami→health).
   - Re-grep after editing: `grep -rn '\b<svc>_[a-z]' <svc>/internal/mcp` should
     return only Go identifiers, never a bare tool name inside a string literal.

7. **Tests** (`<svc>/internal/mcp/*_test.go`): update every asserted tool name to
   the branded form; rename the whoami case to health and assert the new envelope
   shape (status/version/service/details + owner_email/client_id). Update
   `NewHandler(...)` call sites in tests to the new signature (pass a fixed test
   version/service and a nil-or-stub reporter). Where a test enumerates the
   descriptor set, assert the full branded names.

**Per-service gate:** `go build ./...` && `go test ./...` (root) green before the
next service step starts.

### Step 2 — crm
Tools: `search,get,save,delete,log,whoami→health`. Cross-ref: `crm_log` in
`saveDescription`. `Spec.Health` stays nil.

### Step 3 — ledger
Tools: per `ledger/internal/mcp/tools.go` (`…,whoami→health`). Cross-refs:
`ledger_describe` / `ledger_balance` / `ledger_register` in tool descriptions and
the `bad_root` corrective message in `ledger/internal/mcp/mcp.go`. `Spec.Health`
nil.

### Step 4 — notify
Single placeholder tool today (`notify_whoami` → `ikigenba_notify_health`). Update
the package doc comment in tools.go that references `notify_whoami`. `Spec.Health`
nil.

### Step 5 — wiki
Tools: `whoami→health, ingest_text, ingest_url, search, ask, job_status` (per
`wiki/internal/mcp/tools.go`). Cross-refs are the densest here (mcp.go prose +
every ingest/ask description). Note wiki has several test files
(`tools_test.go`, `ask_test.go`, `search_test.go`, `ingest_test.go`) — update tool
names in all of them. `Spec.Health` nil.

### Step 6 — ralph
Tools per `ralph/internal/mcp/tools.go` (`whoami→health` + the `ralph_session_*`
set). Update `ralph/internal/mcp/describe.go` prose (the `ralph_whoami` mention)
and `mcp_test.go`. `Spec.Health` nil. **Also** Step 8 covers the broader
`describe.go` prose pass — keep ralph's describe edit here limited to the tool-name
references; do the full prose read in Step 8.

### Step 7 — dropbox (fold two tools into one)
DECISIONS §7. Dropbox currently has **both** `dropbox_whoami` and `dropbox_health`.

- **Drop** `dropbox_whoami` entirely: remove its descriptor, its `case`, and its
  implementation.
- **Keep one tool**, `ikigenba_dropbox_health`, built via `tool("health")`.
- Its existing top-level telemetry (`mirror_bytes`, `disk_free_bytes`,
  `disk_total_bytes`, `failed_files`) **moves under `details`** per the envelope
  contract — it must NOT splat at the top level (DECISIONS §3 example).
- Unlike the other five services, dropbox **has** real telemetry, so it supplies a
  reporter. Wire it as `Spec.Health` in `dropbox/cmd/dropbox/main.go` so the HTTP
  `/health` route and the MCP tool both emit the same `details`. The reporter
  returns `{mirror_bytes, disk_free_bytes, disk_total_bytes, failed_files}` by
  calling the same source the current `dropbox_health` tool used (`info` in
  `dropbox/internal/mcp/tools.go`). The MCP `toolHealth` then renders
  envelope + identity automatically (§ pattern step 4), with details supplied by
  the reporter.
- Update the tools.go package doc comment (lines ≈11–13) that describes the
  two-tool migration; it is now a single tool.
- Tests (`dropbox/internal/mcp/tools_test.go`): drop the whoami case, assert the
  single branded health tool, and assert the telemetry keys are under `details`,
  not top-level.

**Gate:** build + test green.

---

## Step 8 — docs + remaining prose sweep

After all services are green, one subagent sweeps the remaining non-code
references (DECISIONS change-inventory §4):

- `docs/runbook-dashboard-box-cutover.md`
- `docs/appkit-extraction-map.md`
- `docs/runbook-d2-ledger-box-prototype.md`
- `docs/event-plane-decisions.md`

For each: update `<svc>_whoami` / `GET /whoami` references to the new reality —
the HTTP route is `GET /health` (ungated, liveness, `{status,version,service,
details}`), and the MCP tool is `ikigenba_<svc>_health` (gated, adds
`owner_email`/`client_id`). Where a doc describes the old whoami auth-chain proof,
move that description to the MCP health tool (§6). Where a doc shows a route table,
mark `/health` ungated (joins PRM + `/feed`).

Also re-run a **suite-wide grep** as the closing check:

```
grep -rn "whoami" --include=*.go --include=*.md .
```

Every remaining hit must be intentional (e.g. historical changelog prose). No live
code path, tool name, route, test, or current-state doc may still say `whoami`.

**Gate:** `go build ./...` && `go test ./...` (root) green; the grep returns no
unaccounted-for live references.

---

## Sequencing summary (strictly in order)

1. appkit — builder + `Spec.Health` + ungated `/health` + Options threading + tests
2. crm
3. ledger
4. notify
5. wiki
6. ralph
7. dropbox (fold two → one; supplies the reporter)
8. docs + closing grep

Each step ends green before the next begins. The dashboard has **no**
`internal/mcp` surface and is Apex (no PRM/whoami/health standard route), so it
needs no code change here — but per `CLAUDE.md`, after deploying the renamed MCP
services, the dashboard must be restarted so it re-reads the manifests (an
operational note, not a code step).

## The one design decision this plan pins down (was implicit in DECISIONS)

DECISIONS §4 mandates a *single* builder serving both transports but does not say
how the per-service MCP tool — which lives in `<svc>/internal/mcp` and is
constructed service-side — reaches it. This plan resolves it minimally and in
keeping with the existing seams: the builder is exported
(`server.Envelope`, re-exported as `appkit.Envelope`); appkit owns the required
keys; `version`/`service`/`reporter` flow Spec → `server.Options` → `appHandler`
→ Router accessors (`rt.Version()`/`rt.Service()`/`rt.Health()`); the service
passes them into `NewHandler` exactly as it already passes the domain `svc`. The
HTTP route adds nothing; the MCP tool adds identity. Neither transport
re-implements the envelope, so they cannot drift.
