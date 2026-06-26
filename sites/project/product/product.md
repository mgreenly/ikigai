# sites — Product (landing page)

**Authority: intent.** This document owns *why* sites serves a web landing
page, *for whom*, what is in and out of scope, and what we **promise** the
viewer — in outcome terms only. Mechanism (the handler, the embedded template,
the Carbon tokens, the nginx fragment, the route pattern) and its checkable
proof live in `project/design/design.md`. Where the two touch observable
behavior, product states the *promise* and design states the *exact, checkable
form*; that boundary keeps product, design, and plan from overlapping.

> **Scope note.** This product doc covers **only** the new standardized
> landing page for sites — a dynamic, in-process card at the mount root. sites'
> existing domain (the `ikigenba_sites_*` MCP surface that publishes static
> websites, plus the public/private static tiers it already serves from disk)
> is owned by `sites/cmd/sites/main.go` and the sites notes; it is untouched
> here. This is the suite-wide landing-page change **applied to the one service
> that already served web pages.**

## Problem

The suite is evolving so that **every deployable app serves its own HTML web
pages**, beginning with a single, uniform landing page. For most services this
is a brand-new direction — they served only machine surfaces (PRM, `/health`,
the bearer-gated `/mcp`) and declared "no UI." **sites is the special case: it
already serves web pages.** sites is the loopback-only static-website host —
its whole domain is publishing HTML, and its nginx fragment already exposes a
**public** static tier (no auth) and a **private** static tier (gated by the
dashboard browser session) served from disk. What sites does *not* yet have is
the **standardized, dynamic landing card** that the rest of the suite is
adopting at the bare mount root: a human who opens `<account>.ikigenba.com/srv/sites/`
itself (the mount root, not a published-site path under `public/` or `private/`)
gets no on-system page that says *which service this is* and *what version is
running*.

This work adds exactly that card. It is the uniform v1 page every app gets — a
Carbon-styled service-name-and-version card — slotted at the bare mount root of
sites, beside (not on top of) the static tiers sites already serves. Each app's
landing page will later diverge to serve that app's specific purpose, so there
is **no shared landing handler** — sites owns its own page.

## Purpose

The sites landing page is the **standardized human front door** to the sites
service at the bare mount root `/srv/sites/`. For v1 it is intentionally
minimal: a single Carbon-styled card showing the **service name** and the
**running version**, rendered dynamically by the sites process (unlike the
public/private tiers, which are static files served straight off disk by nginx).
It is gated by the viewer's **dashboard browser session** (the login cookie),
not by a bearer token — because a browser cannot present a bearer token, and
because a name-and-version page warrants only a coarse "are you a logged-in user
of this box" check, never a per-resource authorization. Crucially for sites,
this gate is **already present**: sites' private static tier already uses
`auth_request /_session-authn`, so the landing page **reuses an
already-wired gate** and introduces **no new suite dependency** for sites. The
page proves the service is deployed, reachable, and on-system, and it
establishes the seam (handler + embedded template + embedded design assets) that
every later sites-specific web page grows from.

## Users

- **A logged-in dashboard user, in a browser.** Any human authenticated to this
  box's dashboard who navigates to the bare `/srv/sites/` root. They see the
  service name and version on the Carbon design system. The check is deliberately
  **coarse**: any logged-in dashboard user may view any app's landing page —
  there is no per-resource or per-owner authorization on this page.
- **The operator, confirming a deploy.** Opens the mount root after a deploy or
  rollback to confirm sites is up and which version is live — a browser-visible
  liveness signal that complements the machine `/health` and `version` checks.

The page is **not** for agents or MCP clients — those keep using the
bearer-gated `/mcp` endpoint, which is unchanged — and it is **not** the place a
published website is viewed (those live under the `public/`/`private/` tiers).

## Scope

The sites landing page does this and only this:

- **Serve one dynamic landing page at the bare mount root** — a `GET` of the
  bare `/srv/sites/` root returns an HTML page rendered by the sites process.
  Internally (nginx strips the mount prefix) the service answers this at its
  exact root path `/`.
- **Show the service name and version** — the page displays the service name
  (`sites`) and the running version, taken from the values the chassis already
  exposes. Nothing else is shown in v1.
- **Look like the suite** — the page is styled with the **Carbon** design system:
  monochrome neutrals, blue `#2563EB` as the only signal color, the Space
  Grotesk / IBM Plex Sans / IBM Plex Mono type pairing, the 4px spacing grid. A
  simple centered card: service name in display type, version as a mono label.
- **Carry its own design assets** — sites embeds its **own** copy of the Carbon
  `tokens.css` and the woff2 fonts under its static directory and serves them
  from its own mount; it does not depend on the dashboard's assets at runtime.
- **Gate humans by the dashboard session cookie** — the page is reachable only by
  a viewer whose `dashboard_session` cookie validates against the dashboard's
  web-session store. An unauthenticated browser gets `401`. This is the **same**
  coarse session gate sites **already uses** for its private static tier — no new
  gate, no new dependency.

It deliberately does **nothing else** in v1 — in particular it does not: perform
any per-resource or per-owner authorization (the session gate is coarse by
design); expose any sites domain data, published-site listing, or file on the
page; add or change any MCP tool; touch the public or private **static tiers**
or what they serve; alter the bearer-gated `/mcp`, the PRM well-known, the
`@sites_authn_500` rate-limit re-emit, or `/health`; or share a landing handler
with any other service. Later sites-specific web pages are **out of scope** for
this work — this establishes only the uniform v1 page and the seam they will
grow from.

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **The landing page lives at the bare mount root only.** A human reaches it at
  `<account>.ikigenba.com/srv/sites/`; the service answers it at its exact root
  path `/` and nowhere else. It never shadows `/mcp`, `/health`, the PRM
  well-known, or the `public/`/`private/` static tiers (which are paths **under**
  the mount, not the bare root).
- **The page is gated by the dashboard browser session, not by a bearer token.**
  The gate is `auth_request /_session-authn` (the dashboard-owned, loopback-only
  cookie validator) — never `/_authn` (the bearer gate). A failed session check
  yields `401`. sites **already uses this exact gate** for its private static
  tier, so the landing page reuses an already-present mechanism.
- **The gate is coarse.** Any logged-in dashboard user may view the page; there
  is no per-resource check. This is acceptable precisely because the page reveals
  only the service name and version.
- **v1 content is exactly: service name + running version.** No more. The values
  come from what the chassis already exposes (`rt.Service()` / `rt.Version()`);
  the page adds no new data source.
- **Each app owns its own landing page.** There is no shared landing handler;
  sites' page code, template, and embedded assets live under `sites/`.
- **The visual system is Carbon.** `design/carbon.md` (rules) + `design/tokens.css`
  (tokens) + `design/example.html` (reference) are the source of truth; sites
  embeds its own copy of the tokens and fonts.

## What we promise (user-facing behavior)

- **A logged-in human who opens the bare `/srv/sites/` sees a real page** — the
  sites service name and the running version, on the suite's design system, not a
  raw proxy error, a directory listing, or a blank page.
- **A browser that is not logged in is refused** — an unauthenticated browser
  hitting `/srv/sites/` gets `401`, because the page is gated by the dashboard
  session cookie.
- **Agents are unaffected** — the bearer-gated `/mcp` endpoint, the PRM
  well-known, and `/health` behave exactly as before; the landing page is added
  beside them, shadowing none of them.
- **The static tiers are unaffected** — the public tier (`/srv/sites/public/`)
  and the private tier (`/srv/sites/private/`) still serve their files from disk
  exactly as before; the exact-match landing root carves out only the bare root,
  not any path under those folders.
- **The page looks like the rest of the suite** — same fonts, same neutral
  palette, same single blue signal color, same spacing grid as the dashboard and
  the other apps.
- **The version on the page is the version that is actually running** — it
  reflects the deployed binary's build version, so the operator can confirm a
  deploy or rollback in a browser.

## Success criteria (outcomes)

Each is a result the viewer or operator can confirm against the running service:

- As a logged-in dashboard user I open `<account>.ikigenba.com/srv/sites/` and
  see a Carbon-styled page showing the service name `sites` and the running
  version.
- As a browser with no dashboard session I open `/srv/sites/` and am refused with
  `401`, not shown the page.
- The version shown on the page matches the version the deployed binary reports.
- The page's fonts and colors match the suite design system (Carbon), and the
  page loads its own embedded `tokens.css` and fonts, not the dashboard's.
- An MCP client still discovers the AS via the PRM well-known and calls the
  bearer-gated `/mcp` exactly as before; the landing page changed nothing for it.
- Opening a published-site path under `/srv/sites/public/…` or
  `/srv/sites/private/…` still serves the static file from disk, and `/health`
  still responds — the landing page shadowed none of them.
