# Plan — prompts in-run suite tools (Surface 2)

**Status:** plan only — no code yet. This is the implementation plan for giving the
sandboxed Claude agent *inside a run* access to the other suite services' MCP
tools (send email, read dropbox, post to the ledger, …). It is the continuation
of "Surface 2 — the in-run toolset" carved out of scope in
`REDESIGN-DECISIONS.md`; that doc covered Surface 1 (the foreground
`ikigenba_prompts_*` tools) and explicitly deferred this.

Companion docs: `ARCHITECTURE.md` (how the code is shaped today),
`REDESIGN-DECISIONS.md` (the prompt/run model), suite `CLAUDE.md` (the trust
boundary, manifests, deploy).

---

## 1. Goal & the one-paragraph mechanism

Today an in-run agent has a fixed, sandbox-confined toolset —
`read, bash, write, edit, glob, grep` (`agentkit/tools.All()`), dispatched by
`agentkit/tools.Dispatch(ctx, sandboxRoot, …)`. This plan **adds** the suite's
MCP tools to that set so a run can act across the suite *on behalf of the run's
owner*.

The mechanism reuses what already exists. Every service binds
`127.0.0.1:<PORT>` and mounts `POST /mcp` behind `requireIdentityHeaders`, which
does **no token logic** — it trusts a non-empty `X-Owner-Email` header blindly
(`appkit/server/middleware.go`). nginx is "the trust boundary" only because it is
the sole *public* ingress; on loopback, a co-resident process supplies those
headers itself. So prompts connects directly to `http://127.0.0.1:<PORT>/mcp` and
injects `X-Owner-Email: <run.owner_email>` — acting as a second trusted loopback
identity-injector alongside nginx. No OAuth, no tokens, no authorization-server
involvement. This is the user's "localhost only / no auth / dynamic."

---

## 2. Settled decisions (do not re-litigate)

1. **All-on.** Advertise every discovered service's tools to the in-run agent. No
   per-prompt allow-list in this phase (deferred — §10).
2. **Exclude `prompts`.** Never advertise prompts' own tools to its in-run agent
   (no run-spawns-run recursion). Self-chaining stays an event-plane concern.
3. **Discover via manifests.** Glob the on-box per-service `etc/manifest.env`
   (the dashboard's existing mechanism), filter `MCP=true`, read each service's
   loopback `PORT`.
4. **Snapshot at spawn.** Freeze the service + tool set once when the run starts;
   it stays fixed for the run's life (mirrors the "pin inputs per run" rule).
5. **Identity.** `X-Owner-Email: <run.owner_email>` (already on `prompt.Run`),
   `X-Client-Id: prompts:<prompt_id>` for downstream audit attribution.
6. **Module boundaries.** Generic, reusable pieces in **agentkit** (the
   `agent.Run` tool-source seam + the MCP-over-HTTP client). Suite-specific policy
   in **prompts** (manifest discovery, identity injection, self-exclusion).
7. **Share discovery.** Lift the dashboard's manifest **scan** into **appkit** so
   prompts and the dashboard agree on what counts as an MCP service and where
   manifests live.
8. **`agent.Run` → options struct.** Collapse the trailing args into an
   `agent.Options` and add the optional tool source there. wiki's three call sites
   become `nil`-tools (behaviorally unchanged).
9. **Best-effort everywhere.** A down/garbled service is skipped at discovery
   (log loud, proceed); a downstream call that errors or drops mid-run returns an
   `is_error` tool_result — it never crashes the run.

---

## 3. Architecture — three layers

```
appkit/inventory      (NEW)  Read(root) -> []Service{Name,Mount,Port,Feed}
   │                          lift of dashboard/internal/inventory; uses
   │                          appkit/manifest.Parse. dashboard re-points here.
   ▼
agentkit/agent        (CHG)  Run(ctx, client, sess, req, Options{...})
   │  agentkit/agent.ToolSource (NEW interface): Descriptors()+Owns()+Dispatch()
   │  agentkit/mcpclient (NEW): JSON-RPC 2.0 over HTTP POST tools/list + tools/call
   ▼
prompts/internal/suite (NEW) discovery + identity policy: at spawn, glob
   │                          manifests (appkit/inventory), exclude self, build a
   │                          remote ToolSource over the live loopback endpoints
   ▼
prompts/internal/runner (CHG) build the suite ToolSource at spawn, pass it via
                              agent.Options.Tools
```

Two planes stay separate: this is the **MCP/tool plane** (loopback `POST /mcp`),
distinct from the **event plane** (`/feed` SSE, the existing `Consumes`). This
change does **not** touch `CONSUMES` or any feed wiring.

---

## 4. Work breakdown — strict sequential phases

Each phase below is **one sub-agent's unit of work**, and they run **strictly one
at a time in the order listed — never in parallel**, even where the dependency
graph would permit it (Phases 1, 2, and 3 are mutually independent, yet are still
done in sequence). Every phase **begins from the previous phase's committed, green
state and ends green**: `go build ./...` and the phase's own tests (§8) pass
before the next phase starts. A phase that cannot reach green is a blocker to
surface — not a half-finished thing to hand to the next sub-agent.

"No parallel work" governs *development*: one sub-agent at a time. Runtime
concurrency *inside* a phase's code (e.g. Phase 5's concurrent `ListTools`
fan-out at discovery) is a separate matter and is unaffected.

### Phase 1 — `appkit/inventory` (lift the manifest scan)

- New package `appkit/inventory` with `Read(root string) ([]Service, error)`,
  `Service{Name, Mount, Port, Feed string}` — moved verbatim from
  `dashboard/internal/inventory/inventory.go`, but parsing via the existing
  `appkit/manifest.Parse` instead of the package's private `parseManifest`
  (removes the duplicate parser).
- Re-point `dashboard/internal/inventory` consumers (`dashboard/cmd/dashboard`
  `deriveResources`, `dashboard/internal/server` inventory handler) to
  `appkit/inventory`; delete the dashboard-local package (or leave a thin alias if
  test fixtures need it — prefer deletion + test move).
- Move/port the existing inventory tests to appkit.

**Why first:** prompts depends on it; doing the lift first keeps one
implementation from day one rather than prompts forking a second copy.

**Watch:** appkit must not gain a dependency on dashboard. The scan is pure
stdlib + `appkit/manifest`, so this is clean.

**Exit:** `appkit/inventory.Read` lands with ported tests, the dashboard
re-points to it, the dashboard-local package is gone, and
`go build ./... && go test ./appkit/... ./dashboard/...` is green.

### Phase 2 — `agentkit/mcpclient` (the outbound MCP client)

**Depends on:** nothing — a self-contained new package.

New package. There is **no** existing outbound MCP/JSON-RPC client in the repo —
all `/mcp` code today is server-side. Build a minimal one:

- `type Client struct { endpoint string; httpClient *http.Client; headers map[string]string }`
- `ListTools(ctx) ([]Tool, error)` → JSON-RPC `tools/list`, returns
  `{name, description, inputSchema}` per tool.
- `CallTool(ctx, name string, args json.RawMessage) (text string, isError bool, err error)`
  → JSON-RPC `tools/call`, flattening the MCP result
  `{content:[{type:"text",text}], isError?}` to a string (matching how the
  in-run agent's other tool results are strings).
- Wire shape mirrors `prompts/internal/mcp/mcp.go`: JSON-RPC 2.0, `POST`,
  `Content-Type: application/json`, **no SSE**.
- Skip the `initialize` handshake: the suite handlers are stateless per request
  (identity from headers each call) and answer `tools/list`/`tools/call`
  directly. (If a service ever requires `initialize`, best-effort discovery in
  Step D simply drops it.)
- Per-call timeout via the client's `http.Client.Timeout` (a wedged peer must not
  hang a run beyond its TTL). Bounded, generous default.

Headers are injected per-client (set once when the client is built for a given
owner): `X-Owner-Email`, `X-Client-Id`.

**Exit:** `go test ./agentkit/mcpclient/...` green over the httptest coverage in §8.

### Phase 3 — `agentkit/agent`: `Options` struct + `ToolSource` type (behavior-preserving refactor)

**Depends on:** nothing. This phase adds **no new behavior** — it is a pure,
repo-wide signature refactor whose green build is the checkpoint proving zero
regression before the live seam lands in Phase 4. The `ToolSource` interface is
declared but never consulted, and `Tools` is `nil` at every call site, so runtime
behavior is byte-identical to today.

Collapse `Run`'s trailing parameters:

```go
// before
func Run(ctx, client, sess, req, sch *schema.Schema, sandboxRoot string, tracer *trace.Tracer) error
// after
type Options struct {
    Schema      *schema.Schema // nil → freeform terminal mode (unchanged default)
    SandboxRoot string         // "" → unconfined (engine tests)
    Tracer      *trace.Tracer  // nil → no tracing
    Tools       ToolSource     // nil → built-ins only (wiki, tests)
}
func Run(ctx context.Context, client provider.Client, sess *wire.Session, req provider.Request, opts Options) error
```

Update all **four** call sites, every one behavior-identical (`Tools` nil):
- `wiki/internal/ask/ask.go`, `.../lint/lint.go`, `.../ingest/ingest.go` →
  `agent.Run(ctx, client, sess, req, agent.Options{SandboxRoot: j.sandboxRoot})`
  (they keep advertising their own `req.Tools` subsets and dispatching via
  built-in `tools.Dispatch`).
- `prompts/internal/runner/runner.go` → `agent.Run(ctx, client, wireSess, req,
  agent.Options{SandboxRoot: sandboxRoot})` — **`Tools` left nil here.** The suite
  source is built and wired in Phase 6, *not* now; this keeps Phase 3 a clean
  behavior-preserving checkpoint.

Declare the `ToolSource` interface in this same phase (the `Options.Tools` field
needs the type) but **do not consult it yet** — advertising and dispatch routing
are Phase 4:

```go
type ToolSource interface {
    Descriptors() []provider.Tool                       // extra tools to advertise
    Owns(name string) bool                              // does this source handle this tool name?
    Dispatch(ctx context.Context, name string, input json.RawMessage) (wire.ToolResultBlock, error)
}
```

**Exit:** `go build ./... && go test ./agentkit/... ./wiki/... ./prompts/...`
green, with every existing test unchanged (no behavior delta).

### Phase 4 — `agentkit/agent`: advertise + dispatch routing (the live seam)

**Depends on:** Phase 3 (the `Options`/`ToolSource` types). This is the first
phase where `opts.Tools`, when non-nil, changes behavior.

**Advertise.** In `Run`, before the first `client.Stream`, append
`opts.Tools.Descriptors()` to `req.Tools` (when `opts.Tools != nil`). Caller-
supplied `req.Tools` (built-ins) stay; the source's descriptors are added. So the
model sees built-ins **plus** the suite tools.

**Dispatch routing.** In `dispatchTools`, route each `tool_use` block:
`opts.Tools != nil && opts.Tools.Owns(tu.Name)` → `opts.Tools.Dispatch(...)`;
else → the existing `tools.Dispatch(ctx, sandboxRoot, tu)`. Built-in tools have no
service prefix (`read`/`bash`/…); suite tools are service-prefixed
(`ikigenba_crm_*`), so `Owns` is an exact-name membership test — no collisions.
A source-dispatched result has no sidecar (nil), same as the read/write/glob/grep
path.

**Threading:** `opts` must reach `dispatchTools` (today it takes `sandboxRoot`,
`tracer`). Pass `opts` (or the two needed fields) through.

**Exit:** `go test ./agentkit/...` green, including the new fake-`ToolSource`
tests (§8); wiki's `nil`-tools path stays byte-identical.

### Phase 5 — `prompts/internal/suite` (discovery + policy)

**Depends on:** Phases 1 (`appkit/inventory`), 2 (`agentkit/mcpclient`), and 3
(the `ToolSource` type it implements).

New package owning the suite-specific glue:

- **Discover(ctx, owner, promptID) ToolSource** — at spawn:
  1. `appkit/inventory.Read(manifestRoot)` → MCP services on the box.
  2. Drop the entry whose `Name == "prompts"` (self-exclusion, decision #2).
  3. For each remaining service, build an `mcpclient.Client` for
     `http://127.0.0.1:<Port>/mcp` with headers
     `X-Owner-Email: <owner>`, `X-Client-Id: prompts:<promptID>`.
  4. `ListTools(ctx)` each (concurrently, short per-call timeout); **skip** any
     service that errors or is unreachable, log loud, continue (decision #9).
  5. Build the index `tool name → owning client` and return a `ToolSource`
     implementation that advertises every discovered tool and dispatches by
     routing the name to its client's `CallTool`.
- The returned `ToolSource.Dispatch` wraps `CallTool`: a transport error or
  `isError` result becomes a `wire.NewToolResultBlock(id, true, msg)` — never a Go
  error that would crash the run.
- `manifestRoot` from new env `PROMPTS_MANIFEST_ROOT` (default `/opt`; in local
  dev, the repo root resolves `<repo>/*/etc/manifest.env` to the right dev ports).

**Exit:** `go test ./prompts/internal/suite/...` green over the httptest-peer
coverage in §8 — self (`prompts`) excluded, a down peer skipped, identity headers
asserted, a prefixed name routed to the right peer.

### Phase 6 — `prompts/internal/runner` (wire it in)

**Depends on:** Phase 3 (the call site is already `Options`-shaped, `Tools` nil)
and Phase 5 (`suite.Discover`). This phase flips that nil to the live source.

- `runner.New` (or a field) gains the `manifestRoot` so `execute` can call
  `suite.Discover`.
- In `execute`, after reading the run's pinned inputs and before
  `agent.Run`: build `suiteSource := suite.Discover(ctx, run.OwnerEmail, run.PromptID)`
  (note: `run.PromptID` for the client id, `run.OwnerEmail` for identity — both
  already on `prompt.Run`).
- Change the Phase-3 call to pass the source:
  `agent.Run(ctx, client, wireSess, req, agent.Options{SandboxRoot: sandboxRoot, Tools: suiteSource})`.
- `buildRequest` keeps advertising the built-in `tools.All()` in `req.Tools`; the
  suite tools are added by the agent loop from the ToolSource (Phase 4). (Do
  **not** also merge them in `buildRequest` — single source of truth for the suite
  tools is the ToolSource.)

**Exit:** `go test ./prompts/...` green, including the extended fake-client runner
test asserting a `ToolSource` is built at spawn and threaded into `agent.Run`.

### Phase 7 — docs

**Depends on:** Phase 6 (the behavior the prose describes is now real). The §9
doc updates, pulled out as the final phase so the code phases (1–6) stay
build-gated: `prompts/ARCHITECTURE.md` gains the Surface-2 section and the §13
deferred-gaps update. No code, no build gate.

---

## 5. Config / env

| env | default | meaning |
|---|---|---|
| `PROMPTS_MANIFEST_ROOT` | `/opt` | root globbed for `*/etc/manifest.env` to discover MCP peers. Repo root in local dev. |

No new secret. Downstream credentials (e.g. gmail's per-owner Google grant)
remain the **downstream service's** concern — prompts only asserts identity via
the header; the peer looks up that owner's grant. No `~/.secrets` change, no
`.envrc` change.

`CONSUMES` / feed wiring is **unchanged** (different plane).

---

## 6. Security note (document in ARCHITECTURE.md §13 / new section)

This is a deliberate, real trust escalation, and the plan records it as such:

- A run's blast radius grows from "its sandbox dir" to "anything the run's owner
  can do across the suite," driven by a non-deterministic agent.
- prompts becomes a **second loopback identity-injector** alongside nginx. This is
  consistent with the existing model (loopback bind = the trust boundary;
  everything co-resident is trusted) but it *is* an expansion of who injects
  `X-Owner-Email`. The injected owner is always the run's own `owner_email`, set
  when the owner created the prompt through nginx — legitimate delegation, not
  privilege escalation across owners.
- The sandbox path-confinement still governs the *file* tools; it does **not** and
  cannot constrain the MCP tools, which reach real owner data in real services by
  design.
- OS/network isolation of the sandbox remains the pre-existing deferred gap
  (ARCHITECTURE.md §9/§13); this change *intentionally* grants the agent loopback
  access to privileged peers, so any future bubblewrap/podman `--network` story
  must allowlist the loopback MCP ports rather than cut all networking.

---

## 7. Observability

Suite tool calls flow through the same `dispatchTools` → `sess.EmitUser` path as
built-in tools, so they already land in the run's `output.jsonl` stream-json log
and through the tracer (when set) for free. No extra logging plumbing — just
confirm the proxied `tool_use` / `tool_result` render correctly in the stream.

Optional (fast-follow, not this plan): add a sentence to `agent.FramingPrompt`
noting that suite tools may be present. The per-tool descriptions from
`tools/list` are self-documenting, so the agent already gets schemas + docs; the
framing nudge is a nicety, not a requirement.

---

## 8. Testing strategy

- **appkit/inventory** — port the dashboard inventory tests; assert `Read` over a
  temp tree with mixed `MCP=true`/absent manifests, sorted, garbled-skipped.
- **agentkit/mcpclient** — `httptest.Server` speaking the JSON-RPC shape: assert
  `ListTools` parses `tools/list`, `CallTool` flattens text + surfaces `isError`,
  headers (`X-Owner-Email`/`X-Client-Id`) are sent, timeout fires, transport error
  surfaces as a Go error (the caller maps it to `is_error`).
- **agentkit/agent** — a fake `ToolSource`: assert its descriptors are advertised
  (appended to `req.Tools`), `Owns`-matched names route to it, unmatched names
  fall through to built-in `tools.Dispatch`, and a source-dispatch error becomes
  an `is_error` tool_result (run continues). Confirm the four call sites compile
  and wiki's `nil`-tools path is byte-identical to today.
- **prompts/internal/suite** — discovery over a temp manifest root + `httptest`
  peers: self (`prompts`) excluded, a down peer skipped (best-effort), identity
  headers carry `run.owner_email` and `prompts:<prompt_id>`, the assembled source
  routes a prefixed name to the right peer.
- **prompts/internal/runner** — extend the existing fake-client run test to assert
  a `ToolSource` is built at spawn and threaded into `agent.Run` (no real network;
  inject a fake discoverer). No real Anthropic call (existing `clientFactory`
  seam).
- **No new live-network test in CI** — discovery is best-effort and unit-tested
  against `httptest`.

---

## 9. Doc updates (part of the change, not separate)

- `prompts/ARCHITECTURE.md` — new section for Surface 2 (the in-run suite
  toolset): the loopback mechanism, snapshot-at-spawn, identity injection,
  self-exclusion, the security note (§6 above), and `PROMPTS_MANIFEST_ROOT`.
  Update §13 deferred-gaps (network isolation now must allowlist loopback MCP).
- `prompts/CLAUDE.md` — does not exist today; not created by this plan.
- Suite `CLAUDE.md` — no change required (the trust model is unchanged in kind);
  optionally note prompts as a loopback MCP *client*.

---

## 10. Deferred / explicitly out of scope

- **Per-prompt allow-list / deny-list** of services or tools. All-on now;
  allow-listing is an additive `config` field later (no schema churn) once the
  wiring is proven.
- **Re-discovery mid-run** (a service coming up after spawn). Snapshot-at-spawn is
  the decision; runs are short relative to deploy cadence.
- **`initialize` handshake / MCP session state.** Skipped — suite handlers are
  stateless per request.
- **Sandbox network isolation** that would also gate these loopback calls — the
  pre-existing platform-level deferred gap; see §6.
- **Surface 1** (`ikigenba_prompts_*` foreground tools) — unchanged by this work.
- **Talking back / push-to-owner** from a tool result — unchanged deferred item.

---

## 11. Risks & open watch-items

- **appkit must stay dashboard-free.** The inventory lift is pure stdlib +
  `appkit/manifest`; verify no accidental dashboard import sneaks in.
- **Tool-name uniqueness across peers.** Suite tools are service-prefixed
  (`ikigenba_<svc>_*`), so cross-peer collision is structurally impossible; assert
  it anyway when building the name→client index (log loud on a dup rather than
  silently shadowing).
- **Loopback path is `/mcp`, not `/srv/<svc>/mcp`.** nginx strips the prefix; the
  service mounts the bare `POST /mcp` (`appkit/server/server.go`). The client uses
  `127.0.0.1:<PORT>/mcp` — `MOUNT` is irrelevant on loopback.
- **Dev vs box port parity.** Manifests carry the same `PORT` the service binds in
  both dev and prod (confirmed against the feed-URL table in `ARCHITECTURE.md`
  §8), so manifest discovery yields correct loopback endpoints in both.
- **Run TTL vs. slow peers.** Per-call client timeout must be well under the run
  TTL so a wedged peer surfaces as an `is_error` tool_result, not a run-killing
  hang.
```
