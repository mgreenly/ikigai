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
- **Manage sites over MCP.** Create a site — private by default, or public in a
  single step — edit its files with the file tools, flip it public↔private, and
  delete it, through the `ikigenba_sites_*` tools. Content edits are
  **immediately live** (there is no working-copy/publish indirection).
- **Record who and when.** Each site records the owner who created it and the
  creation time, surfaced by the tools and on the landing page. Because `create`
  is the only way a site comes into being, **every site has a real creator** —
  there are no anonymously-imported sites.
- **Serve a landing page at the bare mount root.** A dynamic, session-gated page
  showing the service version and the list of existing sites (slug, visibility,
  creator, created-at), styled with the suite's Carbon design system. The list is
  **browsable in the page**: a fuzzy search box filters by slug, the name /
  created / creator columns sort (click a header to change direction), results
  **paginate past ten** rows, and a single control clears the filter and ordering
  back to the default view.
- **Describe itself to a connecting agent.** The MCP surface is self-describing:
  its connection instructions name what sites is for in everyday words and point
  at a `guide` tool that returns the site model and worked examples, so an agent
  can discover and drive sites from the connection alone, with no external skill.
- **Import from a Dropbox mirror.** The `sync` tool reconciles a Dropbox-mirrored
  subtree into an **already-created** site's files (the site must be created
  first; `sync` never brings a site into being). It writes directly into the
  site's served folder and leaves the site's visibility unchanged.

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
  it offline. Choosing public/private at create, and flipping it thereafter, are
  the only visibility controls.
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
  its URL immediately. A site is **private by default**; the owner may instead
  create it **public in a single call** when it should be world-visible from the
  start.
- **An agent can learn sites from the connection alone** — on connecting, the
  instructions say what sites is for in the words a user actually uses, and a
  `guide` tool returns the site model, the rules, and worked examples (including
  creating a public page in one call and importing from Dropbox). No external
  skill or doc is needed to route work to sites or to drive its tools.
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
  whether it is public or private, who created it, and when. Each slug is a link
  that opens that site.
- **A browser with no dashboard session is refused the landing page** — `401`,
  not the page.
- **Agents are unaffected in how they connect** — the bearer-gated `/mcp`, the
  PRM well-known, and `/health` behave as before. The *tools* evolve: `create`
  takes an optional public flag, `sync` requires the site to already exist, and
  the self-description tool is `guide` (returning worked examples) rather than
  `describe`.
- **The version on the page is the version actually running** — so the operator
  can confirm a deploy in a browser.
- **The landing list is browsable in place.** A logged-in user can **type in a
  search box to fuzzily filter the sites by slug** (partial, out-of-order letters
  still match), **sort by name, created-at, or creator by clicking a column
  header** (clicking again reverses the direction), and **page through the
  results ten at a time** once there are more than ten. A single **Clear** action
  returns to the default view (no filter, newest-first, first page). Filtering,
  sorting, and paging happen **instantly in the browser** with no page reload;
  the default view is newest-first. This is a convenience for the human viewer —
  it changes nothing an agent sees over MCP.

## Success criteria (outcomes)

Each is a result the owner, viewer, or operator can confirm against the running
service:

- As the owner I create a site, write an `index.html` into it, and immediately
  fetch its URL and get that page — with no publish step.
- As the owner I create a site **public in one call**, and a request with no
  dashboard session is immediately served its page; a site I create with no
  visibility flag is private and returns `401` to that same session-less request.
- As an agent I call `sync` for a site that does not exist yet and get a clear
  "not found — create it first" refusal, not a silently-created site.
- As an agent connecting to sites for the first time I can tell what it is for,
  and by calling `guide` I get worked examples that let me create and publish a
  site without any external instructions.
- A site I set **public** is served to a request with no dashboard session; a site
  I set **private** returns `401` to a request with no session and its files to a
  request with a valid session.
- I flip a site from private to public (or back) with one tool call and its
  reachability changes accordingly; it is never served under both visibilities at
  once.
- I delete a site and its URL stops serving its files.
- As a logged-in dashboard user I open `<account>.ikigenba.com/srv/sites/` and see
  a Carbon-styled page showing the running version and a row for each site with
  its slug, public/private status, creator, and creation time; clicking a slug
  opens that site.
- As a browser with no dashboard session I open `/srv/sites/` and am refused with
  `401`.
- As a logged-in user on the landing page I type part of a slug into the search
  box — including letters that are non-adjacent in the name — and the list narrows
  to just the matching sites as I type, with no page reload.
- As a logged-in user I click the Name, Created, or Creator column header and the
  list reorders by that column; clicking the same header again reverses the
  direction. The list opens sorted newest-first by default.
- As a logged-in user with more than ten sites (or more than ten matches after
  filtering) I see a Prev/Next pager with a "Page X of Y" readout and can page
  through ten at a time; with ten or fewer, no pager appears.
- As a logged-in user I click **Clear** and the search box empties, the ordering
  returns to newest-first, and I am back on the first page.
- As a logged-in user with JavaScript disabled I still see the complete list of
  sites (unfiltered, unsorted controls absent) — the page degrades to the plain
  listing rather than showing a broken search box.
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
