# Plan — `ikigenba_<svc>_reflection`: event-graph self-description over MCP

## Context

ikigai is meant to be a **space people connect agents to**, where those agents can
build software that runs in the space — self-improving. For an agent to build a
new event-plane service, it must be able to *discover*, at runtime, the **event
graph**: what facts each service publishes (and their exact shapes), and what each
service listens to. Today it can discover neither:

- **Published event types are bare string literals at the emit site.**
  `crm/internal/crm/events.go` inlines `"contact.created"`, `"contact.updated"`,
  `"contact.tagged"`, `"contact.untagged"` where it builds `outbox.Event{Type: …}`.
  Nothing enumerable exists.
- **Subscriptions are a hardcoded `if` plus a manifest name.** `notify` decides what
  it acts on with `if ev.Type != "contact.created"` (`notify/internal/push/push.go:113`)
  and declares its upstream only as `Spec.Consumes: ["crm"]` → `CONSUMES=crm`.
  Neither is reflectable, and the two aren't tied together.
- The protocol **deliberately defers** a registry (`event-protocol.md` §8.5 "No
  central type registry exists yet… a later addition if the type count grows"; §12
  lists "A central event-type registry" as deferred/non-normative). It also makes
  published payload shape **interop, not prose** (§4.4, §8.5: a consumer written
  later must agree on it without reading the producer's source).

This plan adds one MCP tool, **`ikigenba_<svc>_reflection`**, with two halves:

- **`publishes`** — the event types this service emits, with on-demand JSON Schema +
  worked example (the out-edges and node schemas of the graph).
- **`subscribes`** — what this service listens to: `{source, filter, description}`
  (the in-edges). No schema here — an agent resolves an edge's shape by **following
  it** to that source's `publishes` reflection.

Realized **decentrally** — each service self-describes; no central coordinator —
which fits the suite's "template + copies inherit" model. Events are JSON end to
end (§8.7; `outbox.Event.Payload` is `json.RawMessage` from a Go struct), so JSON
Schema is the natural descriptor.

## Decisions settled in discussion

### Shared

1. **Single source of truth, no drift.** Each half is declared once and read by both
   the runtime path and reflection — the "two sites cannot drift" pattern the
   rebrand uses for `tool()`/`toolPrefix`.
2. **Mechanism is shared library code; content is per-service data.** Registry/
   subscription types + rendering live in `eventplane`; each service declares only
   its own (mirrors `appkit.Envelope` + `Spec.Health`).
3. **One tool, two levels, optional argument** (don't proliferate tools):
   - **No args** → the index: `{publishes: [{type, description}], subscribes: [{source, filter, description}]}`.
   - **`event_type` arg** → publish detail for one type:
     `{type, description, schema, example}`.
   - Unknown `event_type` → a **corrective error** listing valid types (the pattern
     ledger's `bad_root` message uses), not an empty result.
4. **Mounts on any event-plane participant** — producer and/or consumer. `crm`
   (producer) shows `publishes`, empty `subscribes`; `notify` (consumer) shows empty
   `publishes`, populated `subscribes`; a service that's both shows both.
5. **Builds on the already-landed rebrand+health foundation:** `tool()`/`toolPrefix`,
   `appkit.Envelope`, `Spec.Health` are present (`appkit/appkit.go:170`,
   `crm/internal/mcp/tools.go`). Reflection is wired the way `health` is.

### Publish side — a static registry

6. **Schema + example both derive from one registered Go value.** Each registered
   type carries a populated **sample** instance of its payload struct. The library
   reflects the sample's *type* → JSON Schema and marshals its *value* → the worked
   example. Add/remove a payload field and the compiler-checked sample moves with it
   — schema, example, and wire shape can't diverge.
7. **Advertise only types that can actually be emitted** (§8.5 "naming a type is not
   specifying it"; crm deliberately does *not* build the second-wave `deal.*`/
   `interaction.*` events). The registry-at-emit-site approach gets this for free.
8. **Static is correct here.** A service's emittable types are compile-time payload
   structs — you cannot reflect a JSON Schema from a type that does not exist at
   build time. So `Spec.Events` is a static registry value.

### Subscribe side — a live provider

9. **Filtering stays consumer-side and unchanged (§7.3).** The producer streams
   every event; the consumer matches each one, runs its effect only on a match, and
   **commits the cursor for every event regardless** (including discarded ones). No
   producer-side filtering; no change to that behavior.
10. **The filter is a declared value, not a hidden literal.** The handler matches
    against the same declared `Subscription` that reflection reports, so the two
    can't drift. notify's hardcoded `if ev.Type != "contact.created"` becomes
    `if !sub.Match(ev.Type) { return nil }` against the declared subscription. No
    engine `Mux`, no change to cursor semantics.
11. **Glob filter via stdlib `path.Match`** (punt, no dependency). Event types are
    dotted with no slashes, so it's effectively a flat glob: `contact.created`
    (exact), `contact.*`, `*` (all). Honest caveat to document: with no separator,
    `*` spans dots. The glob gates *whether the effect runs*, never the cursor
    commit.
12. **Multiple sources and multiple filters per source** — native to the list model;
    the same source may appear many times with different filters. Reflection flattens
    them. (Runtime: one feed connection per source; several filters on one source
    share it and the handler routes among them — the existing per-Worker model.)
13. **Live provider, not a static slice** — because subscriptions can be **dynamic**.
    Mirror `Spec.Health`: `Spec.Subscriptions func() []consumer.Subscription`, called
    *at reflection time*, returning the current set. Static consumers return a fixed
    list; a future dynamic consumer (e.g. ralph, where each session subscribes
    uniquely) returns the union of its live sessions' subscriptions. Reflection then
    always reports the live graph with **no redesign** when dynamic subscription
    lands.
14. **`Spec.Consumes` stays the static envelope, separate from the provider.**
    `Consumes` feeds `CONSUMES=` in the manifest, which must be known at build time
    without any sessions running, so it is **not** derived from the provider.
    Invariant: **live subscription sources ⊆ declared `Consumes`** — you declare the
    envelope of upstreams you might connect to up front; sessions subscribe within it.
    For a static consumer the two coincide.

### Explicit non-goals (this pass)

- **Not** the MCP tool surface — MCP already self-describes via `tools/list`.
- **Not** full JSON Schema hardening / `$ref`-ing shared sub-objects — flat
  generated schema is sufficient for v1.
- **Not** the dynamic-subscription machinery (runtime per-session feed open/close,
  ralph sessions). Only the **provider abstraction** is built now so reflection is
  ready for it; the runtime machinery is deferred.

## Architecture

Each layer owns what it already owns.

### A. `eventplane/outbox` — publish registry + rendering (shared mechanism)

Add to `eventplane/outbox/outbox.go` (or a new `registry.go`):

```go
// EventType declares one published event type. Sample is the single source for
// BOTH the JSON Schema (reflected from its type) and the worked example (marshaled
// from its value), so the two cannot drift.
type EventType struct {
    Type        string // wire type, e.g. "contact.created" (§8.5)
    Description string // what this fact means and when it fires
    Sample      any    // a filled-in instance of the payload struct
}

type Registry []EventType   // ordered ⇒ stable reflection output
```

Rendering helpers the reflection tool calls:

- `Index() []map[string]any` → `[{type, description}]`.
- `Detail(eventType string) (map[string]any, error)` → `{type, description, schema, example}`;
  unknown type → a typed error carrying the valid-type list for the corrective message.
- `schema(sample any)` → JSON Schema from the sample's type. **Hand-rolled, zero new
  dependencies** (decided): a small `reflect`-based reflector covering exactly the shapes
  in play — `string`/`bool`/`int*`/`float*`, pointers (→ optional, non-`required`),
  slices (→ `array` with `items`), and nested structs (→ inline `object`). Honors the
  `json` tag for property names and `omitempty`/pointer for the `required` set. The narrow
  shape space (it only ever reflects our own payload structs) makes a ~60-line reflector
  simpler and lighter than pulling in `invopop/jsonschema`; "pay complexity only when
  forced" wins over "battle-tested foundations" here because the surface is tiny and fully
  test-covered. An unsupported kind must **fail loudly** (panic at registry-build / test
  time), never emit a silently-wrong schema.

**Append validation (fail loudly, §5.3).** When an outbox is built with a non-empty
registry, `Append` (`outbox.go:157`) rejects an `ev.Type` not in the registry —
guaranteeing reflection lists everything emittable. Empty registry → today's
behavior, so adoption is incremental. Thread it via a new `Registry` field on
`outbox.Options` (alongside `Source`).

### B. `eventplane/consumer` — subscription type + matcher (shared mechanism)

Add to `eventplane/consumer`:

```go
// Subscription declares one thing this consumer listens to. Filter is a glob
// (path.Match) over the dotted event type. Handler runs the effect for a match;
// the engine commits the cursor for every event regardless (§7.3).
type Subscription struct {
    Source      string  // upstream service name, e.g. "crm" (must be ⊆ Spec.Consumes)
    Filter      string  // glob: "contact.created", "contact.*", "*"
    Description string  // what this service does in reaction
    Handler     Handler // effect; omitted from reflection output
}

func (s Subscription) Match(eventType string) bool // path.Match(s.Filter, eventType)
```

The handler matches against its declared `Subscription` (`sub.Match`) instead of a
hardcoded literal. No engine `Mux`; cursor semantics unchanged.

### C. `appkit` — two declarations + accessors (mirror `Spec.Health`)

- Add to `Spec` (next to `Health` at `appkit/appkit.go:170`):
  ```go
  // Events: published event types — static registry for the reflection tool and
  // Append-time validation. Empty for non-producers.
  Events outbox.Registry
  // Subscriptions: a LIVE provider of what this service currently listens to,
  // called at reflection time (mirrors Spec.Health). Returns a fixed list for a
  // static consumer; the live union for a future dynamic one. nil for non-consumers.
  Subscriptions func() []consumer.Subscription
  ```
  appkit already imports `eventplane/outbox`; it gains an import of
  `eventplane/consumer` (which `notify` already uses).
- Thread `spec.Events` to the outbox on the `Spec.Feed != ""` path. This is **two hops**,
  not one: `verbs.go` builds `feed.Options` → `feed.Start` builds `outbox.Options` →
  `outbox.New`. So add a `Registry` field to **both** `feed.Options` (`appkit/feed/feed.go`)
  and `outbox.Options`, and pass `spec.Events` through `feed.Options{… Registry: spec.Events}`
  at `appkit/verbs.go` (the `feed.Start(...)` call), which forwards it into `outbox.Options`.
- Expose both on `Router`, like the version/service/health accessors:
  `func (rt *Router) Events() outbox.Registry` and
  `func (rt *Router) Subscriptions() func() []consumer.Subscription`.

### D. Per-service wiring (tiny; identical to the `health` tool pattern)

1. **Add the MCP tool** in `<svc>/internal/mcp/tools.go`: one
   `desc(tool("reflection"), …, schema-with-optional-event_type)` in
   `toolDescriptors()`; one `case tool("reflection"):` in `dispatchTool` calling the
   shared renderers — no arg → `{publishes: registry.Index(), subscribes: render(subs())}`;
   with `event_type` → `registry.Detail(arg)` (unknown → corrective error). `subscribes`
   renders each `Subscription` to `{source, filter, description}` (Handler dropped).
2. **Thread registry + provider into the Handler** the way version/service/health are:
   extend `NewHandler` and pass `rt.Events()` / `rt.Subscriptions()` from
   `<svc>/cmd/<svc>/main.go`.
3. **Producers** (crm, ledger, dropbox): declare the registry in the domain
   `events.go` from the real payload structs and point emit sites at it; wire
   `Spec.Events`.
4. **Consumers** (notify): build the declared `Subscription`; the handler matches via
   `sub.Match`; wire `Spec.Subscriptions` to return the fixed list.

## Sequencing — execution phases

Six sequential phases, each dispatched to its own subagent in order. **No phase starts
until the previous one is green.** This section is the orchestration contract: each phase
below is self-contained (scope · files · gate · commit) so a subagent can execute it from
this plan alone.

**Conventions for every phase**
- **Branch:** all work lands on `feat/mcp-reflection` (created off `main` before Phase 1a).
- **Gate (uniform):** `go build ./...` && `go test ./...` at the repo root, both green.
  This is the *only* acceptance check a subagent runs — there is no e2e step inside a phase
  (see "Final manual verification" below).
- **Commit:** one commit per phase on `feat/mcp-reflection` after the gate passes, message
  `mcp-reflection: <phase title>`. Do **not** push, deploy, bump, or merge — phases stop at
  the commit.
- **Dependency:** each phase strictly depends on all prior phases (the foundation types in
  1a/1b are consumed by 2–6). Phases 2–6 do not depend on *each other's* domain code, but
  are still run sequentially as specified.
- A subagent that cannot get its phase green leaves the tree uncommitted and reports the
  failure rather than committing red or papering over it (fail loudly).

### Phase 1a — eventplane foundation (shared mechanism)
*Scope:* the publish registry + the subscription matcher, with their unit tests. No appkit,
no service wiring yet.
- `eventplane/outbox`: add `EventType` + `Registry` (new `registry.go`); the hand-rolled
  `schema(sample any)` reflector (§A — `reflect`-based, json-tag aware, fail-loud on
  unsupported kinds); `Index()` and `Detail(eventType)` renderers (`Detail` returns the
  typed unknown-type error carrying the valid-type list); add `Registry` to
  `outbox.Options`; `Append` validation rejecting an unregistered `ev.Type` when a non-empty
  registry is set, unchanged when empty (`outbox.go:157`).
- `eventplane/consumer`: add `Subscription` + `Match` (`path.Match` glob over the dotted
  type) (new file ok).
- *Tests (this phase owns them):* the eventplane/outbox and eventplane/consumer unit tests
  from §Verification (Index/Detail/schema round-trip/corrective-error/Append-guard;
  `Match` table test).
- *Gate + commit* as above (`mcp-reflection: eventplane foundation`).

### Phase 1b — appkit foundation (wiring seam)
*Scope:* expose the two new declarations through appkit so services can wire them; no
service touched yet.
- `appkit/appkit.go`: add `Spec.Events outbox.Registry` and
  `Spec.Subscriptions func() []consumer.Subscription` next to `Spec.Health` (:170); add the
  `eventplane/consumer` import.
- `appkit/feed/feed.go`: add `feed.Options.Registry`; forward into `outbox.Options`.
- `appkit/verbs.go`: pass `spec.Events` via `feed.Options{… Registry: spec.Events}` on the
  `Spec.Feed != ""` path.
- `appkit/server/server.go`: add `Router.Events()` and `Router.Subscriptions()` accessors
  (mirror `Router.Health()`); thread the two values through `RouterOptions`/`App` the same
  way `Health` flows.
- *Gate + commit* (`mcp-reflection: appkit foundation`). Nothing consumes the new fields
  yet, so the suite must build and all existing tests stay green unchanged.

### Phase 2 — crm (publish side; the worked example)
*Scope:* first producer end to end.
- `crm/internal/crm/events.go`: declare the `Registry` from the real payload structs
  (`contactSnapshotPayload`, `contactTagPayload`) with filled-in `Sample` values + the four
  type strings + descriptions; point the emit sites at the registered type constants
  instead of bare literals.
- `crm/internal/mcp/tools.go`: add the `desc(tool("reflection"), …)` descriptor (optional
  `event_type` arg) and the `case tool("reflection"):` dispatch calling
  `registry.Index()` / `registry.Detail(arg)`; render `subscribes` (empty for crm).
- `crm/internal/mcp/mcp.go`: extend `NewHandler` to take the registry + subscriptions
  provider; store on `Handler`.
- `crm/cmd/crm/main.go`: set `Spec.Events`; pass `rt.Events()` / `rt.Subscriptions()` into
  `mcp.NewHandler`.
- *Tests:* the per-service mcp tests from §Verification (tools/list includes the tool;
  no-arg → `{publishes, subscribes}`; `event_type` → detail; bad type → corrective error).
- *Gate + commit* (`mcp-reflection: crm publish side`).

### Phase 3 — notify (subscribe side; the worked example)
*Scope:* the consumer end to end. Note notify is consumer-only — empty `publishes`, and its
`NewHandler` has no domain-service arg.
- `notify/internal/push/push.go`: build the declared
  `Subscription{Source:"crm", Filter:"contact.created", …}`; replace the hardcoded
  `if ev.Type != "contact.created"` (:113) with `if !sub.Match(ev.Type) { return nil }`
  against that declared subscription.
- `notify/internal/mcp/{tools.go,mcp.go}`: reflection descriptor + dispatch; extend
  `NewHandler` to take the subscriptions provider (and registry, empty).
- `notify/cmd/notify/main.go`: wire `Spec.Subscriptions` to return the fixed list; pass
  `rt.Subscriptions()` into `mcp.NewHandler`. `Spec.Consumes` stays as-is.
- *Tests:* per-service mcp tests — notify shows empty `publishes` + its one `subscribes`
  entry, with `Handler` not leaked into the rendered output.
- *Gate + commit* (`mcp-reflection: notify subscribe side`).

### Phase 4 — ledger (publish side)
*Scope:* second producer; same shape as Phase 2.
- `ledger/internal/ledger/events.go`: registry + samples for ledger's journal event types;
  point emit sites at them.
- `ledger/internal/mcp/{tools.go,mcp.go}` + `ledger/cmd/ledger/main.go`: reflection tool,
  `NewHandler` threading, `Spec.Events`.
- *Tests + gate + commit* (`mcp-reflection: ledger publish side`).

### Phase 5 — dropbox (publish side)
*Scope:* third producer; same shape as Phase 2.
- `dropbox/internal/dropbox/events.go`: registry + samples for dropbox's event types;
  point emit sites at them.
- `dropbox/internal/mcp/{tools.go,mcp.go}` + `dropbox/cmd/dropbox/main.go`: reflection
  tool, `NewHandler` threading, `Spec.Events`.
- *Tests + gate + commit* (`mcp-reflection: dropbox publish side`).

Services with neither role (wiki, ralph, dashboard) get no reflection tool — until ralph
grows dynamic subscription (future), at which point its provider returns the live
per-session set with no reflection redesign.

### Final manual verification (orchestrator + user, after all phases)
Not a subagent phase. After Phase 5 is green, run the §Verification "end-to-end (local)"
walkthrough by hand: bring the suite up via `nginx/run` (:8080), connect an agent, and
confirm `mcp__crm__ikigenba_crm_reflection` (no-arg index + `contact.created` detail
matching a real `/feed` frame) and `mcp__notify__ikigenba_notify_reflection` (empty
`publishes`, the one `crm`/`contact.created` `subscribes` in-edge). Only after this passes
does the branch merge / deploy per the normal bump → ship flow.

## Critical files

- `eventplane/outbox/outbox.go` — `Event`/`Options`/`Append` (registry type,
  `Options.Registry`, Append validation); new `registry.go` for `Registry` + rendering.
- `eventplane/consumer/consumer.go` — `Subscription` + `Match` (new file ok).
- `appkit/appkit.go` — `Spec.Events`, `Spec.Subscriptions` (by `Spec.Health` at :170).
- `appkit/server/server.go` — `Router.Events()` / `Router.Subscriptions()` accessors.
- `appkit/feed/feed.go` — add `feed.Options.Registry`, forward it into `outbox.Options`.
- `appkit/verbs.go` — producer-outbox construction (`Spec.Feed != ""` path) — pass
  `spec.Events` through `feed.Options{… Registry: spec.Events}`.
- Producers: `<svc>/internal/.../events.go` (registry + samples),
  `<svc>/internal/mcp/{tools.go,mcp.go}` (descriptor/dispatch/Handler),
  `<svc>/cmd/<svc>/main.go` (pass `rt.Events()`), `*_test.go`.
- Consumer: `notify/internal/push/push.go` (declared `Subscription` + `sub.Match`
  replacing the literal at :113), `notify/internal/mcp/{tools.go,mcp.go}`,
  `notify/cmd/notify/main.go` (wire `Spec.Subscriptions`, pass `rt.Subscriptions()`),
  `*_test.go`.

## Reuse (do not reinvent)

- `appkit.Envelope` + `Spec.Health` (`appkit/appkit.go:170`) — the exact "shared
  builder + per-service hook/value + Router accessor + NewHandler threading" pattern.
  `Spec.Subscriptions` is a reporter shaped like `Spec.Health`.
- `tool()`/`toolPrefix` + `desc()` (`crm/internal/mcp/tools.go`) — brand as
  `tool("reflection")`.
- ledger's `bad_root` corrective-message pattern — for the unknown-`event_type` error.
- `path.Match` (stdlib) — the glob matcher; no dependency.
- `outbox.Event`/`Append` (`eventplane/outbox/outbox.go:43,157`),
  `consumer.Handler`/`Run` (`eventplane/consumer/consumer.go:70,107`), and crm's
  `contactSnapshotPayload`/`contactTagPayload` — the samples ARE these structs.

## Verification

- **Unit (eventplane/outbox):** `Registry.Index()` returns every declared type;
  `Detail("contact.created")` returns a schema whose properties/`required` match
  `contactSnapshotPayload`'s json tags and an `example` that round-trips through that
  struct; `Detail("nope")` returns the corrective error listing valid types. `Append`
  rejects an unregistered type when a registry is set, unchanged when empty.
- **Unit (eventplane/consumer):** `Subscription.Match` table test — exact, `*.`-glob,
  `*`, and non-match cases over dotted types.
- **Unit (per-service mcp):** `tools/list` includes `ikigenba_<svc>_reflection`; no-arg
  call returns `{publishes, subscribes}` with the expected entries (and Handler not
  leaked into `subscribes`); `event_type` arg returns the detail; a bad type returns
  the corrective error. notify shows empty `publishes` + its one `subscribes` entry.
- **Gate:** `go build ./...` && `go test ./...` at repo root after each step.
- **End-to-end (local):** run the suite via `nginx/run` (:8080), connect an agent.
  `mcp__crm__ikigenba_crm_reflection` (no args) → contact.* index; with
  `event_type: "contact.created"` → schema + example; confirm the example matches a
  real `contact.created` frame on crm's `/feed`.
  `mcp__notify__ikigenba_notify_reflection` (no args) → empty `publishes`,
  `subscribes: [{source:"crm", filter:"contact.created", …}]` — the in-edge that
  completes the graph.
