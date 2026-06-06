# Plan — `ikigenba_<svc>_reflection`: event-plane self-description over MCP

## Context

ikigai is meant to be a **space people connect agents to**, where those agents can
build software that runs in the space — self-improving. For an agent to build a
new event-plane consumer, it must be able to *discover*, at runtime, what facts a
service publishes and the exact shape of each. Today it cannot:

- **Event types are bare string literals at the emit site.** `crm/internal/crm/events.go`
  inlines `"contact.created"`, `"contact.updated"`, `"contact.tagged"`,
  `"contact.untagged"` where it builds `outbox.Event{Type: …}`. Nothing enumerable
  exists; the type list lives only in scattered strings.
- The protocol **deliberately defers** a registry: `event-protocol.md` §8.5 ("No
  central type registry exists yet… a later addition if the type count grows") and
  §12 (lists "A central event-type registry" as deferred/non-normative).
- Yet the protocol also makes payload shape **interop, not prose** (§4.4: a consumer
  MUST NOT call back to interpret an event; §8.5: "a consumer written later must
  agree on it without reading the producer's source").

This plan adds a new MCP tool, **`ikigenba_<svc>_reflection`**, that lets a
connected agent discover a producer's published event types and their schemas —
the deferred registry, realized **decentrally** (each producer self-describes; no
central coordinator), which fits the suite's "template + copies inherit" model.

Events are JSON end to end (§8.7; `outbox.Event.Payload` is `json.RawMessage`
marshaled from a Go struct), so a proper JSON Schema is the natural descriptor.

## Decisions settled in discussion

1. **Single source of truth at the emit site (no drift).** A per-service event
   registry is the one place types are declared; emit sites reference it and
   reflection reads the same registry — the exact "two sites cannot drift" pattern
   the rebrand uses for `tool()`/`toolPrefix`.
2. **The *mechanism* is shared library code; the *content* is per-service data.**
   The registry type, schema/example rendering, and the reflection renderer are
   written once (mirrors `appkit.Envelope` + `Spec.Health`); each service declares
   only its own types.
3. **Two-level reflection, one tool, optional argument** (don't proliferate tools —
   the suite prizes fixed/minimal verb sets):
   - **Level 1 (no args)** → the *index*: `[{type, description}, …]` for every
     published type. The "what can I subscribe to?" call.
   - **Level 2 (`event_type` arg)** → the *detail* for one type:
     `{type, description, schema, example}`. The "give me the exact shape so I can
     build a consumer" call.
   - Unknown `event_type` → a **corrective error** listing the valid types (the
     pattern ledger's `bad_root` message uses), not an empty result.
4. **Schema + example both derive from one registered Go value.** Each registered
   type carries a populated **sample** instance of its payload struct. The library
   reflects the sample's *type* → JSON Schema, and marshals its *value* → the
   working example. Add/remove a payload field and the compiler-checked sample
   moves with it — schema, example, and wire shape can't diverge.
5. **Advertise only types that can actually be emitted** (§8.5 "naming a type is
   not specifying it"; crm deliberately does *not* build the second-wave
   `deal.*`/`interaction.*` events). The registry-at-emit-site approach gets this
   right for free.
6. **Producers only.** Reflection describes *published* events, so the tool mounts
   only on services with `Spec.Feed != ""` — **crm, ledger, dropbox**. Consumers
   (notify) publish nothing; wiki/ralph are not producers.
7. **Builds on the already-landed rebrand+health foundation:** `tool()`/`toolPrefix`,
   `appkit.Envelope`, and `Spec.Health` are present (`appkit/appkit.go:170`,
   `crm/internal/mcp/tools.go`). Reflection is wired the same way `health` is.

### Explicit non-goals (this pass)

- **Not** the MCP tool surface — MCP already self-describes via `tools/list`.
- **Not** consumed/subscribed types — a future "what does this service listen to?"
  extension; producers-publish-only here.
- **Not** full JSON Schema validation hardening / `$ref`-ing shared sub-objects —
  flat generated schema is sufficient for v1.

## Architecture

Three layers, each owning what it already owns.

### A. `eventplane/outbox` — the registry + rendering (the shared mechanism)

Add a registry type next to `Event`/`Options` in `eventplane/outbox/outbox.go`
(or a new `registry.go`):

```go
// EventType declares one published event type: its wire string, a one-line
// agent-facing description, and a populated sample of the payload struct. Sample
// is the single source for BOTH the JSON Schema (reflected from its type) and the
// working example (marshaled from its value), so the two cannot drift.
type EventType struct {
    Type        string // wire type, e.g. "contact.created" (§8.5)
    Description string // what this fact means and when it fires
    Sample      any    // a filled-in instance of the payload struct
}

// Registry is a service's ordered set of published event types (ordered ⇒ stable
// reflection output). It is the source of truth read by both Append validation
// and the reflection tool.
type Registry []EventType
```

Rendering helpers on `Registry` (the shared mechanism the reflection tool calls):

- `Index() []map[string]any` → level-1 `[{type, description}]`.
- `Detail(eventType string) (map[string]any, error)` → level-2
  `{type, description, schema, example}`; returns a typed "unknown type" error
  carrying the valid type list for the corrective message.
- `schema(sample any)` → JSON Schema from the sample's type. **One new dependency**
  in eventplane for struct→JSON-Schema reflection (e.g. `invopop/jsonschema`,
  battle-tested) — or a small hand-rolled reflector for the limited shapes in play
  (strings, bools, pointers, slices of structs). Recommend the library per
  "battle-tested foundations"; final pick at implementation.

**Append validation (fail loudly, §5.3 defense-in-depth).** When an outbox is
constructed with a non-empty registry, `Append` (`outbox.go:157`) rejects an
`ev.Type` not in the registry — guaranteeing reflection lists everything that can
be emitted. Empty registry → today's behavior (no validation), so adoption is
incremental and this never collides with in-flight work.

Thread the registry through construction: add `Registry Registry` to
`outbox.Options` (alongside `Source`) so the outbox knows it at `New`.

### B. `appkit` — one static field + accessor (mirrors `Spec.Health` threading)

- Add to `Spec` (`appkit/appkit.go`, next to `Health`):
  ```go
  // Events declares the service's published event types for the reflection MCP
  // tool and for Append-time validation. Empty for non-producers. Static data,
  // not a hook (like ManifestExtras).
  Events outbox.Registry
  ```
  appkit already imports `eventplane/outbox` (the `Producer` hook), so no new
  dependency direction.
- Pass `spec.Events` into `outbox.New(...)` where appkit builds the producer outbox
  (the `Spec.Feed != ""` path).
- Expose it on `Router` exactly like the health work exposes version/service/health:
  `func (rt *Router) Events() outbox.Registry`.

### C. Per-producer wiring (tiny; identical to the `health` tool pattern)

For **crm, ledger, dropbox** only:

1. **Declare the registry** in the domain package's `events.go`, using the real
   payload structs as samples (e.g. crm: `contactSnapshotPayload{…}`,
   `contactTagPayload{…}`), and reference its type strings at the existing emit
   sites so the literals stop being independent (constants drawn from the registry,
   or a `registry.Type("contact.created")` lookup). Wire it via `Spec.Events`.
2. **Add the MCP tool** in `<svc>/internal/mcp/tools.go`:
   - one descriptor `desc(tool("reflection"), …, schema-with-optional-event_type)`
     in `toolDescriptors()`;
   - one `case tool("reflection"):` in `dispatchTool` that calls the shared
     renderer: no `event_type` → `registry.Index()`; with it →
     `registry.Detail(arg)` (unknown → corrective error).
3. **Thread the registry into the Handler** the same way version/service/health are
   threaded: extend `NewHandler` and pass `rt.Events()` from
   `<svc>/cmd/<svc>/main.go`'s Handlers hook.

## Sequencing

1. **eventplane + appkit foundation** — `EventType`/`Registry` + `Index`/`Detail`/
   schema rendering + `Append` validation + `outbox.Options.Registry`; `Spec.Events`
   + `outbox.New` threading + `rt.Events()`. Gate: `go build ./...` && `go test ./...`.
2. **crm** — the worked example: declare the registry from `contactSnapshotPayload`/
   `contactTagPayload`, point emit sites at it, add the reflection tool, wire
   `Spec.Events`/`NewHandler`. Gate green.
3. **ledger** — repeat for its journal event types (`ledger/internal/.../events.go`).
4. **dropbox** — repeat for its event types.

Each step ends green before the next. Non-producers (notify, wiki, ralph,
dashboard) get **no** reflection tool.

## Critical files

- `eventplane/outbox/outbox.go` — `Event`/`Options`/`Append` (registry type,
  `Options.Registry`, Append validation); new `registry.go` for `Registry` +
  rendering.
- `appkit/appkit.go` — `Spec.Events` (next to `Spec.Health` at :170).
- `appkit/server/server.go` — `Router.Events()` accessor (mirror the
  version/service/health accessors the health work added).
- appkit producer-outbox construction site (the `Spec.Feed != ""` path that calls
  `outbox.New`) — pass `spec.Events`.
- Per producer: `<svc>/internal/crm|.../events.go` (declare registry; samples),
  `<svc>/internal/mcp/tools.go` (descriptor + dispatch), `<svc>/internal/mcp/mcp.go`
  (Handler struct + `NewHandler`), `<svc>/cmd/<svc>/main.go` (pass `rt.Events()`),
  and the `*_test.go` in each.

## Reuse (do not reinvent)

- `appkit.Envelope` + `Spec.Health` (`appkit/appkit.go:170`) — the exact "shared
  builder + per-service hook + Router accessor + NewHandler threading" pattern this
  follows.
- `tool()`/`toolPrefix` (`crm/internal/mcp/tools.go`) — brand the new tool as
  `tool("reflection")`; reuse `desc()` for its descriptor.
- ledger's `bad_root` corrective-message pattern — model the unknown-`event_type`
  error on it.
- `outbox.Event`/`Append` (`eventplane/outbox/outbox.go:43,157`) and crm's
  `contactSnapshotPayload`/`contactTagPayload` — the samples ARE these structs.

## Verification

- **Unit (eventplane):** table test that `Registry.Index()` returns every declared
  type; `Detail("contact.created")` returns a schema whose `required`/properties
  match `contactSnapshotPayload`'s json tags and an `example` that round-trips
  through that struct; `Detail("nope")` returns the corrective error listing valid
  types. Test that `Append` rejects an unregistered type when a registry is set and
  is unchanged when it is empty.
- **Unit (per service mcp):** `tools/list` includes `ikigenba_<svc>_reflection`;
  calling it with no args returns the index; with `event_type` returns the detail;
  with a bad type returns the corrective error.
- **Gate:** `go build ./...` && `go test ./...` at repo root after each step.
- **End-to-end (local):** run the suite via `nginx/run` (:8080), connect an agent,
  call `mcp__crm__ikigenba_crm_reflection` with no args (see the contact.* index),
  then with `event_type: "contact.created"` (see schema + example). Confirm the
  example's shape matches a real `contact.created` frame on crm's `/feed`.
