# notify — Product (landing page)

**Authority: intent.** This document owns *why* notify serves a web landing
page, *for whom*, what is in and out of scope, and what we **promise** the viewer
— in outcome terms only. Mechanism (the handler, the embedded template, the
Carbon tokens, the nginx fragment, the route pattern) and its checkable proof
live in `project/design/design.md`. Where the two touch observable behavior,
product states the *promise* and design states the *exact, checkable form*; that
boundary keeps product, design, and plan from overlapping.

> **Scope note.** This product doc covers **only** the new web-pages direction
> for notify — the landing page. notify's existing domain (the event-plane
> consumer loops and the `send`/`health`/`reflection` MCP surface) is owned by
> `notify/CLAUDE.md` and the event-protocol docs; it is untouched here. This page
> mirrors the **crm landing-page template** the suite's simple services copy, so
> it states the uniform v1 starting point precisely — retargeted to notify.

> **Second concern in this backlog (no user-facing shift).** notify's design/plan
> (D9–D10, phases 7–8) also carry a separate, **behavior-preserving** change:
> notify resolves its own listen port and its crm/prompts feed URLs through the
> shared `registry` address table **by name**, instead of the hardcoded
> `127.0.0.1:30xx` literals it carries today. The resolved values are identical to
> the current literals, so **no promise or outcome in this document changes** — a
> viewer, an operator, and an MCP client all see exactly what they see now. The
> only outcome is internal correctness/operability: the port is written down once
> (in `registry`), so a renumber can no longer drift silently and reach deploy.
> This is noted here only so the product doc does not appear to contradict a
> Decision in the same backlog; it adds no user-facing promise of its own.

## Problem

Until now every ikigenba service except the dashboard served **only** machine
surfaces — the RFC 9728 PRM bootstrap, `/health`, and the bearer-gated `/mcp`. A
human who opened `<account>.ikigenba.com/srv/notify/` in a browser got nothing
useful: there is no token in a browser, so the bearer gate refuses them, and
there was no human-facing page behind that mount at all. The services declared
"no UI," and that statement is now being deliberately retired.

The suite is evolving so that **every deployable app serves its own HTML web
pages**, beginning with a single landing page. Each app's page will later
diverge to serve that app's specific purpose, so there is **no shared landing
handler** — each app owns its own page. v1 is the uniform starting point: a
human who lands on the mount root should see, at minimum, *which service this is*
and *what version is running*, presented on the suite's design system rather than
as a raw error or a blank proxy response.

## Purpose

The notify landing page is the **human front door** to the notify service under
`/srv/notify/`. For v1 it is intentionally minimal: a single Carbon-styled card
showing the **service name** and the **running version**. It is gated by the
viewer's **dashboard browser session** (the login cookie), not by a bearer
token — because a browser cannot present a bearer token, and because a
name-and-version page warrants only a coarse "are you a logged-in user of this
box" check, never a per-resource authorization. The page proves the service is
deployed, reachable, and on-system, and it establishes the seam (handler +
embedded template + embedded design assets) that every later notify web page
grows from.

## Users

- **A logged-in dashboard user, in a browser.** Any human authenticated to this
  box's dashboard who navigates to `/srv/notify/`. They see the service name and
  version on the Carbon design system. The check is deliberately **coarse**: any
  logged-in dashboard user may view any app's landing page — there is no
  per-resource or per-owner authorization on this page.
- **The operator, confirming a deploy.** Opens the mount root after a deploy or
  rollback to confirm notify is up and which version is live — a browser-visible
  liveness signal that complements the machine `/health` and `version` checks.

The page is **not** for agents or MCP clients — those keep using the
bearer-gated `/mcp` endpoint (notify's `send`/`health`/`reflection` tools), which
is unchanged.

## Scope

The notify landing page does this and only this:

- **Serve one landing page at the mount root** — a `GET` of the bare
  `/srv/notify/` root returns an HTML page. Internally (nginx strips the mount
  prefix) the service answers this at its exact root path `/`.
- **Show the service name and version** — the page displays the service name
  (`notify`) and the running version, taken from the values the chassis already
  exposes. Nothing else is shown in v1.
- **Look like the suite** — the page is styled with the **Carbon** design system:
  monochrome neutrals, blue `#2563EB` as the only signal color, the Space
  Grotesk / IBM Plex Sans / IBM Plex Mono type pairing, the 4px spacing grid. A
  simple centered card: service name in display type, version as a mono label.
- **Carry its own design assets** — notify embeds its **own** copy of the Carbon
  `tokens.css` and the woff2 fonts under its static directory and serves them
  from its own mount; it does not depend on the dashboard's assets at runtime.
- **Gate humans by the dashboard session cookie** — the page (and any future
  notify web page) is reachable only by a viewer whose `dashboard_session` cookie
  validates against the dashboard's web-session store. An unauthenticated browser
  gets `401`. This is the same coarse session gate `sites` already uses for its
  private static tier.

It deliberately does **nothing else** in v1 — in particular it does not: perform
any per-resource or per-owner authorization (the session gate is coarse by
design); expose any notification, push history, consumer-cursor, or event-feed
data on the page; add or change any MCP tool (`send`/`health`/`reflection`);
serve any interactive control, form, or write action; alter the bearer-gated
`/mcp`, the PRM well-known, or `/health`; touch notify's east/west event-plane
consumer loops; or share a landing handler with any other service. Later
notify-specific web pages are **out of scope** for this work — this establishes
only the uniform v1 page and the seam they will grow from.

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **The landing page lives at the mount root only.** A human reaches it at
  `<account>.ikigenba.com/srv/notify/`; the service answers it at its exact root
  path `/` and nowhere else. It never shadows `/mcp`, `/health`, or the PRM
  well-known.
- **The page is gated by the dashboard browser session, not by a bearer token.**
  The gate is `auth_request /_session-authn` (the dashboard-owned, loopback-only
  cookie validator) — never `/_authn` (the bearer gate). A failed session check
  yields `401`.
- **The gate is coarse.** Any logged-in dashboard user may view the page; there
  is no per-resource check. This is acceptable precisely because the page reveals
  only the service name and version.
- **v1 content is exactly: service name + running version.** No more. The values
  come from what the chassis already exposes (`rt.Service()` / `rt.Version()`);
  the page adds no new data source.
- **Each app owns its own landing page.** There is no shared landing handler;
  notify's page code, template, and embedded assets live under `notify/`.
- **The visual system is Carbon.** `design/carbon.md` (rules) +
  `design/tokens.css` (tokens) + `design/example.html` (reference) are the source
  of truth; notify embeds its own copy of the tokens and fonts.

## What we promise (user-facing behavior)

- **A logged-in human who opens `/srv/notify/` sees a real page** — the notify
  service name and the running version, on the suite's design system, not a raw
  proxy error or a blank page.
- **A browser that is not logged in is refused** — an unauthenticated browser
  hitting `/srv/notify/` gets `401`, because the page is gated by the dashboard
  session cookie.
- **Agents are unaffected** — the bearer-gated `/mcp` endpoint (notify's
  `send`/`health`/`reflection` tools), the PRM well-known, and `/health` behave
  exactly as before; the landing page is added beside them, shadowing none of
  them. notify's east/west event-plane consumer loops are likewise untouched.
- **The page looks like the rest of the suite** — same fonts, same neutral
  palette, same single blue signal color, same spacing grid as the dashboard and
  the other apps.
- **The version on the page is the version that is actually running** — it
  reflects the deployed binary's build version, so the operator can confirm a
  deploy or rollback in a browser.

## Success criteria (outcomes)

Each is a result the viewer or operator can confirm against the running service:

- As a logged-in dashboard user I open `<account>.ikigenba.com/srv/notify/` and
  see a Carbon-styled page showing the service name `notify` and the running
  version.
- As a browser with no dashboard session I open `/srv/notify/` and am refused
  with `401`, not shown the page.
- The version shown on the page matches the version the deployed binary reports.
- The page's fonts and colors match the suite design system (Carbon), and the
  page loads its own embedded `tokens.css` and fonts, not the dashboard's.
- An MCP client still discovers the AS via the PRM well-known and calls the
  bearer-gated `/mcp` exactly as before; the landing page changed nothing for it.
- The bearer-gated `/srv/notify/mcp` still requires a token and `/health` still
  responds — the landing page shadowed neither.
