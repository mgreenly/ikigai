# crm — Product

**Authority: intent.** This document owns *why* crm serves the surfaces it does,
*for whom*, what is in and out of scope, and what we **promise** — in outcome
terms only. Mechanism (handlers, templates, the Carbon tokens, the nginx
fragment, the MCP transport, the tool descriptors, the guide document) and its
checkable proof live in `project/design/design.md`. Where the two touch
observable behavior, product states the *promise* and design states the *exact,
checkable form*; that boundary keeps product, design, and plan from overlapping.

> **Scope note.** This doc covers crm's **ralph-governed** work — the surfaces
> this `project/` builds. There are two threads:
>
> 1. **The web landing page** — the human front door under `/srv/crm/`.
> 2. **Agent-facing MCP self-discovery** — how a connecting agent learns what
>    crm is and how to drive its tools, from the crm connection alone.
>
> crm's underlying **CRM domain behavior** — the entity model, the verbs'
> semantics, validation, the migrations, the outbox producer — is owned by
> `crm/CLAUDE.md` and the domain notes. The discovery thread changes only how
> that surface is **described** to an agent and adds one **read-only** guide;
> it changes nothing about what any tool *does*.

## Problem

**Landing page.** Until recently every ikigenba service except the dashboard
served **only** machine surfaces — the RFC 9728 PRM bootstrap, `/health`, the
bearer-gated `/mcp`, and the loopback `/feed`. A human who opened
`<account>.ikigenba.com/srv/crm/` in a browser got nothing useful: there is no
token in a browser, so the bearer gate refuses them, and there was no
human-facing page behind that mount at all. The services declared "no UI," and
that statement has been deliberately retired: every deployable app now serves its
own HTML pages, beginning with a single landing page.

**Agent discovery.** A human does the actual *work* of crm through an AI agent,
not the landing page. For that agent to use crm well, it must discover — from
crm's own MCP connection alone — what crm is for, when to reach for it, and how to
drive each tool. Today that discovery is uneven. The tool surface is verbose in
one place (a single tool carries a full per-entity field reference that **every**
tool listing pays for in context) and silent in others, and agents have leaned on
an **external, separately-installed skill** to map everyday language ("contacts",
"companies", "pipeline") to crm and to recall field shapes. An agent that has only
crm connected, with no such skill, cannot make efficient use of it. The surface
should describe itself.

## Purpose

crm exposes two self-explaining front doors. The **landing page** is the human
front door under `/srv/crm/`: a minimal Carbon-styled card showing the service
name and running version, gated by the dashboard browser session. The **MCP
surface** is the agent front door: it describes itself so a connecting agent
learns what crm is, when to use it, and how to drive each tool, and can pull a
single fuller usage guide **on demand** — all from the crm connection itself, with
**no external skill, plugin, or doc** required for correct use.

## Users

- **A logged-in dashboard user, in a browser.** Any human authenticated to this
  box's dashboard who navigates to `/srv/crm/` sees the service name and version
  on the Carbon design system. The check is deliberately **coarse**: any
  logged-in dashboard user may view any app's landing page — there is no
  per-resource or per-owner authorization on this page.
- **The operator, confirming a deploy.** Opens the mount root after a deploy or
  rollback to confirm crm is up and which version is live — a browser-visible
  liveness signal that complements the machine `/health` and `version` checks.
- **A connecting AI agent (and, through it, the account owner).** An agent that
  has crm's MCP server registered. It routes work to crm, drives the tools, and
  when it needs field shapes or examples, retrieves crm's usage guide. Its whole
  understanding of crm comes from crm's own MCP surface.

The landing page is **not** for agents; agents use the bearer-gated `/mcp`
endpoint. The discovery surface is **not** for humans in a browser; it is what an
agent reads over MCP.

## Scope

**Landing page.** crm serves one page at the mount root: a `GET` of the bare
`/srv/crm/` root returns an HTML page showing the service name (`crm`) and the
running version, styled with the **Carbon** design system, carrying its **own**
embedded `tokens.css` and fonts, gated by the dashboard session cookie (an
unauthenticated browser gets `401`). It does nothing else in v1 — no per-resource
authorization, no domain data on the page, no interactive control, and it shares
no handler with any other service.

**Agent self-discovery.** The crm MCP surface describes itself so a connecting
agent can use it with no external skill:

- **Orient from the connection.** A concise service overview names crm's domain
  in the words users actually use (companies, people, deals/pipeline, tasks,
  notes) and states the normal working flow, so an agent can tell what crm is for
  and route to it.
- **Lean per-tool descriptions.** Each tool tells the agent *when to use it* and
  *what it returns*, without carrying bulk reference material that every listing
  must pay for.
- **A guide on demand.** An agent can retrieve a single crm usage guide covering
  the field shapes for each entity and **basic and advanced** worked examples —
  only when it wants it, not in every listing.

It deliberately does **nothing else**: it does **not** change what any crm tool
*does* (the entity model, the verbs, their semantics, validation, and the event
surface are unchanged), does **not** require any external skill/plugin/doc for
correct use, and does **not** alter the landing page or the machine endpoints
(`/mcp` transport, PRM well-known, `/health`, `/feed`).

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **The landing page lives at the mount root only** — reachable at
  `<account>.ikigenba.com/srv/crm/`; the service answers it at its exact root
  path `/` and nowhere else. It never shadows `/mcp`, `/health`, `/feed`, or the
  PRM well-known.
- **The landing page is gated by the dashboard browser session, not a bearer
  token** — `auth_request /_session-authn`, never `/_authn`; a failed session
  check yields `401`. The gate is **coarse**: any logged-in dashboard user may
  view it.
- **v1 landing content is exactly: service name + running version** — from what
  the chassis already exposes (`rt.Service()` / `rt.Version()`); no new data
  source.
- **Each app owns its own landing page** — no shared landing handler; crm's page
  code, template, and assets live under `crm/`.
- **The visual system is Carbon** — the suite tokens/fonts; crm embeds its own
  copy.
- **The MCP surface is self-sufficient** — a connecting agent can discover and
  correctly use crm from the crm MCP connection alone, with **no external skill**.
- **Discovery describes; it does not change behavior** — the entity model, the
  verb set, their semantics, validation, and the event surface are unchanged by
  the discovery work. The guide is **read-only**, and it adds **no per-entity
  tool** (the tool count stays a function of verbs, not entities).

## What we promise (user-facing behavior)

**Landing page.**

- **A logged-in human who opens `/srv/crm/` sees a real page** — the crm service
  name and running version, on the suite's design system, not a raw proxy error
  or blank page.
- **A browser that is not logged in is refused** — `401`, because the page is
  gated by the dashboard session cookie.
- **The page looks like the rest of the suite** — same fonts, palette, single
  blue signal color, and spacing grid; it loads its **own** embedded assets, not
  the dashboard's.
- **The version on the page is the version actually running** — the operator can
  confirm a deploy or rollback in a browser.

**Agent self-discovery.**

- **An agent with only crm connected can find and use it without any external
  skill** — it routes work about companies, people, deals/pipeline, tasks, and
  notes to crm from crm's own overview, and knows the normal flow.
- **Each tool tells the agent when to use it and what it returns**, concisely,
  without every listing carrying a full field reference.
- **An agent can ask crm for a usage guide** and get field shapes per entity plus
  **basic and advanced** worked examples, on demand.
- **Nothing an agent could already do changed** — every crm tool behaves exactly
  as before; only how the surface describes itself changed, plus the added
  read-only guide.

**Agents are unaffected across both threads** — the bearer-gated `/mcp`
transport, the PRM well-known, `/health`, and the loopback `/feed` behave exactly
as before; the landing page shadows none of them.

## Success criteria (outcomes)

Each is a result a viewer, operator, or connecting agent can confirm against the
running service:

- As a logged-in dashboard user I open `<account>.ikigenba.com/srv/crm/` and see
  a Carbon-styled page showing the service name `crm` and the running version.
- As a browser with no dashboard session I open `/srv/crm/` and am refused with
  `401`, not shown the page.
- The version shown on the page matches the version the deployed binary reports.
- The page loads its own embedded `tokens.css` and fonts, and its fonts and colors
  match the suite design system.
- With **only crm connected and no external skill installed**, an agent asked to
  work with contacts / companies / deals routes to crm and completes a basic
  create → find → log flow.
- An agent retrieves crm's usage guide and, using only it, correctly constructs a
  `save` for each entity type — including the set-valued-field and
  derived-deal-status gotchas.
- crm's everyday tool listing is materially leaner than before (the bulk per-type
  field reference is no longer carried in every listing) while each tool still
  conveys when to use it.
- Every existing crm tool call still produces the same result it did before this
  work — the discovery changes altered no behavior.
- An MCP client still discovers the AS via the PRM well-known and calls the
  bearer-gated `/mcp` exactly as before; opening `/srv/crm/feed` from nginx still
  returns `404` and `/health` still responds.
