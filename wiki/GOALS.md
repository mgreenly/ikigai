# wiki — Goals & Design Intent

> Status: **brainstorm / goals**. This is the design intent agreed in the
> bring-up discussion, not an implementation spec. Architecture (`agentkit`
> extraction boundary, schema doc, migrations) and a build plan come next.

## What it is

`wiki` is a path-routed suite service (`/srv/wiki/`) that gives a user's
general-purpose agent harness — Claude Code, ChatGPT/Cowork, etc. — a **personal,
compounding knowledge base over MCP**. The user says "record this" / "ingest this
file" in their outer session and it gets filed into the wiki; the user's agent
makes search requests to the wiki while exploring — *another source to consult,
like the internet, except this one is personal and pre-curated*.

It is heavily inspired by **Andrej Karpathy's "LLM wiki"**: a persistent,
LLM-maintained collection of plain-markdown pages that sits *between* the user
and their raw sources. Knowledge is **compiled once and kept current, not
re-derived per query** (the explicit anti-RAG stance). The LLM does the
bookkeeping humans abandon; the human curates sources and asks questions.

## Prior art we're standing on (all already in hand)

- **Karpathy's LLM wiki** — the north star. Three layers (immutable raw sources →
  LLM-authored wiki → a schema doc that tells the agent the conventions);
  operations Ingest / Query / Lint; index-first navigation; explorations file
  back in so the base compounds.
- **`~/projects/ralph-wikis`** — a prior, heavier prototype of the same idea
  (categories, claims-with-provenance, contradiction surfacing, three prompt
  roles: ingest/search/lint). We take its **content model, lightened** (see
  Philosophy).
- **`qmd`** (Go) — local markdown search: SQLite FTS5 **BM25** + optional
  vector + hybrid. CLI *and* MCP. Our search backend.
- **`ralph/`** (this repo) — an agent-SDK MCP service: an in-house LLM agent loop
  with confined bash + file tools in a per-session sandbox, kicked off by an MCP
  call, owner-scoped behind nginx auth. The chassis we extract `agentkit` from.

## Suite placement

| field | value |
|---|---|
| dir / Go module | `wiki/` — `module wiki` |
| route | `/srv/wiki/` (stripped before proxy; internal routes stay `/mcp`, …) |
| loopback port | **3006** (crm 3001, ledger 3002, notify 3003, ralph 3004, dropbox 3005) |
| manifest | `etc/manifest.env`: `MOUNT=/srv/wiki/`, `PORT=3006`, `MCP=true` |
| trust | nginx introspects against dashboard `/internal/authn`; service trusts injected `X-Owner-Email` / `X-Client-Id` blindly; **owner-scoped** everywhere |
| identity proof | `ikigenba_wiki_health` MCP tool |
| registration | manifest-driven via `bin/registry` (`MCP=true` auto-surfaces); confirm dashboard `DASHBOARD_RESOURCES` wiring (now likely registry-derived) |
| also | an **eventplane consumer** (notify-shaped) — see Ingest phase 2 |

## Philosophy — closer to Karpathy than ralph-wikis

Light bodies, schema-driven, broad-not-narrow. We **drop** ralph-wikis' heavy
machinery (atomic claim-objects with minted IDs, hop-0/1/2 reconciliation rules,
the formal `contested.md` ledger) until we actually feel the need. We **keep**
the four cheap invariants that protect trust:

1. **Provenance** — every page traces back to a `source` (even when the source is
   a chat snippet).
2. **Immutable `raw/`** — originals are never mutated; re-ingest is always safe.
3. **Flag, don't overwrite** — contradictions are surfaced, not silently clobbered.
4. **Append, don't destroy** — supersede rather than delete.

## Architecture decisions (the resolved forks)

- **`agentkit` — a new foundational shared library.** The generic agent loop is
  extracted from `ralph` into a shared Go module (`replace agentkit => ../agentkit`,
  the eventplane pattern), because **multiple future services will embed agents**,
  not just wiki. It is **extracted and hardened with tests**, not consumed as
  ralph-is-today. wiki builds against it first; ralph is retrofitted onto it
  afterward and inherits the tests. ralph's current immaturity never blocks wiki.
  - **Seam (to verify against `ralph/internal/engine`):** `agentkit` owns the
    *generic* — Anthropic streaming client, the tool-use loop, base tools
    (bash/read/write/edit/grep/glob), sandbox path-confinement, the stream-json
    wire codec, the model registry, **and the async agent-job lifecycle**
    (runs / poll / single-flight / crash-recovery sweep). Each service keeps the
    *specific* — its system prompt(s), enabled toolset, MCP verb surface, content
    model, and DB.
- **In-house Go loop, full stop.** agentkit owns its own Anthropic client and Go
  tool implementations. **Shelling out to an external `pi`/`claude` binary is a
  permanently closed path** — keeps the suite convention (one self-contained Go
  binary at `/opt/<app>`, calls the API directly, secrets via SSM app-config).
- **Sandbox/OS-level confinement is a later phase.** Early stage keeps ralph's
  draft Go path-checks. (Noted: unattended ingest of dropped files will
  eventually want real OS-level confinement — landlock/namespaces — but not now.)

## Ingest

Ingest is a **pipeline with pluggable front doors**, all converging on one core:

> **trigger → persist bytes to immutable `raw/` → async `agentkit` job (the
> integration pass) → `qmd` re-index**

Properties of the core:

- **Async is mandatory** — the event trigger (below) has no synchronous caller to
  block, so agentkit's job lifecycle is required regardless. "record this" rides
  the same async core; a synchronous "wait for it" is at most a thin convenience
  over the job, never a separate path.
- **Autonomous is the floor** — an event-triggered file landing has no human in
  the loop, so ingest must run unattended. Karpathy's interactive mid-pass
  checkpoint becomes an *optional* enhancement on the direct-MCP path only.
  Immutable `raw/` is what makes unattended filing safe.
- **The integration pass** is the agentic part: read the raw doc, write/update a
  `source` page, update the touched `concept`/`entity`/`event` pages, update
  `index`, append to `log`. (Karpathy: one source can touch ~10–15 pages.)

### Triggers (front doors)

| trigger | delivery | phase |
|---|---|---|
| `wiki_ingest_text(content, title?, source?, tags?)` | inline bytes — the **lowest common denominator**; every harness (incl. cloud) can call it | 1 |
| `wiki_ingest_url(url, …)` | the **service** fetches + extracts (HTML→markdown) server-side | 1 (or 1.x) |
| eventplane consumer | **dropbox** emits a file-lifecycle event for a hardcoded `wiki/ingest` folder → wiki reacts, fetches the bytes, files them | **2** |
| binary / upload (PDFs, images) | base64 or an upload mechanism | later |

- **There is no path-based ingest verb.** The service is always remote, so a file
  only ever enters as **inlined bytes** or via the **dropbox event** path.
  "Ingest the file at /my/path" is simply not expressible over MCP — and that's
  correct, not a gap.
- **Naming is delivery-shaped** (`ingest_text` vs `ingest_url`) so the tool schema
  is unambiguous and each harness self-selects the only verb it can satisfy.
- **Provenance even for inline text:** the caller stamps `title` / `source` /
  `tags`; the service adds `sha256` + `ingested_at` and writes it all into `raw/`.

## Data model & taxonomy

- **One unified wiki** in the UX (maximises cross-linking / compounding; "record
  this" has nothing to target). A **`collection` key is carried in the model from
  day one, defaulted to `"default"`**, so splitting into many wikis later is
  *additive, not a migration*. No `collection` argument on the verbs yet.
- **Taxonomy is schema-driven; the service enforces none.** On disk it's just
  markdown + frontmatter with a `type:` field. The type set lives in the wiki's
  own **schema doc** (its `CLAUDE.md`/`AGENTS.md`-equivalent that the ingest agent
  reads), so new types can appear without a code change.
- **Broad, not narrow — breadth via metadata, not type proliferation.** A small
  set of wide types; distinctions captured in a freeform `kind:` rather than new
  silos.

  | type | what it is |
  |---|---|
  | **source** | one page per ingested raw doc — the provenance anchor |
  | **concept** | an idea / topic / method ("ideas") |
  | **entity** | a person / org / tool / product / **place** / work, via `kind:` ("identities") |
  | **event** | something that happened, dated ("events") |
  | **synthesis** | a compiled answer filed back in (the compounding artifact) |

  …plus the two navigation files: **index** (the catalog the query side reads
  first) and **log** (append-only chronological).
- **Lint** keeps the open vocabulary coherent (consolidate synonymous
  types/pages, merge dupes, flag orphans/missing cross-refs). It's the gravity
  that lets "broad" not rot into "fragmented." Trigger cadence TBD.

## Query

The asymmetry that makes this cheap: **ingest is agentic because it has to
integrate; search is cheap because the pages are already integrated.** Two verbs,
ship the first to start:

- **`wiki_search(query)`** — **primary, synchronous, no inner agent.** `qmd`
  ranks and returns *whole curated pages* (+ the index), not raw fragments. The
  outer agent reads and reasons. Cheap enough to hammer constantly while
  exploring. *This is the "internet, but personal and pre-curated" experience.*
- **`wiki_ask(question)`** — *later / optional*, **agentic**: an inner agent does
  index-first navigation → reads pages → returns a synthesized, **cited** answer,
  and **files a good answer back as a `synthesis` page** so explorations
  compound. It's an agent job → rides the same agentkit async lifecycle as
  ingest, with a read-only toolset.

Start `wiki_search`-only; add `wiki_ask` when we want digested answers +
compounding (layers on with zero rework).

## MCP tool surface (sketch)

Phase 1: `ikigenba_wiki_health`, `ikigenba_wiki_ingest_text`,
`ikigenba_wiki_ingest_url`, `ikigenba_wiki_search`, plus an ingest job-status read
(reuse agentkit's run/output verbs).
Later: `ikigenba_wiki_ask`, and whatever the dropbox consumer needs internally.

## Phasing

- **Phase 1 — direct MCP, single wiki, BM25.** Extract+harden `agentkit`; wiki
  service scaffold (clone the ledger chassis); `wiki_ingest_text` / `_url` →
  raw → async integration job → `qmd` (BM25) index; `wiki_search` over whole
  pages; the schema doc + default type set; basic lint. One end-to-end demo:
  *"record this" in a session → it's filed → search finds it.*
- **Phase 2 — event ingest + richer query.** Dropbox eventplane consumer
  (notify-shaped) on the `wiki/ingest` folder; `wiki_ask` (agentic synthesis +
  file-back-in).
- **Later.** OS-level sandbox confinement; hybrid/vector search (embeddings
  infra); many collections in the UX; interactive ingest checkpoint on the
  direct path; ralph retrofit onto `agentkit`; reconsider any ralph-wikis rigor
  (claims, contested ledger) if/when felt.

## Open / deferred questions

- `agentkit` extraction boundary — **verify against `ralph/internal/engine`**
  how cleanly the loop separates from ralph's session/run chassis (decides how
  much lifts cleanly vs. needs reshaping). Library name (`agentkit`) is a
  placeholder.
- `qmd`: which fork (`tobi/qmd` per Karpathy vs `akhenakh/qmd` per ralph-wikis);
  vendor the `store` package in-process vs shell out to the binary; BM25-only
  (zero infra) vs hybrid/vector (needs embeddings).
- Lint trigger cadence (manual / scheduled / post-ingest).
- Agent model + cost ceiling for ingest/ask jobs.
- Whether wiki is *also* an eventplane **producer** (emit `wiki.page.*` events).
- How the dropbox event delivers bytes to wiki (fetch-by-reference vs shared
  storage) — pins down at phase 2 from dropbox's actual event contract.
