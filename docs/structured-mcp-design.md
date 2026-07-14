# Design — Structured MCP (one verb surface for agents and machines)

This document settles how deterministic code invokes a service's domain verbs.
The suite already has three planes: the **auth plane** decides who a request is
from, the **event plane** moves small facts, and the **content plane** moves
file bytes by reference. What it did not have is an answer for a Python script
(or any future non-agent actor) that needs to call `crm save` or
`ledger record`. The answer is: **there is no fourth surface.** MCP is the
suite's single verb surface, and every domain verb's result carries a machine
rendering alongside its agent rendering.

The short version: **verbs and facts move over MCP; bytes move over the content
plane; nothing else exists.** A service registers each verb once with the
appkit chassis; the chassis serves it to agents and machines alike, in one
response.

## Context — the problem and the road not taken

The scripts service runs deterministic Python wired to suite events, and it is
(with prompts) becoming the suite's actor: the consumer of events, the mover of
content, the caller of other services' capabilities. Today a script has stdlib
Python, an event payload, and nothing else: no sanctioned way to reach any
service.

Technically it could always call MCP. Every service answers `POST /mcp` on
loopback, and services trust identity headers blindly (nginx is the outside
boundary, not the inside one). What actually blocked it was **result shape**:
MCP results are agent-shaped. Content is `[{type:"text", text:…}]`, sometimes
JSON in a string, sometimes prose. An agent tolerates that; a deterministic
parser is fragile against exactly that.

The obvious fix was a second surface: give every service a loopback domain API
mirroring its MCP verbs (as dropbox's filesystem API already sits beside its
MCP tools). Rejected. The MCP spec (revision `2025-06-18`) already carries a
machine channel: a tool may declare an **`outputSchema`** and return
**`structuredContent`** (a plain JSON object) alongside its text blocks in the
same result. With that, a mirrored API adds only a permanent second surface on
every service, a second set of routes to guard, and a mirroring discipline to
police, in exchange for saving a machine caller one JSON-RPC envelope. The
envelope is ten lines of client code. The second surface is forever.

Two facts make the single-surface choice cheap here:

1. **appkit owns the transport.** `appkit/mcp` is the suite's own minimal
   JSON-RPC layer; supporting the newer revision is a small, central change.
2. **Services are already structured in practice.** Nearly every tool result is
   built by `JSONResult(v)`: a Go struct marshalled to JSON and shipped as
   text. The refactor formalizes what services already do; it does not reshape
   results.

## The core principle

> **A domain verb is registered once and answers every caller. Its result
> carries `structuredContent` for machines and mirrored text for agents, in
> one response.**

Callers then divide by kind, not by surface:

- **Agents** (a claude.ai client through nginx, a prompts run over loopback)
  discover tools with `tools/list`, read descriptions and guides, and consume
  the text rendering.
- **Machines** (a scripts run, any future deterministic actor) call the same
  tools through a thin client and consume `structuredContent`, never parsing
  prose.

## The result contract

Every appkit-served service conforms:

- **Protocol.** `appkit/mcp` speaks revision `2025-06-18`. (The agentkit client
  already speaks a newer revision and adopts the server's answer; verified
  compatible.)
- **Output schemas.** Each registered tool declares an `outputSchema`
  alongside its `inputSchema`, surfaced by `tools/list`. The schema is the
  machine contract: versioned with the service, testable, and stable in the
  way agent-facing descriptions are not required to be.
- **Structured results.** A domain verb's success result contains
  `structuredContent` conforming to its declared schema, plus a text block
  mirroring the same JSON (the agent rendering). appkit provides one helper,
  `StructuredResult(v)`, that emits both from a single typed value; hand-built
  content arrays are for the exceptions below.
- **Structured errors.** Tool errors keep `isError` and a human message, and
  additionally carry the suite's shared error vocabulary
  (`validation`, `not_found`, `conflict`, `too_large`, `source_unavailable`)
  as a structured field, so machine callers branch on a code, never on message
  text.
- **Prose exceptions.** Documentation tools (`guide`, `describe`) and
  raw-content reads remain plain text; they are for readers, and they declare
  no output schema.
- **Chassis tools included.** appkit's built-in `health` and `reflection`
  declare schemas and return structured results like any other verb.

## The identity contract

MCP verbs are owner-scoped, so a loopback caller must say who it acts for.
The rule, uniform across surfaces:

- **The request carries identity; the handler never cares which door it came
  through.** nginx injects `X-Owner-Email`/`X-Client-Id` for requests from
  outside; a loopback caller asserts the same headers itself. On-box processes
  are inside the trust boundary already; this states the model rather than
  hiding it.
- **`X-Client-Id` names the actor** (`scripts:<script_id>`,
  `prompts:<prompt_id>`), giving every write provenance and every emitted
  event its `origin`, exactly as the file share stamps prompts-run writes
  today.
- **The loopback guard narrows to its real meaning.** Loopback-private
  endpoints (`/feed`, content endpoints, the file-share API) currently return
  404 on seeing `X-Owner-Email` *or* `X-Forwarded-Proto`. The guard's question
  is "did this cross nginx?", and `X-Forwarded-Proto` alone answers it; nginx
  stamps it on every proxied request. Keying on `X-Owner-Email` conflated
  "came from outside" with "carries identity", which caller-asserted identity
  breaks. The guard moves into appkit as one shared helper instead of
  per-service copies.

## The consumer doctrine

- **Agents speak MCP** through discovery, descriptions, and guides.
- **Deterministic code speaks MCP** through a thin generic client:
  `call(service, tool, args) → structuredContent`. No per-service client
  libraries; a hand-written wrapper per service would be a second, drift-prone
  description of an API that is already self-describing.
- **Bytes never ride MCP.** File movement is the content plane's job: follow a
  `content_url` reference, or use the file share's filesystem API. The 25 MiB
  base64 conveniences remain conveniences.
- **Services still do not chain.** Machine callers of MCP are the actor
  services working on the owner's behalf. Domain services keep publishing
  facts and consuming feeds; this document adds no service-to-service verb
  calls.

Discovery for authoring agents comes free: the machine surface *is* the MCP
surface, so an agent that knows a service's tools (it has them loaded) already
knows exactly what a script may call, with identical names, arguments, and
now result schemas.

## The first machine consumer: the scripts runtime

scripts adopts the plane end to end; the specifics land through its own
`project/` loop, shaped as:

- A runner-injected **`suite` Python module** (embedded in the scripts binary,
  materialized beside `main.py`), exposing roughly:
  `suite.event()` (the parsed trigger payload), `suite.mcp(service, tool,
  args)` (the generic verb client, identity asserted, structured results and
  error codes), `suite.fetch(content_url, dest)` (the content-plane acceptor
  verb, loopback-confined, hash-verified), and `suite.files.*` (the file
  share's filesystem API, stamped `X-Client-Id: scripts:<script_id>`).
- The **holder side**: `GET /run-content?run_id=&path=` serving run-dir files
  under the standard guard, and a `content_url` on every non-directory
  `run_fs_list` entry, so a script's products travel onward by reference. Both
  are lifts of prompts' D22.
- `describe` teaches the runtime contract, so authoring agents write against
  `suite` instead of hand-rolling HTTP.

prompts is unaffected: its runs consume MCP as agents (text rendering) and
already have their own content-plane tools.

## Conformance map

| unit | change | status |
|---|---|---|
| appkit | protocol `2025-06-18`; `Tool.OutputSchema`; `StructuredResult`; structured error vocab; schemas for `health`/`reflection`; shared loopback guard | new — the root dependency |
| agentkit | none (tolerates `structuredContent`/`outputSchema`; verified) | done |
| every service | swap `JSONResult` → `StructuredResult`; declare output schemas; adopt the shared guard | new; mechanical, parallel, no ordering |
| nginx | confirm `X-Forwarded-Proto` is stamped on every proxied route (prod and local-dev) | verify during cutover |
| scripts | `suite` module; `/run-content` holder; `run_fs_list` references; `describe` | new — the feature this enables |

Each service row lands through that service's own `project/` spec loop; this
document is the shared contract they cite. The migration window (no live
customer data) is when the guard change and protocol bump cut over.

## Decisions resolved

- **No second surface.** MCP is the single verb surface for agents and
  machines; no per-service domain APIs beyond the byte-plane endpoints that
  already exist.
- **Structured results by construction.** One registration in appkit renders
  both `structuredContent` and mirrored text; declared `outputSchema` is the
  machine contract.
- **Structured error codes** from the shared vocabulary on every tool error.
- **Caller-asserted identity on loopback**; the loopback guard keys on
  `X-Forwarded-Proto` only and lives in appkit.
- **No per-service client libraries.** Deterministic callers use one generic
  MCP client; the tool schemas are the documentation.
- **scripts is the first machine consumer**, via a runner-injected `suite`
  module, and becomes a content-plane holder for its run products.

## Non-goals

- **A REST/domain API convention.** Deliberately rejected; adding one later
  would mean this contract failed, not that it was incomplete.
- **Bytes over MCP.** Large or binary content stays on the content plane;
  `structuredContent` carries facts and references, never file bodies.
- **Schema-checked enforcement in the transport.** appkit surfaces
  `outputSchema` and helps produce conforming results; it does not validate
  every response against the schema at runtime. Conformance is the service's
  test suite's job.
- **Cross-box or public machine callers.** Loopback callers asserting identity
  is a single-box trust statement; nothing here weakens nginx as the sole
  outside boundary.
- **Rewriting prompts' agent path.** agentkit needs no changes; the text
  rendering agents consume today is unchanged.
