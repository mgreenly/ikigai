# dropbox — Product

**Authority: intent.** This document owns *why* dropbox exists, *for whom*, what
is in and out of scope, and what we **promise** — in outcome terms only.
Mechanism (the handlers, the upload queue, the mirror layout, the Dropbox API
calls, the Carbon tokens, the route patterns) and its checkable proof live in
`project/design/`. Where product and design touch observable behavior, product
states the *promise* and design states the *exact, checkable form*; that boundary
keeps product, design, and plan from overlapping.

> **Scope note.** dropbox now serves two product threads, and this doc covers
> both as one coherent statement:
>
> 1. **The human web landing page** — the session-gated front door under
>    `/srv/dropbox/` showing the service name and running version.
> 2. **The bidirectional, service-facing filesystem** — dropbox is the suite's
>    shared file store: a single folder every service can read *and* write, kept
>    in sync with the owner's Dropbox app folder in **both** directions.
>
> **Out of scope here:** wiring the suite's *other* services to call the
> filesystem API (each service adopts it in its own `project/` later — this
> covers only dropbox's own capability). The lower-level download-engine
> correctness rules (crash/replay, cursor, case-folding) are mechanism, owned by
> design and `dropbox/CLAUDE.md`.

## Problem

Every ikigenba service runs as an isolated process with its own database and no
shared place to put files. dropbox already keeps a **private, read-only** mirror
of the owner's Dropbox app folder — but that mirror was a dead end for the rest
of the suite: a service could not create a file in it, and nothing a service
produced (an export, a rendered artifact, an email attachment, a script output)
could reach the owner's Dropbox. Files could flow **down** (Dropbox → box) but
never **up** (service → Dropbox), and services had no sanctioned way to read or
write the shared folder at all. Meanwhile a human who opened `/srv/dropbox/` in a
browser saw nothing useful — no token in a browser meant the bearer gate refused
them, and there was no human-facing page there.

## Purpose

dropbox is the suite's **shared file store and its Dropbox bridge**. It owns one
folder that mirrors the owner's Dropbox app folder, exposes that folder to the
suite's services as a read/write/discovery **API** (dropbox stays the sole owner
of the bytes — services ask dropbox, they never touch the raw files), and keeps
the folder and Dropbox in sync **both ways**: changes made in Dropbox flow down,
and changes made by any service flow up. It also serves a minimal human landing
page confirming the service is deployed and which version is live. The product
surface for agents is **MCP**; the surface for on-box services is a set of
loopback endpoints; the human surface is the landing page.

## Users

- **A suite service, reading and writing files.** Any on-box service (crm, wiki,
  scripts, gmail, sites, …) that needs to store a file where the owner can see it
  in Dropbox, or read a file the owner or another service placed there. It
  creates, reads, overwrites, deletes, moves, and lists files and directories by
  calling dropbox.
- **An off-box agent, over MCP.** An agent with no local mount browses, fetches,
  and also creates, overwrites, deletes, and moves files through dropbox's MCP
  tools. Writes are **reference-based**: the agent names a file another suite
  service holds and dropbox moves the bytes itself, so the file never passes
  through the agent; small inline writes remain a convenience.
- **The owner and other people, through Dropbox directly.** They see the folder
  as any Dropbox user does. By convention they treat it as read-only, or add
  files only in designated **inbox** folders; the suite is the authority on the
  rest.
- **A future integrator agent.** An agent later wiring another service to use the
  filesystem, who needs a precise reference for every endpoint it will call.
- **A logged-in dashboard user / the operator, in a browser.** Opens
  `/srv/dropbox/` to confirm the service is up and which version is running.

## Scope

dropbox does this and only this:

- **Serve the shared folder to services as an API** — create, read, overwrite,
  delete, move, and discover (list / stat / walk) files **and** directories.
  Directories are real, including **empty** ones.
- **Sync both directions** — changes originating in Dropbox continue to flow down
  into the mirror (unchanged); changes originating from a service flow **up** to
  Dropbox. The suite is the **authority**: a service write wins and overwrites
  the Dropbox copy (last-writer-by-sync-order); Dropbox is a replica that
  converges to the suite's current state.
- **Return writes immediately, propagate durably** — a write succeeds as soon as
  it is safe on the box; reaching Dropbox happens in the background and survives
  restarts and Dropbox outages, and a push that cannot complete stays visible
  rather than being lost.
- **Move large files without exhausting memory** — reads, writes, and uploads
  stream, so a large image moves within a small, fixed memory footprint on a
  memory-constrained box.
- **Tell consumers what changed and who changed it** — every file change emits an
  event that names the change and its **origin** (which service wrote it, or
  Dropbox).
- **Ship an integrator reference** — a document describing every filesystem-API
  endpoint and its behavior, so a future agent can wire another service without
  reading the source.
- **Serve the human landing page** — a `GET` of `/srv/dropbox/` returns a
  Carbon-styled page showing the service name and running version, gated by the
  dashboard browser session.

It deliberately does **nothing else**: it does not give services raw filesystem
access to the bytes (they go through dropbox); it does not preserve conflicting
Dropbox edits as separate copies (the suite wins, overwrite); it does not offer a
real on-disk path / mount for tools that need one (a service does that work in
its own temp space and transfers final artifacts); it does not enforce which
service may write where (the shared namespace is organized by convention, not
walls); it does not wire any other service to use the API; and it does not expose
the sync internals, the event feed, health, MCP bootstrap, or the landing page as
part of the filesystem-API reference.

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **The suite is the authority; Dropbox is a replica.** A service write is pushed
  up with **overwrite** semantics and always wins; Dropbox converges to the
  suite's current state. Conflicting concurrent Dropbox edits are not preserved
  as separate copies.
- **The shared folder is one namespace, organized by convention.** Any service
  may read or write anywhere under the folder; there is no per-service
  enforcement. Human read-only / inbox usage is convention, not enforced.
- **A write is durable locally before it is on Dropbox.** The write returns on
  local commit; the upload is asynchronous, coalescing (only the latest version
  of a path need reach Dropbox), and never silently dropped on failure.
- **Agent writes are reference-based; bytes never cross the agent.** MCP `put`
  primarily takes a reference to bytes another suite service holds, and dropbox
  fetches them itself — from the box's own services only, never the open
  internet — so a referenced file's size is not bounded by the agent. The
  inline convenience form is capped at **25 MiB** (like `get`).
- **The filesystem-API reference ships under `dropbox/docs/`.** It covers the
  filesystem-interaction endpoints only, and its completeness is mechanically
  enforced.
- **The landing page lives at the mount root only, gated by the dashboard
  session.** A human reaches it at `<account>.ikigenba.com/srv/dropbox/`; an
  unauthenticated browser gets `401`; the gate is coarse (any logged-in dashboard
  user). v1 content is exactly the service name + running version.
- **The visual system is Carbon.** dropbox embeds its own copy of the Carbon
  tokens and fonts.

## What we promise (user-facing behavior)

- **A service can put a file into the shared folder and the owner sees it in
  Dropbox** — it creates or overwrites a file through dropbox, and that file
  appears in the owner's Dropbox app folder without the service doing anything
  else.
- **A service can read, delete, move, and list files and directories** — the
  folder behaves like a real filesystem: create an empty directory and it exists;
  move a large file and it relocates without re-transferring its bytes; delete a
  directory and its whole subtree goes.
- **Writes are fast and survive outages** — a write returns immediately on the
  box; if Dropbox is down or slow, the write still succeeds and the change is
  pushed later, and a push that keeps failing shows up as a visible backlog, not
  a silent loss.
- **Large files move without blowing memory** — moving a big image in or out does
  not load the whole file into memory.
- **Consumers can tell who changed a file** — each change event says whether a
  service wrote it (and which one) or whether it came from Dropbox, so a service
  can ignore its own writes.
- **An off-box agent can write, not just read** — through MCP it can create,
  overwrite, delete, and move files, symmetric with browsing and fetching. It
  can hand `put` a reference to a file another suite service holds and the file
  lands in the folder whatever its size; it can also inline small content it
  authored itself. A reference pointing anywhere but the box's own services is
  refused.
- **Consumers can subscribe precisely** — each change event is addressed by the
  operation and the file's place in the folder, so a consumer (a trigger, a
  workflow) can select exactly the files it cares about — e.g. only PDFs
  created under `/bills` — and is not woken by every other change on the box.
- **A future integrator has a complete API reference** — `dropbox/docs/`
  documents every filesystem endpoint, its parameters, and its behavior, and the
  reference cannot silently fall out of date as endpoints are added.
- **A logged-in human who opens `/srv/dropbox/` sees a real page** — the service
  name and running version, on the suite's design system; an unauthenticated
  browser gets `401`; the version shown is the version actually running.
- **The machine surfaces are unaffected** — the bearer-gated `/mcp`, the PRM
  well-known, `/health`, and the loopback `/feed` behave as before; the download
  direction and its guarantees are unchanged.

## Success criteria (outcomes)

Each is a result a service, agent, operator, or viewer can confirm against the
running service:

- A service creates a file through dropbox and, after the sync settles, the file
  is present in the owner's Dropbox app folder with the same contents.
- A service overwrites an existing file and the Dropbox copy converges to the new
  contents (the suite's version wins); it is not turned into a conflicted copy.
- A service creates an **empty** directory and a later `list`/`stat` shows that
  directory; deleting the directory removes its whole subtree.
- A service moves a large file and it relocates in Dropbox without re-uploading
  its bytes; a large file transfers in or out without the service exhausting
  memory.
- A write returns success while Dropbox is unreachable, and once Dropbox is back
  the file appears there; a push that cannot complete is visible in the service's
  health backlog rather than lost.
- A consumer reading the event feed can see, for each change, whether it was
  written by a specific service or came from Dropbox.
- An off-box agent uses the MCP `put`/`mkdir`/`delete`/`move` tools to change the
  folder; a `put` given a reference to a file held by another suite service
  lands that file's exact bytes without them passing through the agent, while a
  reference to anything outside the box's own services is refused; an inline
  `put` over 25 MiB is refused as too large.
- A consumer subscribed to one folder pattern (e.g. PDFs created under
  `/bills`) receives the event for a matching file and receives nothing for a
  file created elsewhere in the folder.
- `dropbox/docs/` documents every filesystem-interaction endpoint, and adding a
  new such endpoint without documenting it fails a check.
- As a logged-in dashboard user I open `/srv/dropbox/` and see a Carbon-styled
  page showing the service name `dropbox` and the running version; with no
  session I am refused with `401`.
- The bearer-gated `/mcp`, the PRM well-known, `/health`, and `/feed` behave
  exactly as before; the download direction still pulls Dropbox changes into the
  mirror.
