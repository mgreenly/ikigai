# wiki — Build Plan (phased, for sequential subagent dispatch)

> Status: **build plan**. Companion to `GOALS.md` (design intent). This document
> breaks the work into **phases**, each phase into **tasks**, where **one task =
> one subagent run**. The orchestrator dispatches tasks **strictly in sequence,
> never in parallel** — each task's *Acceptance* gate must pass before the next
> task starts. Nothing here is parallelized; do not try.
>
> On any conflict, `GOALS.md` and the metaspot specs win. Read order for a
> subagent picking up a task: this file's task brief → `wiki/GOALS.md` → the
> repo `CLAUDE.md` → the named "mirror" service(s) for that task.

---

## How the orchestrator should use this document

For each task, in order:

1. **Dispatch one subagent** with the task's *Brief*, *Inputs*, *Deliverables*,
   and *Acceptance*. Tell it to stay in the repo root (`/mnt/projects/ikigai/add_wiki`,
   the `add_wiki` worktree) and use relative paths — never `cd`.
2. **Gate on Acceptance.** When the subagent returns, verify the acceptance
   criteria yourself (build, tests, the named check). Only on pass do you
   advance. On fail, re-dispatch with the gap, or fall back per the task's notes.
3. **Commit between phases** (not between every task unless useful). Suggested:
   one commit per phase with a message naming the phase.
4. **Carry context forward** via the task *Inputs* — later tasks name the
   artifacts earlier tasks produced.

Conventions every task inherits (from repo `CLAUDE.md` + the metaspot specs):

- Go module per service; `replace eventplane => ../eventplane` committed; root
  `go.work` wires local dev but `bin/build` must not depend on it.
- `bin/build` cross-compiles `CGO_ENABLED=0 GOOS=linux GOARCH=amd64`. **Every Go
  dependency must be pure-Go.** SQLite is always `modernc.org/sqlite`.
- A service is loopback-bound, path-routed under `/srv/<svc>/`, owner-scoped, and
  trusts injected `X-Owner-Email` / `X-Client-Id` blindly. nginx is the only
  trust boundary.
- Migrations are **immutable once applied**: additive, higher-numbered only.
- Secrets only ever flow `~/.secrets/<NAME>` → `.envrc`/SSM app-config → env var;
  **never read a secret's contents**.
- **If a service gets its own `.envrc`, its first line must be `source_up`.** The
  repo-root `.envrc` exports `GOFLAGS=-buildvcs=false` (the worktree's bare repo
  lives in the parent dir, so Go's VCS stamping walks up, runs `git status` on a
  bare repo, and dies with exit 128 — aborting every plain `go build`/`go run`).
  A service `.envrc` without `source_up` shadows the root and loses `GOFLAGS`,
  breaking builds. Services with no `.envrc` (crm, ledger) inherit it directly
  via direnv's walk-up — so don't add a wiki `.envrc` until there's a
  service-specific var to put in it (Phase 4's `ANTHROPIC_API_KEY`). Each
  service dir needs a one-time `direnv allow`.

---

## Decisions made up front (override any of these before Phase 0 if wrong)

These resolve the open forks in `GOALS.md` enough to make the plan actionable.
They are recommendations with reasoning, not edicts — veto and I'll re-shape.

1. **qmd integration → reimplement a thin BM25 store on the suite's own
   `modernc.org/sqlite` FTS5; do *not* vendor or shell out to qmd.**
   *(Confirmed by the owner 2026-06-04 — settled, not open.)*
   *Why:* qmd's `store` is a Go `internal/` package (not importable across
   modules) **and** its vector path depends on CGO (`asg017/sqlite-vec`) + a
   llama.cpp embedder — both collide head-on with the suite's hard
   `CGO_ENABLED=0` cross-compile and its pure-Go `modernc.org/sqlite` standard.
   Shelling out to a separately-built qmd binary breaks the "one self-contained
   Go binary at `/opt/<app>`" convention and the deterministic cross-compile.
   For **BM25-only Phase 1** we need just FTS5 + `bm25()`, which `modernc.org/sqlite`
   provides. We borrow qmd's *schema and query shape* as a reference, behind a
   `internal/search` interface, so hybrid/vector (which genuinely needs extra
   infra) is a later additive swap. **Phase 0 spike confirms modernc FTS5/`bm25()`
   before any of this is load-bearing.** (Fork → akhenakh/qmd as the *reference*,
   since it's the one in hand and ralph-wikis already used it BM25-only.)

2. **agentkit extraction is a *physical move* of the generic engine packages +
   a *greenfield, tested* job-runner; ralph is retrofitted LATER.**
   Moving `ralph/internal/engine/*` into `agentkit/` forces a mechanical rewrite
   of ralph's import paths (or the mono-repo won't compile) — that rewrite is
   done in the same task and is *not* the deeper retrofit. ralph keeps its own
   `runner`/`session` for now. agentkit's generic async job-runner is built
   fresh (informed by `ralph/internal/runner`, behind a store interface) and
   **wiki is its first consumer**; ralph migrates onto it in a Later phase. This
   matches GOALS: "wiki builds against it first; ralph is retrofitted afterward."

3. **Ingest agent model + cost ceiling:** default to a mid-tier model
   (`claude-sonnet-4-6`) with a per-job token/cost ceiling read from manifest/
   env config, not hardcoded. Flagged for confirmation at Phase 4.

4. **Data tree is owner-scoped and collection-keyed from day one**, defaulted to
   `collection="default"`, with **no `collection` argument on the verbs yet** (so
   splitting wikis later is additive). On-disk layout pinned in Phase 3.

5. **Phase-1 ingest ships `wiki_ingest_text` first, then `wiki_ingest_url`** as a
   sub-task; both converge on one async core.

**Phase ↔ GOALS mapping:** GOALS "Phase 1" = Phases 0–5 here (groundwork →
agentkit → scaffold → model/search → ingest → search/lint/demo/deploy). GOALS
"Phase 2" = Phase 6. GOALS "Later" = Phase 7.

---

## Phase 0 — Groundwork & de-risking spikes

*Goal: confirm the two load-bearing assumptions before any large code move.
Output is design notes + tiny proofs, no production code.*

### Task 0.1 — Verify the agentkit extraction seam
- **Brief:** Read `ralph/internal/engine/**` and `ralph/internal/runner`,
  `ralph/internal/session`, `ralph/internal/sandbox`. Produce a concrete
  **move-list** (every package/file that goes to `agentkit` vs stays in ralph)
  and the **job-runner seam design**: the Go interface(s) by which a generic
  agentkit job-runner persists/loads run records and enforces single-flight +
  crash-recovery, so both ralph and wiki can supply their own store. Identify
  anything that does NOT lift cleanly at the GOALS boundary and how to split it.
- **Inputs:** `GOALS.md` (Architecture decisions → Seam); the structural map in
  this repo; `ralph/internal/engine/agent/loop.go`,
  `ralph/internal/engine/provider/anthropic/anthropic.go`,
  `ralph/internal/engine/tools/{tools.go,dispatch.go,confine.go}`,
  `ralph/internal/engine/{wire,model,schema,trace}`, `ralph/internal/runner/runner.go`,
  `ralph/internal/session/{model,service,store}.go`, `ralph/internal/sandbox/sandbox.go`.
- **Deliverables:** `wiki/notes/agentkit-extraction.md` — move-list table +
  the job-runner store interface (Go signatures) + risk notes (esp. runner↔
  session/store coupling, the two confinement copies).
- **Acceptance:** The note names exact source paths and target `agentkit/`
  package paths for every generic package, and gives a compilable-looking
  interface for the job store. No code moved yet.

### Task 0.2 — Confirm the BM25 search backend on `modernc.org/sqlite`
- **Brief:** Prove that `modernc.org/sqlite` supports FTS5 + the `bm25()`
  ranking function under `CGO_ENABLED=0`. Write a throwaway test that creates an
  `fts5` virtual table, indexes a couple of markdown blobs, and runs a
  `... ORDER BY bm25(tbl)` query returning ranked rows. Borrow qmd's
  `internal/store` schema/query shape as reference (`/home/mgreenly/projects/git/akhenakh/qmd/internal/store/store.go`).
  Decide & record the `internal/search` interface wiki will use.
- **Inputs:** Decision 1 above; qmd store API (already mapped); the suite's
  existing `modernc.org/sqlite` usage (`ledger/internal/db/db.go`).
- **Deliverables:** `wiki/notes/search-backend.md` — confirmed yes/no on modernc
  FTS5/`bm25()`, the chosen `internal/search` interface (`Index`, `Search`
  returning whole-page results + scores), and the fallback (shell-out) only if
  modernc cannot do FTS5.
- **Acceptance:** A spike test compiled with `CGO_ENABLED=0` runs an FTS5 BM25
  query successfully (paste the output), OR the note documents the failure and
  selects the fallback. Spike code is discarded/not committed to the service.

---

## Phase 1 — Extract & harden `agentkit`

*Goal: a tested, shared `agentkit` Go module that owns the generic agent
machinery + a generic async job-runner. wiki will build against it. ralph stays
green throughout but is NOT yet retrofitted onto the new job-runner.*

### Task 1.1 — Move the generic engine into `agentkit`, rewire ralph
- **Brief:** Create `agentkit/` (`module agentkit`, `go 1.26`). Move the generic
  packages out of `ralph/internal/engine/**` per the Task 0.1 move-list:
  provider + anthropic client, tool-use loop (`agent`), base tools + dispatch +
  `confine`, `wire` codec, `model` registry, `schema`, `trace`, and the generic
  framing prompt. Mechanically rewrite ralph's imports
  (`ralph/internal/engine/...` → `agentkit/...`). Add `replace agentkit =>
  ../agentkit` to `ralph/go.mod`; add `./agentkit` to root `go.work`.
- **Inputs:** `wiki/notes/agentkit-extraction.md`; the source packages named there.
- **Deliverables:** new `agentkit/` module tree; ralph importing it.
- **Acceptance:** `go build ./...` clean across the workspace; **ralph's existing
  test suite passes unchanged** (`go test ./...` in ralph). No behavior change to
  ralph.
- **Notes:** This is a move + import-path rewrite only. Do NOT redesign anything
  here. ralph's `runner`/`session`/`sandbox` stay put (its runner now imports
  `agentkit/agent` instead of the in-tree path).

### Task 1.2 — Build agentkit's generic async job-runner
- **Brief:** Add a new `agentkit` package (e.g. `agentkit/job`) implementing the
  generic async agent-job lifecycle from the Task 0.1 seam design: spawn a run
  (goroutine + context cancellation), poll/status, single-flight gate, and a
  crash-recovery sweep — all behind a **store interface** the consumer supplies
  (run records, terminal updates, sweep-running). Do **not** wire it into ralph.
- **Inputs:** `wiki/notes/agentkit-extraction.md`; `ralph/internal/runner/runner.go`
  and `ralph/internal/session/{service,store}.go` as the behavioral reference.
- **Deliverables:** `agentkit/job` package + an in-memory/stub store for tests.
- **Acceptance:** Unit tests cover spawn → succeed, spawn → cancel, single-flight
  rejection, and crash-recovery sweep; `go test ./agentkit/...` passes. ralph
  untouched and still green.

### Task 1.3 — Harden agentkit with tests
- **Brief:** Fill coverage gaps so wiki inherits a tested foundation, not
  ralph-as-today. Ensure the moved packages keep their ralph tests (moved along)
  and add a small **end-to-end agentkit test** using a stub provider: a tiny
  agent job that uses a base tool (e.g. write a file in a confined sandbox) and
  drives the `agentkit/job` runner to completion.
- **Inputs:** the moved test files; `agentkit/job` from 1.2.
- **Deliverables:** filled tests; one e2e job test with a stub provider (no real
  Anthropic call).
- **Acceptance:** `go test ./agentkit/...` green; the e2e test exercises
  loop → tool dispatch → confinement → job completion without network.

---

## Phase 2 — Scaffold the `wiki` service (clone the ledger chassis)

*Goal: a buildable, startable `wiki` service answering `wiki_whoami` behind the
auth gate on loopback 3006, wired into the suite's registry, dev front door, and
local-dev orchestrators. No domain logic yet.*

### Task 2.1 — Clone ledger → wiki and rename
- **Brief:** Copy `ledger/` → `wiki/` (preserving `wiki/GOALS.md`, this
  `PLAN.md`, and `wiki/notes/`). Rename every token: module `ledger`→`wiki`, app
  name, port `3002`→`3006`, mount `/srv/ledger/`→`/srv/wiki/`, db `ledger.db`→
  `wiki.db`, env prefixes `LEDGER_*`→`WIKI_*`, binary names. **Strip ledger's
  domain**: delete `internal/ledger/` (the 8 verbs) and the `/feed` producer
  route + outbox wiring (wiki is not an eventplane producer in Phase 1). Keep
  `db`, `ids`, `logging`, `mcp`, `server`. Reduce the MCP surface to **only
  `wiki_whoami`**. `go.mod`: `module wiki` + `replace eventplane => ../eventplane`
  + `replace agentkit => ../agentkit` (agentkit not yet imported, but declared).
  Migrations: keep `001_schema_migrations.sql`; add `002_wiki.sql` (placeholder/
  minimal — real schema lands in Phase 3).
- **Inputs:** `ledger/` (full chassis — mirror it); `notify/bin/secrets` (copy as
  the template for Phase 4's secret); repo `CLAUDE.md` per-service conventions.
- **Deliverables:** complete `wiki/` service tree: `bin/{build,deploy,setup,start,
  stop}`, `etc/{manifest.env,deploy.env,nginx.conf}`, `cmd/wiki/main.go`,
  `internal/{db,ids,logging,mcp,server}`, `go.mod`/`go.sum`.
- **Acceptance:** `etc/manifest.env` = `APP=wiki, MOUNT=/srv/wiki/, DEFAULT=false,
  PORT=3006, MCP=true`. `bin/build` produces `build/wiki` + `build/wiki.bin` under
  `CGO_ENABLED=0`. `bin/start` runs it on `127.0.0.1:3006`; a loopback
  `/whoami` with injected identity headers returns owner/client; `/mcp`
  `tools/list` shows `wiki_whoami` only.

### Task 2.2 — Wire wiki into the suite (go.work, registry, dev nginx, orchestrators)
- **Brief:** Add `./wiki` and `./agentkit` to root `go.work`. Confirm `bin/registry
  list-mcp` surfaces `wiki` and `bin/registry resource-url <domain> wiki` returns
  `https://<domain>/srv/wiki/mcp` (MCP=true). Add `wiki` to root `bin/start` and
  `bin/stop` enumeration (dependency order: after dashboard; no upstream producer
  yet). Ensure `nginx/run` regenerates a `wiki` dev location fragment from the
  manifest (it templates `__PORT__`). Confirm the dashboard inventory
  (`DASHBOARD_MANIFEST_ROOT`-derived) will pick wiki up — **no `DASHBOARD_RESOURCES`
  edit needed** (registration is now registry/manifest-derived; a dashboard
  restart suffices).
- **Inputs:** `bin/registry`, root `bin/{start,stop}`, `nginx/run`,
  `dashboard/bin/build` (the `DASHBOARD_MANIFEST_ROOT` note), repo `CLAUDE.md`
  (Registering a new MCP service).
- **Deliverables:** edited `go.work`, `bin/start`, `bin/stop`; confirmed registry
  output; dev nginx fragment generation working.
- **Acceptance:** `bin/registry list-mcp` includes `wiki`; root `bin/start` brings
  wiki up alongside the others; `cd nginx && ./run` serves `/srv/wiki/` →
  `127.0.0.1:3006`; the `/srv/wiki/mcp` 401 challenge carries `resource_metadata`
  and the PRM well-known returns 200 (dashboard sees wiki via inventory).

---

## Phase 3 — On-disk content model, schema doc, and search backend

*Goal: the wiki's data layout on disk, the agent-facing schema doc, the
immutable-`raw/` writer with provenance, and the BM25 search store — all
owner-scoped and collection-keyed. No agent/ingest wiring yet.*

### Task 3.1 — Filesystem store, data layout, provenance, schema doc, wiki.db schema
- **Brief:** Implement `internal/store` (filesystem): owner+collection → confined
  root resolution; the **immutable `raw/` writer** (stamps `sha256` + `ingested_at`
  + caller-supplied `title`/`source`/`tags` into frontmatter; never mutates an
  existing raw doc — re-ingest is safe); page read/list helpers over the page
  tree. Pin the layout:
  `<data-root>/<owner>/<collection>/{raw/, sources/, concepts/, entities/,
  events/, synthesis/, index.md, log.md, .search/index.sqlite}` with `collection`
  defaulted to `default`. Author the **schema doc** the ingest agent reads (the
  wiki's `CLAUDE.md`/`AGENTS.md`-equivalent): the default type set
  (source/concept/entity/event/synthesis), frontmatter conventions
  (`type:`, `kind:`, `source:`, `collection:`), index-first navigation, the four
  invariants (provenance, immutable raw, flag-don't-overwrite, append-don't-
  destroy). Add `002_wiki.sql` (real): ingest/provenance + job-record tables,
  carrying a `collection` column defaulted to `default`.
- **Inputs:** `GOALS.md` (Data model & taxonomy, Philosophy, Ingest properties);
  `/home/mgreenly/projects/ralph-wikis` SCHEMA.md as a lightened reference;
  `ralph/internal/sandbox/sandbox.go` + `agentkit` confinement for path safety.
- **Deliverables:** `wiki/internal/store/*.go` + tests; the schema doc (committed
  where the ingest agent will load it, e.g. `wiki/internal/store/schema/SCHEMA.md`
  or embedded); `internal/db/migrations/002_wiki.sql`.
- **Acceptance:** Unit tests: raw write is idempotent on identical bytes (same
  sha256 → no duplicate/mutation), frontmatter stamped, paths confined to the
  owner+collection root (escape attempts rejected); `go test ./wiki/...` green;
  migration applies on a fresh DB.

### Task 3.2 — BM25 search store (`internal/search`)
- **Brief:** Implement the `internal/search` interface chosen in Task 0.2 over
  `modernc.org/sqlite` FTS5: index a collection's page tree (whole markdown
  pages, not fragments), and `Search(owner, collection, query)` returning ranked
  **whole pages + the index page**, with `bm25()` scores. Keep it behind the
  interface so vector/hybrid is a later additive swap.
- **Inputs:** `wiki/notes/search-backend.md` (the confirmed approach + interface);
  qmd store schema as reference; `internal/store` from 3.1 for the page tree.
- **Deliverables:** `wiki/internal/search/*.go` + tests against a fixture tree.
- **Acceptance:** Indexing a fixture collection then searching returns the
  expected pages ranked by BM25; results are whole pages; `go test ./wiki/...`
  green under `CGO_ENABLED=0`.

---

## Phase 4 — Ingest core + `wiki_ingest_text` / `wiki_ingest_url`

*Goal: the agentic async ingest pipeline end to end on the direct-MCP path.*

### Task 4.1 — Wire agentkit, define the ingest agent, implement the async core + `wiki_ingest_text`
- **Brief:** Wire `agentkit` into `cmd/wiki/main.go`: construct the Anthropic
  client from `ANTHROPIC_API_KEY` (via SSM app-config; presence-checked at the
  composition root), the model + per-job cost ceiling from config (Decision 3),
  and an `agentkit/job` runner backed by a wiki **job store** (the run-record
  tables from 3.1). Define the **ingest agent**: a write-enabled toolset confined
  to the owner's wiki tree, system prompt = the schema doc + integration-pass
  instructions (read the raw doc → write/update the `source` page → update touched
  `concept`/`entity`/`event` pages → update `index` → append to `log`). Implement
  the ingest **core**: `trigger → persist bytes to immutable raw/ → enqueue async
  agentkit job (integration pass) → on success, re-index via internal/search`.
  Implement `wiki_ingest_text(content, title?, source?, tags?)` (stamps sha256 +
  ingested_at; returns a job id). Add the **job-status read verb** (reuse
  `agentkit/job` run/output).
- **Inputs:** `agentkit/{agent,job,provider/anthropic,tools}`; `internal/store`
  (raw writer + page tree); `internal/search` (reindex); `notify/bin/secrets` +
  `notify/.envrc` as the secret-wiring template; `GOALS.md` (Ingest core,
  Triggers, Provenance). Add `bin/secrets` for `ANTHROPIC_API_KEY` and a `.envrc`
  for local dev — **the `.envrc`'s first line must be `source_up`** (mirror
  `notify/.envrc` / `dropbox/.envrc`) so wiki inherits the root
  `GOFLAGS=-buildvcs=false`; without it, local `go build`/`go run` fails with
  exit 128. Run `direnv allow` in `wiki/` once after creating it.
- **Deliverables:** `cmd/wiki/main.go` wiring; `internal/ingest` (core +
  agent definition); MCP verbs `wiki_ingest_text` + ingest job-status read;
  `bin/secrets`; `.envrc`.
- **Acceptance:** Locally (real or stubbed provider), `wiki_ingest_text` writes
  the raw doc immutably, runs an async integration job that creates/updates pages
  + index + log, and re-indexes search; the status verb reports the run to
  terminal. With a stub provider in tests, the core path (raw → job spawn →
  reindex) is exercised without network; `go test ./wiki/...` green.
- **Notes:** Confirm model + cost ceiling here (Decision 3). Early sandbox is the
  draft Go path-confinement; OS-level confinement is Phase 7.

### Task 4.2 — `wiki_ingest_url`
- **Brief:** Implement `wiki_ingest_url(url, …)`: the **service** fetches the URL
  server-side and extracts HTML→markdown, then feeds the same ingest core (the
  fetched+extracted markdown becomes the raw bytes, with `source=url`). Use a
  pure-Go HTML→markdown path (no CGO). Naming stays delivery-shaped per GOALS.
- **Inputs:** Task 4.1 core; `GOALS.md` (Triggers table, "There is no path-based
  ingest verb").
- **Deliverables:** `wiki_ingest_url` verb + the fetch/extract helper + tests.
- **Acceptance:** Given a fixture HTML (served locally or stubbed), the verb
  fetches, extracts to markdown, writes raw with provenance, and runs the same
  async job; `go test ./wiki/...` green.

---

## Phase 5 — `wiki_search`, basic lint, end-to-end demo, deploy

*Goal: complete GOALS "Phase 1" — search verb, a basic lint pass, a working
local demo, and the service live on the box.*

### Task 5.1 — `wiki_search`
- **Brief:** Implement `wiki_search(query)` MCP verb over `internal/search`:
  synchronous, no inner agent; returns whole curated pages + the index. This is
  the "internet, but personal and pre-curated" experience.
- **Inputs:** `internal/search` (3.2); `GOALS.md` (Query).
- **Deliverables:** `wiki_search` verb + tests.
- **Acceptance:** After an ingest, `wiki_search` returns the integrated page(s);
  `tools/list` now shows `wiki_whoami, wiki_ingest_text, wiki_ingest_url,
  wiki_search` + the job-status verb; `go test ./wiki/...` green.

### Task 5.2 — Basic lint
- **Brief:** Implement a basic **lint** pass (consolidate synonymous types/pages,
  merge dupes, flag orphans / missing cross-refs) — as an agentkit job with a
  read+write toolset over the wiki tree. Trigger cadence = **manual** for now
  (an internal trigger / verb); flag cadence (manual/scheduled/post-ingest) as
  deferred per GOALS.
- **Inputs:** `agentkit/{agent,job}`; the schema doc; `GOALS.md` (Lint).
- **Deliverables:** `internal/lint` (or a lint job def) + a manual trigger; tests
  with a stub provider for the job plumbing.
- **Acceptance:** A manual lint run executes the agent job over a fixture tree to
  terminal; `go test ./wiki/...` green. Cadence left manual + documented as open.

### Task 5.3 — End-to-end local demo + verification note
- **Brief:** Bring the suite up via root `bin/start`; drive the demo over
  loopback (services trust injected identity headers, so `/mcp` can be driven
  directly): `wiki_ingest_text "record this"` → observe immutable `raw/` + the
  integrated pages + `index`/`log` updates + the search index → `wiki_search`
  finds it. Capture the steps in a short verify note.
- **Inputs:** root `bin/start`/`bin/stop`; `nginx/run`; the `verify` skill's
  approach; `GOALS.md` Phase-1 demo definition.
- **Deliverables:** `wiki/notes/verify-phase1.md` (the exact loopback/MCP calls +
  observed results).
- **Acceptance:** The note shows a real round-trip: ingest → filed → searchable,
  with the raw doc immutable and provenance stamped.

### Task 5.4 — Deploy Phase-1 wiki to the `ai` box
- **Brief:** Ship wiki to `<account>.ikigenba.com` (account `int`) in the ikigenba
  bin/* order. Human-in-the-loop: the user runs `aws sso login --profile int`
  (interactive) before any SSM/secrets step. Order: (1) `bin/secrets` to seed
  `ANTHROPIC_API_KEY` (after SSO); (2) `bin/setup` (provision user/tree/systemd/
  nginx fragment, enable-not-start); (3) `bin/deploy` (ship + start); (4) restart
  the dashboard so its manifest-derived inventory picks up wiki — **no
  `DASHBOARD_RESOURCES` edit** (registry-derived); (5) verify on the box.
- **Inputs:** repo `CLAUDE.md` (Deployments, Verify on the box, the consumer
  bring-up order); `etc/deploy.env`; `bin/{secrets,setup,deploy}`.
- **Deliverables:** wiki live on the box; a short deploy/verify log.
- **Acceptance (on box):** `systemctl is-active wiki`; `journalctl -u wiki` shows
  clean boot + migration; loopback `curl 127.0.0.1:3006/...` works; PRM
  well-known → 200; `/srv/wiki/mcp` 401 challenge carries `resource_metadata`;
  end-to-end: connector OAuth round-trip + `wiki_whoami`, then an
  ingest→search round-trip.
- **Notes:** Deploy steps that touch AWS need a live SSO session the **user**
  initiates. The dashboard restart briefly drops `/internal/authn` box-wide
  (seconds) — expected.

---

## Phase 6 — (GOALS Phase 2) Event ingest + richer query

*Goal: the autonomous front door (dropbox event → ingest) and digested,
compounding answers (`wiki_ask`).*

### Task 6.1 — Dropbox eventplane consumer on `wiki/ingest`
- **Brief:** Make wiki a **notify-shaped eventplane consumer** of dropbox's
  file-lifecycle events for a hardcoded `wiki/ingest` folder: filter the events,
  fetch the bytes (mechanism per dropbox's actual event contract — fetch-by-
  reference vs shared storage), and feed them into the **same async ingest core**
  (autonomous, no human in the loop; immutable `raw/` makes it safe). Add
  `CONSUMES=dropbox` to `etc/manifest.env`; add `003_feed_offset.sql` (byte-
  identical to `eventplane` consumer's `SchemaSQL`); wire `consumer.Run` +
  `consumer.Handler` in `main.go` (Source=`dropbox`, feed URL via
  `bin/registry feed-url dropbox`).
- **Gate / dependency:** **CLEARED** — `add_dropbox` landed on `main` and
  `add_wiki` was rebased onto it (2026-06-04), so `dropbox/` is now in this
  worktree. Before starting, read dropbox's actual event payload contract from
  its source (it pins down the fetch-by-reference vs shared-storage mechanism);
  that was the last unknown, not the branch availability.
- **Inputs:** `notify/` (the worked consumer example — `internal/push/push.go`,
  `cmd/notify/main.go` consumer wiring, `002_feed_offset.sql`, `etc/manifest.env`
  `CONSUMES=`); `eventplane/consumer` API; dropbox's event contract (TBD);
  `GOALS.md` (Triggers, Ingest autonomous-floor).
- **Deliverables:** consumer wiring + migration + manifest `CONSUMES`; deploy
  following the consumer bring-up order (producer first, then setup/deploy/verify).
- **Acceptance:** Dropping a file in `wiki/ingest` (or emitting the dropbox event)
  triggers an autonomous ingest that files the bytes; verified on the box per the
  event-plane verification (create → observe downstream effect).

### Task 6.2 — `wiki_ask` (agentic synthesis, compounding)
- **Brief:** Implement `wiki_ask(question)`: an **agentkit job with a read-only
  toolset** that does index-first navigation → reads pages → returns a
  synthesized, **cited** answer, and **files a good answer back as a `synthesis`
  page** so explorations compound. Rides the same async lifecycle as ingest.
- **Inputs:** `agentkit/{agent,job}`; `internal/store` + `internal/search`; the
  schema doc; `GOALS.md` (Query → `wiki_ask`).
- **Deliverables:** `wiki_ask` verb + the read-only synthesis agent + the
  file-back-in of synthesis pages; tests with a stub provider.
- **Acceptance:** A question returns a cited synthesis and writes a `synthesis`
  page that subsequent `wiki_search` can find; `go test ./wiki/...` green.

---

## Phase 7 — (GOALS "Later") Deferred, not yet planned in detail

Listed for the orchestrator's awareness; each becomes its own phase when pulled
forward:

- **ralph retrofit onto `agentkit/job`** — migrate ralph's bespoke `runner`/
  session lifecycle onto agentkit's generic job-runner; ralph inherits the tests.
- **OS-level sandbox confinement** (landlock/namespaces) — replaces the draft Go
  path-checks; matters most for unattended dropbox-triggered ingest.
- **Hybrid / vector search** — embeddings infra; additive swap behind the
  `internal/search` interface (this is where the qmd vector path's CGO/infra
  cost would actually be paid, deliberately, if chosen).
- **Many collections in the UX** — surface the `collection` key on the verbs (the
  model already carries it; this is additive, not a migration).
- **Interactive ingest checkpoint** on the direct-MCP path (Karpathy's mid-pass
  human checkpoint) — optional enhancement, direct path only.
- **wiki as an eventplane producer** (emit `wiki.page.*`) — reconsider if a
  downstream consumer appears.
- **ralph-wikis rigor** (atomic claims, contested ledger) — only if/when felt.

---

## Dependency order (the sequential spine)

```
0.1 ─┐
0.2 ─┴─▶ 1.1 ─▶ 1.2 ─▶ 1.3 ─▶ 2.1 ─▶ 2.2 ─▶ 3.1 ─▶ 3.2 ─▶ 4.1 ─▶ 4.2 ─▶
        └─ agentkit ──┘   └─ scaffold ─┘   └─ model+search ┘  └ ingest ┘
5.1 ─▶ 5.2 ─▶ 5.3 ─▶ 5.4        (── completes GOALS Phase 1 ──)
            then, when dropbox is available:
6.1 ─▶ 6.2                       (── GOALS Phase 2 ──)
```

Every arrow is a hard sequential gate: do not begin a task until the prior
task's *Acceptance* has been verified by the orchestrator.

## Open questions carried from GOALS (resolve at the noted task)

- agentkit job-runner store seam — settle in **0.1**, prove in **1.2**.
- modernc FTS5/`bm25()` viability — settle in **0.2**.
- ingest/ask agent **model + cost ceiling** — confirm in **4.1**.
- **lint cadence** (manual/scheduled/post-ingest) — **left manual in 5.2 (done)**:
  lint ships as an internal `lint.Linter.Lint` trigger (NOT a public MCP verb; the
  surface stays the five 5.1 verbs), wired in `cmd/wiki/main.go` and ready to
  schedule. Cadence is still **open** — revisit in **Phase 7** (the scheduler /
  post-ingest hook attaches at the `_ = linter` seam in main.go). See the package
  doc in `wiki/internal/lint/agent.go` for the rationale + GOALS basis.
- dropbox **event→bytes** mechanism (fetch-by-reference vs shared storage) — pins
  down in **6.1** from dropbox's real contract.
- wiki as eventplane **producer** — deferred to **Phase 7**.
```
