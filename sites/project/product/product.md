# sites — Product

**Authority: intent.** This document owns *why* sites exists, *for whom*, what is
in and out of scope, and what we **promise** the user — in outcome terms only.
Mechanism (the handler, the in-process static server, the DB schema, the nginx
fragment, the MCP tool shapes, the embedded landing template) and its checkable
proof live in `project/design/design.md`. Where the two touch observable
behavior, product states the *promise* and design states the *exact, checkable
form*; that boundary keeps product, design, and plan from overlapping.

> **History note.** sites originally shipped a **publish/unpublish lifecycle**: a
> site had an editable *working tree*, and "publishing" it created a symlink into
> a per-visibility *served tree* that **nginx served straight off disk** via
> `alias`. That machinery is being removed (it duplicated in the filesystem a
> fact the database already held, purely so nginx — which cannot read the DB —
> could serve it). This product doc states the **current** model; the publish
> lifecycle, the working tree, the served symlinks, and disk-serving by nginx are
> gone. History of the change lives in the plan.

## Problem

A single-box customer wants to host a handful of small static websites — a
marketing page, an internal runbook, a private scratch site — without standing up
separate hosting, and to manage them the same way they manage everything else on
the box: by talking to an agent over MCP. They also, occasionally, open the
service in a browser to see what sites exist and confirm the service is up.

The earlier design solved the hosting need but accreted complexity an agent
added: a create → edit-a-working-tree → **publish-to-a-tier** lifecycle, with the
published content represented as a **symlink tree served off disk by nginx**.
That is more moving parts than the job needs — the database already knows which
sites exist and whether each is public, so re-encoding that as an on-disk symlink
tree for nginx's benefit is redundant state that can drift.

## Purpose

sites is the box's **static-website host**. Each site is a slug-named folder of
files. A site is either **public** (served to anyone) or **private** (served only
to a logged-in dashboard user) — that is the whole of its state. The owner
manages sites through the `ikigenba_sites_*` MCP surface (create, edit files,
set public/private, delete); the **sites process itself serves the site bytes**
over its loopback HTTP server, behind the nginx front door. A human who opens the
service root in a browser gets a **landing page** that shows the service version
and lists the sites that exist.

## Users

- **The owner, through an agent (MCP).** Creates sites, edits their files, sets
  each public or private, and deletes them — all as MCP tool calls. This is the
  primary surface.
- **A logged-in dashboard user, in a browser.** Opens the bare `/srv/sites/` root
  and sees the landing page: the running version and the list of sites (slug,
  public/private, who created it, when). The check is coarse — any logged-in
  dashboard user may view it.
- **A visitor to a public site.** Anyone on the internet who opens a public
  site's URL and is served its files. A **private** site's files are served only
  to a logged-in dashboard user.
- **The operator, confirming a deploy.** Opens the root after a deploy to confirm
  sites is up and which version is live.

## Scope

sites does this and only this:

- **Host static sites in two visibilities.** A site's files live under one of two
  places and are served accordingly: **public** (served with no authentication)
  or **private** (served only to a logged-in dashboard user). Which one a site is
  is a single boolean the owner sets; there is no third "unpublished/draft" state
  and no separate publish step — a site that exists is served.
- **Serve site bytes in-process.** The sites process serves the files for both
  visibilities from its own loopback server; nginx proxies to it and never reads
  the site files off disk itself.
- **Manage sites over MCP.** Create a site, edit its files with the file tools,
  flip it public↔private, and delete it — through the `ikigenba_sites_*` tools.
  Content edits are **immediately live** (there is no working-copy/publish
  indirection).
- **Record who and when.** Each site records the owner who created it and the
  creation time, surfaced by the tools and on the landing page.
- **Serve a landing page at the bare mount root.** A dynamic, session-gated page
  showing the service version and the list of existing sites (slug, visibility,
  creator, created-at), styled with the suite's Carbon design system.
- **Import from a Dropbox mirror.** The `sync` tool reconciles a Dropbox-mirrored
  subtree into a site's files (unchanged behavior; it now writes directly into
  the site's served folder rather than a separate working tree).

It deliberately does **nothing else**. In particular it does not: keep any
publish/unpublish lifecycle, working tree, or served-symlink tree; let nginx
serve site files off disk; offer any "draft" or "offline-but-exists" state; run
any token or session logic itself (nginx is the sole trust boundary); or host
anything but static files.

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **A site is public or private — a binary, never a "tier" spectrum.** There is
  no third state. "Not public" means private (served only to a logged-in
  dashboard user).
- **A site that exists is served.** There is no publish step and no unpublished
  state: creating a site and putting files in it makes it live; deleting it takes
  it offline. Flipping public↔private is the only visibility control.
- **sites serves every byte under its mount.** For any path under
  `/srv/sites/…`, the bytes come from the sites process — nginx proxies, it does
  not serve site files off disk.
- **The visibility gate is nginx's.** Public site paths are unauthenticated;
  private site paths are gated by the dashboard browser session
  (`auth_request /_session-authn`). The sites process runs no token/session logic
  and trusts the front door.
- **The landing page lives at the bare mount root only**, is gated by the
  dashboard browser session (not a bearer token), and shows the running version
  plus the list of sites. A failed session check yields `401`.
- **The visual system is Carbon.** `design/carbon.md` + `design/tokens.css` +
  `design/example.html` are the source of truth; sites embeds its own copy of the
  tokens and fonts.

## What we promise (user-facing behavior)

- **Creating a site and adding files makes it live** — with no separate publish
  step. The owner creates a slug, writes files into it, and the site is served at
  its URL immediately.
- **Public sites are served to anyone; private sites only to a logged-in user.**
  Opening a public site's path returns its files with no login; opening a private
  site's path without a dashboard session is refused with `401`.
- **Flipping a site public↔private changes who can reach it** — one tool call,
  and the site's URL and access change accordingly; it is never reachable as both
  at once.
- **Deleting a site takes it offline** — its files stop being served and its
  record is gone. There is no lingering "unpublished" state; delete is how a site
  goes away.
- **The landing page lists the sites that exist** — a logged-in human opening the
  bare `/srv/sites/` sees the running version and a row per site showing its slug,
  whether it is public or private, who created it, and when.
- **A browser with no dashboard session is refused the landing page** — `401`,
  not the page.
- **Agents are unaffected in how they connect** — the bearer-gated `/mcp`, the
  PRM well-known, and `/health` behave as before; only the *tools* change (no
  `publish`/`unpublish`; visibility is a flag).
- **The version on the page is the version actually running** — so the operator
  can confirm a deploy in a browser.

## Success criteria (outcomes)

Each is a result the owner, viewer, or operator can confirm against the running
service:

- As the owner I create a site, write an `index.html` into it, and immediately
  fetch its URL and get that page — with no publish step.
- A site I set **public** is served to a request with no dashboard session; a site
  I set **private** returns `401` to a request with no session and its files to a
  request with a valid session.
- I flip a site from private to public (or back) with one tool call and its
  reachability changes accordingly; it is never served under both visibilities at
  once.
- I delete a site and its URL stops serving its files.
- As a logged-in dashboard user I open `<account>.ikigenba.com/srv/sites/` and see
  a Carbon-styled page showing the running version and a row for each site with
  its slug, public/private status, creator, and creation time.
- As a browser with no dashboard session I open `/srv/sites/` and am refused with
  `401`.
- Every path served under `/srv/sites/…` is served by the sites process — nginx
  holds no `alias` and reads no site files off disk.
- An MCP client still discovers the AS via the PRM well-known and calls the
  bearer-gated `/mcp`; `/health` still responds.

## Addendum — registry adoption (internal address resolution)

A separate, cross-cutting change (beyond the hosting model) removes the loopback
**port literals** sites hardcodes in its Go composition root. sites resolves both
its own port and the dropbox mirror address **by name** through the shared,
authoritative `registry` table, asked once at startup. This is purely internal
and **behavior-preserving**; the existing `SITES_PORT` / `DROPBOX_BASE_URL` env
overrides still take precedence. The promise it adds is operational:

- **sites's loopback addresses cannot silently drift.** The port sites answers on
  and the dropbox address it fetches through come from the one authoritative
  registry, not a literal that can fall out of step with the rest of the suite.

The nginx front-door fragment (`etc/nginx.conf`) keeps its literal port — nginx
reads that config directly and cannot consult a Go library.
