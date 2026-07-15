# appkit — Product

**Authority: intent.** This document owns *why* appkit exists, *for whom*, what
is in and out of scope, and what we **promise** — in outcome terms only.
Mechanism (package layout, signatures, env-var names, route tables, JSON-RPC
envelopes) and its checkable proof live in `project/design/README.md` and the
Decision files it heads. Where the two touch observable behavior, product states
the *promise* and design states the *exact, checkable form*; that boundary keeps
product, design, and plan from overlapping.

## Problem

Every ikigenba service except the dashboard is supposed to be the same machine
with a different domain inside. In practice the codebases have drifted: each
service carries its own copy-pasted JSON-RPC MCP transport (~180 near-identical
lines each), its own landing/static handler harness with small accidental
divergences (double prefix-stripping here, a combined handler there), its own
duplicated `health` and `reflection` tools, and its own embedded HTML/CSS/font
assets baked into the binary. Consumer services additionally hand-write the
same event-plane loop wiring per upstream — near-identical composition-root
blocks that resolve a feed address, assemble the loop, and repeat the same
declaration in three places that must manually agree — and that wiring has
already drifted between services. A fix or improvement to any of this common
surface must be re-made twelve times, and each re-make is a chance for more
drift.
The suite has also decided (`docs/structured-mcp-design.md`) that MCP is its
single verb surface for agents *and* deterministic code — but today a tool's
result is readable only by an agent: JSON or prose inside a text block, errors
as bare messages. A machine caller (a scripts run, any future deterministic
actor) has nothing it can parse reliably and nothing it can branch on. And the
suite's loopback-only boundary (feeds, content endpoints) is a hand-copied
guard whose predicate would wrongly reject the loopback callers the suite's
identity contract now admits.
Embedding the web assets also puts UI files in the wrong place operationally:
they are invisible on the box, cannot be inspected or diffed in a release
directory, and cost a full rebuild in local dev for a one-line HTML tweak.

## Purpose

appkit is the shared chassis library every suite service is built on. It owns
the uniform half of a service — the fixed verbs, config-from-env, migrations,
the loopback HTTP server, the event-plane mounts — so that a service's own code
is only its domain. This unit of work extends that ownership to three more
surfaces that were never legitimately per-service: the **web serving machinery**
(pages rendered from an on-disk, release-shipped asset root instead of embedded
files), the **MCP transport with its standard tools** (the JSON-RPC plumbing
plus `health` and `reflection`, leaving services to declare only their domain
tools), and the **event-plane consumer loops** (a consumer declares its
upstreams and what to do with their events; the chassis runs the loops). The
current unit of work extends the MCP surface once more: every tool result
carries a **machine-readable rendering alongside the agent-readable one**, tool
errors carry **stable codes** from one suite vocabulary, and the chassis owns
the **loopback-only route boundary** as a declared route class instead of a
per-service hand-copied guard.

## Users

- **Service authors (humans and agents) in this mono-repo.** They build and
  maintain the twelve suite services. They want a new service, or a change to an
  existing one, to involve only domain code — and they want a chassis
  improvement to reach every service by recompile, not by twelve hand edits.
- **Operators of a deployed box.** They see the results indirectly: a service's
  web assets are ordinary files in the versioned release directory — visible,
  diffable, atomically swapped and rolled back with the binary.
- **Existing services not yet converted.** Every current service keeps compiling
  and behaving identically until it opts in. Nothing here may force a change on
  a service that hasn't adopted the new surfaces.

## Scope

This work adds to appkit: resolution of a per-service on-disk web-asset root
(shipped in the release, with a local-dev equivalent and a per-service
override), page templating and static-asset serving from that root, automatic
mounting of the static route for services that opt in, a reusable MCP transport,
chassis-owned `health` and `reflection` MCP tools, chassis-run event-plane
consumer loops driven by a per-service declaration of upstreams and handlers,
machine-readable tool results with declared shapes and a single tool-error code
vocabulary across the suite, and a chassis-owned loopback-only route class for
the surfaces that must never be reachable through the front door.
Nothing else: appkit still knows nothing about LLMs, tools-of-agents, or any
service's domain; it still never reads secrets; migrations stay embedded in
each service binary; the event-plane producer/consumer *split* and the wire
protocol are unchanged (what an event means, and what a consumer does with it,
stay service-owned); per-service *pages* (which routes exist beyond static,
what data they render) remain service-owned. The outbox migration SQL is
deliberately out of scope for this round.

## What we promise (user-facing behavior)

- **A service can serve its web surface from ordinary files.** A service author
  puts page templates and static assets in a conventional per-service directory;
  in production the service finds them in its versioned release tree, in local
  dev it finds them in the source tree, and an explicit override can point
  anywhere. Editing a page in dev is visible on refresh, without rebuilding.
- **Opting in gets the static route for free.** A service that declares its web
  root serves its CSS/fonts/JS correctly (right content types, no directory
  browsing) with zero service-side handler code. Pages stay the service's: it
  renders any page it wants, with live data, through the chassis-loaded
  templates.
- **A misconfigured web root fails at startup, loudly.** A service that declares
  a web root that isn't there refuses to start with a clear error — it never
  comes up half-styled.
- **A service's MCP surface is its domain tools and nothing else.** The service
  declares its tool names, descriptions, schemas, and handlers; the chassis
  speaks the protocol, threads the caller identity, and answers `health` and
  `reflection` identically across every service.
- **A consumer service is a declaration, not a wiring exercise.** A service
  that consumes other services' events states which upstreams it follows and
  what to do with each event; the chassis finds each upstream, runs the loops,
  keeps each upstream's place independently across restarts, and reports the
  same subscriptions everywhere they're visible. A contradictory declaration
  refuses to start with a clear error rather than guessing.
- **A tool result is readable by a machine without parsing prose.** Every
  domain tool's result carries a structured rendering a deterministic caller
  consumes directly, and its shape is discoverable up front from the tool
  listing; agents keep the readable rendering they see today. The chassis's
  own `health` and `reflection` conform like any other tool. Documentation
  tools and raw-content reads stay plain text on purpose. **The advertised
  schemas satisfy strict schema-validating MCP clients**, so a service's whole
  tool list loads in the clients users actually connect — not only lenient
  ones; the chassis refuses at startup to stand up a tool whose advertised
  schema a strict client would reject.
- **A tool error is something a program can branch on.** Every tool error
  carries one code from a single, small, suite-wide vocabulary — the same
  words every service already uses informally — alongside its human message.
- **A loopback-only surface is a declaration, not a hand-rolled guard.** A
  service marks a route as never-through-the-front-door and the chassis
  enforces it uniformly — including for callers that legitimately assert
  their own identity from on-box, which the old hand guards would wrongly
  reject.
- **The web and consumer surfaces remain opt-in.** Services that haven't
  adopted them keep building and behaving identically. **The structured
  result contract is the deliberate exception:** it is a suite-wide cutover —
  every service converts its result and error construction, and the suite
  deploys together. During the migration window there are no live customers,
  which is exactly why the cutover happens now.

## Success criteria (outcomes)

- A service converted to the on-disk web root serves the same pages and assets
  it served when they were embedded, in both local dev and the deployed layout,
  and its release bundle visibly contains the asset files.
- A page edit in a converted service's source tree is visible in the running
  dev service on the next request, with no rebuild.
- A converted service started against a missing web root exits with an error
  naming the path, rather than serving unstyled pages.
- A service converted to the chassis MCP transport presents the same tool list
  and tool behaviors it did before, including `health` and `reflection`, while
  its own MCP code has shrunk to a declaration of its domain tools.
- A consumer service converted to the declared form consumes the same events
  from the same upstreams it did before, resuming where it left off after a
  restart, while its own consumer code has shrunk to the declaration and the
  per-event handlers.
- Every service unconverted on the *opt-in* surfaces (web root, consumer
  declaration) still builds and passes its tests with no source change
  (allowing only the one-line build-graph mirror every chassis sibling
  dependency already requires of consuming modules).
- A deterministic program can call any converted service's tool and act on
  the result — and on a failure's code — without any string matching against
  human-readable text, and can learn the result's shape from the tool listing
  before calling.
- The agent-visible behavior of every converted tool is unchanged: an agent
  reading the tool result today sees the same information after conversion.
- A strict schema-validating MCP client (e.g. Claude Code) loads a converted
  service's entire tool list — every advertised tool, standard and domain —
  rather than rejecting the whole listing over one non-conforming schema, and a
  service whose tools would be rejected fails to start instead of coming up with
  a tool list no strict client can fetch.
- A request that arrives through the front door to a loopback-only surface is
  turned away exactly as before, while an on-box caller asserting its own
  identity gets through — across every service, from one shared enforcement
  point.
