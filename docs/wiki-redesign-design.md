# Design — wiki redesign: a prose-pages knowledge service

> Status: **current design** (2026-06-13). This is the build-ready design for
> the `wiki` service rewrite, distilled from the authoritative decision log
> `wiki-redesign-decisions.md` (the running walk-through; consult it for the
> full rationale behind any decision here). It **replaces an earlier retired
> draft** whose graph-cascade / episode-log approach is no longer pursued. The
> paired execution plan is `wiki-redesign-plan.md` (per
> `docs/README.md`). The existing `wiki/` code is superseded — this is a clean
> rewrite, nothing is preserved.
>
> Prior-art context for the read side lives in
> `docs/wiki-prior-art-research.md`. The out-of-band evaluation engine
> (`docs/wiki-evaluation-research.md`) is **not** part of this design and is
> **not** a CI/CD precondition; its only footprint here is the
> config-injection constraint stated in "LLM call-site discipline."

This document fixes the *how* and the decisions; it is not a plan and carries
no steps. Open questions that are genuinely undecided are gathered in
"Open items" at the end — do not treat anything there as resolved.

## 1. Overview and constraints

The wiki is a **prose-pages knowledge service**: one curated, human-readable
page per subject, built and maintained by LLM work done at *write* time so the
reader pays as little as possible. It is one of the suite's deployable
services, built on the shared **appkit** chassis (config-from-env, migration
runner, loopback server, `/feed`, manifest, verb dispatcher), **eventplane**
(producer + consumer plumbing), and **agentkit** (the agent-backed call
machinery). It runs no UI and no token logic — nginx is the trust boundary and
hands it `X-Owner-Email` / `X-Client-Id`. The product surface is **MCP**.

Suite constraints that bind every decision below:

- **Pure Go, `CGO_ENABLED=0`, one static `linux/amd64` binary.** This rules
  out C extensions — notably sqlite-vec (see the vector lane).
- **SQLite is the only datastore** (`modernc.org/sqlite`, CGo-free), plus a
  content-addressed `blobs/` directory for large payloads.
- **One box, one account per box, single SQLite writer.** Scheduled short
  downtime is acceptable; no cluster, no broker, no zero-downtime machinery.
- **Fixed verb set:** `serve` / `version` / `manifest` / `migrate` / `backup`
  / `restore`, via the appkit dispatcher.

The central architectural bet — inherited and sharpened from the decision log —
is **acceptance is decoupled from integration**. Receiving input is cheap,
deterministic, and never touches an LLM; turning input into knowledge is a
downstream consumer running on its own policy. This is the one shape that
serves every front door (pasted note, dropbox burst, suite event stream,
transcript) under one contract and lets every later question (what to derive,
when, with which model) change without touching the write path.

```
front doors ─→ Accept() ─→ inbox ─→ self-selecting workers ─→ integrators ─→ pages
(MCP verbs,                (SQLite   (N identical              (document pass,   (prose,
 eventplane                 table +   goroutines,               digest pass,      SQLite
 consumers)                 blobs/)   no dispatcher)            lint jobs)        + FTS5)
```

Vocabulary discipline (locked): **ingest = into the inbox; integrate = out of
the inbox.** "Episode," "idle-time/sleep-time" are rejected; we say inbox rows
/ arrivals.

## 2. The ingest side

### 2.1 Front doors and `Accept`

A **front door** is any code path that receives input and calls the single
write function:

```go
Accept(owner, kind, source, mime, title, tags string, bytes []byte)
    (id, sha256 string, dup bool, err error)
```

`Accept` is the *only* code that writes the inbox: hash, size, decide
inline-vs-spill, insert the row, nudge the workers. It is synchronous,
transactional, and never calls an LLM. The doors:

- **`ingest_text`** (MCP) → `Accept` directly with the bytes.
- **`ingest_url`** (MCP) → fetch + extract server-side, then `Accept`.
- **eventplane consumers** — one goroutine per subscribed feed (dropbox, crm,
  ledger, …, cron), each with an SSE connection; the cursor commits **only
  after** `Accept` returns (at-least-once; dedup via hash). Per handler:
  - suite **domain event** → `Accept(kind=event, …)`, envelope JSON verbatim;
  - **dropbox file event** → fetch `content_url`, then `Accept(kind=document)`
    (the event is the delivery mechanism, the file is the knowledge — the
    envelope is used and discarded);
  - **`cron.<name>`** → `Accept(kind=event, source="cron:<name>")`.

There is **no synthetic event envelope** wrapping MCP content. The inbox row
*is* the common shape; each door fills the columns from what it natively has.
Eventplane consumer doors stamp a **system identity** in `owner` (no human
acted; exact literal → open items).

**MCP return contract — a receipt, not a job.** `ingest_text`/`ingest_url`
return **inbox id + sha256 + duplicate flag**, meaning "recorded, will be
integrated." No job id exists yet (integration may run seconds or hours
later); a caller that cares polls a status verb against the inbox id.

### 2.2 The inbox

The inbox is the queue of accepted-but-not-yet-integrated arrivals: one SQLite
table plus a blob directory.

**Storage rule — small payloads in the row, large to disk.**
`len(bytes) <= WIKI_INBOX_INLINE_MAX` (default **4096**) → bytes in the row's
`content` column; larger → a content-addressed blob at `blobs/<aa>/<sha256>`
(2-hex fan-out, `WIKI_BLOB_FANOUT = 2`) with the row holding the reference.
Invariants that keep the threshold tunable with no migration: `sha256` is
computed and stored **always**, both paths; the row records which path was
taken and readers dispatch on the row, never the current threshold; one
accessor (`ReadPayload(row)`) is the only code that knows two paths exist;
blob write order is write → fsync → insert row (orphan blobs are legal and
sweepable, rows pointing at missing blobs are a bug). Size alone decides — no
mime/binary rule.

**Size cap — refuse oversized input at the door.** `Accept` refuses input over
`WIKI_INGEST_MAX_BYTES` (default **131072**, 128 KiB ≈ ~33k tokens) loudly at
the front door; **no ingest-side chunking is built.** Refusal at the door never
makes the ingest promise the pipeline provably can't keep (an oversized
document fails every extract attempt identically and would dead-letter after
five). Non-interactive doors (an eventplane consumer with nobody watching)
**notify** on refusal — via the eventplane producer (section 8). If
oversized-but-valuable documents ever appear, the contained fix is a fan-out
extract behind extract's data contract; until then the human remedy is to split
and re-ingest.

**Inbox schema (locked except the failure-policy riders, which are listed in
Open items as schema finals):**

```sql
inbox(
  id            TEXT PRIMARY KEY,            -- ULID, identifies the ARRIVAL
  owner         TEXT NOT NULL,               -- X-Owner-Email; attribution, never isolation
  kind          TEXT NOT NULL,               -- document | event
  source        TEXT NOT NULL,               -- one prefixed string (url:… dropbox:… mcp:… crm:… cron:…)
  sha256        TEXT NOT NULL,               -- content identity, always computed
  size          INTEGER NOT NULL,
  mime          TEXT NOT NULL DEFAULT '',
  content       BLOB,                        -- inline payload (≤ inline-max), NULL if spilled
  blob          INTEGER NOT NULL DEFAULT 0,  -- 1 = bytes at blobs/<aa>/<sha256>
  title         TEXT NOT NULL DEFAULT '',
  tags          TEXT NOT NULL DEFAULT '[]',  -- JSON array, caller-supplied
  received_at   INTEGER NOT NULL,            -- epoch ms; when the inbox accepted it
  integrated_by TEXT NOT NULL DEFAULT ''     -- run id; '' = pending
  -- failure-policy riders (schema finals): ineligible_until, dead_at, requeued_at (nullable epoch-ms)
)
-- indexes: (integrated_by), (sha256)
```

Decisions baked in:

- **`kind` is binary `document | event`** — it classifies what the content
  *is* (routing), not how it arrived. text/url/file collapse to `document`;
  domain events are `event`. **Transcripts are documents** (one coherent text
  = the document pass's exact shape); their only special concern is extraction
  *quality*, handled by a dialog-aware clause in the extract prompt (rider).
- **`source` is one prefixed string**, not split into columns, for humans and
  integrator prompts.
- **Timestamps are INTEGER epoch ms**, not RFC3339 TEXT (one canonical
  representation; native comparisons).
- **`received_at` is the ONLY inbox timestamp.** `occurred_at` (world time) was
  deliberately removed from the inbox — it is interpretation, not arrival fact.
  Integrators resolve world time downstream and record it on the
  facts/pages they produce. **Integration logic must never order knowledge by
  `received_at`.**
- **No UNIQUE on `sha256`** — two arrivals of identical content are two rows;
  bytes dedup via the hash, arrivals don't. Feed re-delivery dedup is the
  consumer cursor's job.
- **`integrated_by`**: `''` = pending; otherwise the ULID of the run that
  consumed the row. The stamp is **permanent** — never cleared (see
  Re-integration, section 4.4). Re-queue of a dead row clears `dead_at`, not
  this column.
- **`owner` is attribution, never isolation (locked).** Knowledge is **shared
  per box**: pages and subjects carry no owner, everyone's documents merge into
  the one wiki, ask reads everything. `owner` answers "who ingested / who
  asked" (audit, provenance, golden candidates). The pending index is
  `(integrated_by)` — there are no owner-scoped pending queries.

**Provenance chain.** The inbox id threads the whole system: answer → page
(cites inbox ids) → inbox row (source, hash, bytes) → original input; in
reverse, `integrated_by` → run → outputs. Provenance is a mechanical key, not
a convention.

## 3. Concurrency — the dispatcher-free worker pool

Integration runs on a pool of `WIKI_INTEGRATION_WORKERS` **identical worker
goroutines** (config, default **4**). There is **no central dispatcher**: each
worker self-selects its next unit of work in the **selection** critical section
under one in-flight-set mutex, runs the matching integrator, commits, drops the
claim, loops. The mutex-guarded selection *is* the lone dispatcher of the
earlier design re-expressed as a lock — correctness-, crash-, and
contention-equivalent — chosen on **legibility** (explicit over implicit,
mechanically verifiable). This replaces the never-agreed single-flight
proposal.

```
N consumer goroutines (one SSE connection per feed)
        │  every handler ends at Accept(); cursor commits after Accept returns
        ▼  (contentless wake nudge)
N identical worker goroutines, each looping:
   lock(inflight):                                    -- the selection critical section
     1. a pending cron row whose bound job name(s) aren't in flight → claim (TryLock job name)
     2. else the oldest pending document row not in flight → claim (TryLock row id)
     3. else nothing → block on the wake Cond
   unlock(inflight)
   run the claimed work (no lock held) → commit → drop the claim → loop
```

- **Selection priority: cron before documents** (cron is already-late
  scheduled work; documents are queue work with no deadline; rare cron rows
  consumed one cycle each cannot starve documents).
- **The nudge is an optimization, not the truth.** Signals carry no data and
  may be lost or duplicated; on any wake (and at boot) a worker re-runs
  selection against the inbox. Crash between acceptance and integration loses
  nothing. Wake sources: arrival nudge, run completion, the
  `ineligible_until` timer, shutdown.
- **Cron events go through `Accept` like any other event.** A pending
  `cron:<name>` row is **durable batch authorization**: if every worker is
  busy it waits; if the process crashes (including scheduled-downtime deploys
  near the nightly tick) the boot scan fires the digests late instead of
  skipping a cycle. No in-memory owed-work state. A cron row no job binds is
  stamped immediately as a no-op.
- **Three row roles:** **document** causes work immediately; **cron** causes
  work on schedule; **event** never causes work — it waits to be swept by a
  digest's selector. Event rows are invisible to selection.

**At-most-one-in-flight is a `TryLock` per work key** (Item 1): key = **job
name** for digests/lint (their work is chosen by a shared selector, so two
concurrent runs of one entry would read the same pending rows), key = **inbox
row id** for the document pass (so the pool isn't serialized — many document
passes run at once; the guard only stops two workers grabbing the *same* row).
The in-flight set is **in-memory only** (RAM).

**Crash-resume rides one rule: stamp only at commit, never at claim** (Item 2).
Two states are durable — pending (`integrated_by=''`) and done
(`integrated_by=<run id>`, written only inside the end-of-run commit);
"in progress" is RAM membership, wiped on crash. A crash can only ever leave a
row **pending**, so restart re-selects it; no cleanup. The durable `running`
row in `runs` is kept for **accounting, not resume**: the boot sweep flips
orphaned `running` → `crashed`, counting one attempt; it does not gate
re-selection.

**SQLite contention is identical to the lone-dispatcher model** (Item 3): same
per-run write footprint (one `running` insert, one end-of-run commit, no DB
write for the in-memory claim). Selection is serialized; WAL +
`busy_timeout` cover the occasional commit queue. The DB is touched only by the
milliseconds-long end-of-run commit, never while an LLM thinks.

**Optimistic commit guards lost page updates.** Pages carry a `version`
INTEGER bumped on every write; the manifest records the version merge read; the
commit writes
`UPDATE pages SET body=?, version=version+1 WHERE subject=? AND version=?` and
treats zero rows affected as a conflict → roll back the whole transaction,
re-read the changed page, **re-run merge only** for that page (extract and
resolve outputs cannot go stale — claims and identities don't, prose does),
recommit. Pessimistic page locks (held over LLM-minutes) and admission control
(the write set is unknown until extract+resolve finish) are rejected.

**Duplicate subject minting is caught by the alias UNIQUE.** Two runs both
minting a not-yet-registered "Acme Corp" both queue alias rows; SQLite's single
writer serializes the commits; the loser's alias insert hits `UNIQUE(type,
norm)` → roll back → **restart at resolve** for the colliding subject only (the
lookup now hits the winner's freshly-committed aliases). **Extract is never
re-entered for any conflict** — nothing another run does invalidates what
*this* document said. A *found-it* alias attachment hitting UNIQUE on the same
subject_id is a harmless `ON CONFLICT DO NOTHING`; a different subject_id is
bridging evidence routed through the same conflict arm.

**The conflict loop** (shared by both conflict types): the failing statement
identifies the colliding page/subject (no diagnosis step); the retry re-enters
the *existing* stage functions with the existing extract output. Bound: **cap
3 commit attempts, then fail cleanly** with the error naming **conflict-retry
exhaustion** (deliberately distinct from the run-level retry budget — two
budgets: 3 commit attempts inside one run vs. N runs per causing row). In-run
retries are **unspaced** (each retry is a minutes-long merge call, already
jittered by natural latency); post-exhaustion re-selection **is** delayed via
`ineligible_until` (section 7). `conflicts INTEGER DEFAULT 0` on `runs` records
collisions per run, so "is the pool creating problems" is a query, not
archaeology.

**Ordering is forfeited explicitly.** The pool commits out of arrival order;
this was already worthless because integration must never order by
`received_at`, merge is already required to be order-tolerant (claims carry
their own resolved dates, corroboration adds citations, contradictions keep
both sides). Selection stays oldest-first, so arrival order remains the
fairness order for who gets a worker — no starvation.

## 4. Integration — the document pass

The document pass is a fixed per-row pipeline:

```
inbox row → extract → resolve → merge → commit
            (LLM)    (mostly    (LLM    (one SQLite
                      mechanical) agent)  transaction)
```

**Eager rule: one document per run** — no debounce, no combined passes;
quality stays per-document. 1:1:1 — one inbox row, one manifest, one merge run.

### 4.1 The knowledge layer — prose pages over a registry

- The knowledge layer is **prose subject pages**: one page per subject,
  agent-curated, readable, compressed. Structured alternatives (facts tables,
  graphs) re-import the ontology complexity rejected at ingest; prose is what an
  LLM reads/writes natively.
- **Pages live in a `pages` table** (SQLite), with FTS5 over it — not loose
  files. Decisive: the end-of-run transaction (pages + registry + run row +
  `integrated_by`) is only airtight if pages are rows (files can't join a
  transaction). The merge agent's read/write tools serve pages by path;
  storage is invisible to it.
- **The registry is `subjects` + `aliases`** — every subject ever minted plus
  every name it has been known by.

```sql
subjects(id ULID, type, canonical_name, page, created_by_run)
  -- + kind, + occurred_at (nullable TEXT ISO-8601 prefix, events only) — schema finals
aliases(norm TEXT, subject_id, UNIQUE(type, norm))
```

- **Subject taxonomy — closed set of three types**, each with a decision test:
  `entity` (has identity — person, org, product, place; `kind` is freeform
  subtype), `event` (happened at a time), `concept` (idea / topic / method).
- **`subjects.occurred_at`** — nullable TEXT, ISO-8601 **prefix** (`"2019"`,
  `"2019-06"`, `"2019-06-11T14:00"`; sorts/ranges lexicographically, encodes
  honest variable precision), populated only for `type=event`, written in the
  commit from the manifest, **first-writer-wins** (a later different value is a
  page-level contradiction, not an overwrite). This is where the world-time
  value extract/compile emit finally lands.
- **`raw/` dies** — the inbox (rows + `blobs/`) is the durable original-bytes
  store; pages cite inbox ids.
- **No source page** — a document's knowledge is dismembered into subject
  pages; the document is represented only by its inbox row + blob, reached via
  citations + `read_source`. A document that genuinely *is* a subject (a named
  master contract) gets a page the ordinary way, as an extracted `entity`.
- **No `index.md`** — the registry *is* the index; search and ask's lookup are
  discovery. An index page would be a global write hotspot under optimistic
  commit. Browsing, if ever wanted, is an on-demand view over `subjects`.

### 4.2 Extract — one full-context structured call

Input: the whole document + a **context header** (a few lines built
mechanically from the inbox row — `source`, `title`, `tags`, `received_at`
rendered as a date, framed **"received on", never "today is"**) + the subject
schema. Output: JSON — subjects, each
`{type, kind, name, aliases, claims[], occurred_at (events)}`. **No tools, no
wiki access, no loop** — single-pass extraction is the model's native
competence and is golden-testable. Output budget is **not** capped small.

Key extract decisions: `kind` is **freeform, prompt-anchored** (the prompt
names exemplar kinds; the schema enforces nothing). Extract resolves
**within-document co-reference** only (cross-document identity is the
registry's job). Names emitted **as the document states them** — never
pre-canonicalized (normalization is the registry's job; model cleanup destroys
the evidence lookup needs). A claim is a short, **self-contained** prose
statement (no pronouns, no document-relative references), **asserted by the
document, not inferred** (synthesis is merge's job). Relative time is resolved
inside claims using the header; if it can't be resolved confidently, **keep it
relative** rather than guess (the cheap visible failure over the confident
wrong date). The salience gate: identifiable (Wikipedia-article test + a
concrete never-list) **and** claim-bearing, with the polarity **when in doubt,
do not extract** (sub-salient info becomes a claim on the subject it's really
about). The extract prompt has six sections (task framing, subject schema,
salience, identity discipline, claims discipline, output recap + one worked
example); the dialog-awareness clause for transcripts rides here.

### 4.3 Resolution — mechanical first, LLM second

The extracted name is a lookup key, never an address; the address is a registry
`subject_id`. Per subject, build the key set
`normalize(name) ∪ normalize(aliases)` (NFKC, casefold, trim, collapse
whitespace, strip diacritics — pure, versioned, rebuildable), then one query
`SELECT DISTINCT subject_id FROM aliases WHERE type=? AND norm IN (keys)`:

- **One id** → resolved, no LLM (companion surface forms added as alias rows).
- **Many ids** → the document's aliases bridge two registry subjects; those
  subjects *are* the candidate set → straight to **match**, no search; the pair
  is also dup-flagged. A create-it answer is still legal.
- **Zero ids** → **candidates**: two FTS queries (same type, top ~5,
  deterministic, no threshold initially) — name/alias tokens vs. registry
  names (lexical), and claim text vs. page bodies (catches zero-token-overlap
  synonyms like AWS / "Amazon Cloud"). **Zero candidates → create it, no LLM.**

**Match** — the one LLM call in resolution: a structured, tool-less call judging
the whole shortlist at once, **binary out**: `same(id)` or `no_match`. Prompted
**identity, not similarity**; **doubt is no_match** (false-split is cheap and
lint-repairable, false-merge poisons pages). "Uncertain" was removed from the
contract. A side channel reports candidate-pairs that look like each other
(feeds `dup_flags`). The candidate excerpt = canonical name + full alias list +
the first `WIKI_MATCH_EXCERPT_CHARS` (default **600**) of the page body. This
creates a **cross-prompt invariant the merge prompt owes match**: every page's
lead must stay identity-establishing. Actions (queued for the commit): *found
it* → alias rows for all incoming forms → the found id; *create it* → new
subject row, all forms become first alias rows, fresh page planned. Every
judgment is written back as alias rows — **judgment converts to mechanism**:
each name is LLM-decided at most once, then string-lookup forever.

The match prompt has five sections (task framing — identity not similarity; the
evidence; binary decision rule + doubt-is-no_match polarity; the candidate-pair
side channel; output schema + worked example).

### 4.4 Merge — one agent run per document, prose in

Input: the manifest (the in-memory work order — every extracted subject
annotated with resolved subject_id + target page + claims; never persisted, the
run id is its durable identity). **One agent run per document** (not per page
— for cross-page coherence and exactly one run row per integration). **Write
set = the manifest's pages, exactly** (new evidence is the only license to
write; no link-following expansion). Read set is looser (neighbors for
context). Per page, fold *that subject's claims only* into the body as **prose,
not a ledger**: new info woven in; already-known gets the new citation
(corroboration); contradictions flagged with both statements and citations
kept, corralled in a marked section. **Merge sees the manifest only, never the
original document** (the raw document invites re-extraction past the salience
gate). Tools: read + write pages, nothing else.

Page anatomy: thin frontmatter (`subject` registry id, `type`, `kind`,
`title` — nothing more; aliases live in the registry, provenance in citations,
history in `runs`), then prose body where **every statement carries inline
inbox-id citations** `[01HX4…]`. **Lead discipline** (the match obligation):
the first paragraph states identity. The merge prompt has six sections, and
inherits the **citation-preservation** obligation (section 6.1). New pages get
frontmatter from the registry row plus a lead built from claims.

### 4.5 Runs, the commit, and re-integration

A **run** is one execution of one job (document pass, a digest, or a lint job)
— a row in `runs`, the provenance key:

```sql
runs(
  id          TEXT PRIMARY KEY,  -- ULID
  job         TEXT NOT NULL,     -- 'document-pass' | 'crm-digest' | 'lint-dups' | …
                                 -- (renamed from 'integrator' — schema finals)
  caused_by   TEXT NOT NULL,     -- inbox id of the causing row
  status      TEXT NOT NULL,     -- running | succeeded | failed | crashed
  started_at  INTEGER NOT NULL,  -- epoch ms
  finished_at INTEGER,
  usage       TEXT,              -- token/cost accounting
  conflicts   INTEGER NOT NULL DEFAULT 0,  -- optimistic-commit retries
  error       TEXT NOT NULL DEFAULT ''
)
```

`inbox.integrated_by → runs.id` and `runs.caused_by → inbox.id` cross-reference
one column each way, making provenance, the status poll, and retry counting
single queries. **One run row per attempt** (a document failing twice then
succeeding has three rows; the inbox stamp points at the one that succeeded).
The `running` row is inserted before the run executes (the one write outside
the commit). Exactly one terminal state follows: **succeeded** (set inside the
commit, atomic with pages/registry/stamps), **failed** (clean, transaction
never committed, row stays pending), or **crashed** (process died; the boot
sweep marks orphaned `running` rows `crashed`; thereafter indistinguishable
from failed to retry logic).

**One SQLite transaction at end of run** writes everything: updated/created
pages, registry inserts, dup flags, the run row, `integrated_by`. Mid-run there
are zero partial writes.

**Stamping principle:** a row is stamped by whatever fulfills its promise, at
the moment the promise becomes true, never earlier. Document/event rows are
stamped inside the run's commit (atomic with the writes). Cron rows are stamped
by a **worker-local completion-time join** — the worker finishing a bound digest
queries "do all bound entries for this `caused_by` now have a *succeeded* run?";
if yes, `UPDATE inbox SET integrated_by=… WHERE id=? AND integrated_by=''` (the
WHERE makes a double-stamp race a harmless no-op). The stamp fires only once
**all** bound runs **succeeded** — a failed run leaves the cron row pending,
which is the retry authorization.

**Re-integration is rejected; stamps are permanent.** `integrated_by` is never
cleared; there is no replay of successful runs. Corrections enter through the
front door as new documents (merge is already order-tolerant; a correction is
new content with a new sha256, so dedup doesn't block it). Retry never needed
re-integration (a failed run never writes the stamp). Pipeline upgrades don't
replay either — existing pages are repaired at the page layer by lint.
Determinism ledger: accept/normalize/lookup/candidates/registry-writes/commit
are deterministic; extract/match are structured LLM calls; merge is an agent.
Each LLM stage hides behind a data contract (subjects-out, found-or-create,
pages-in-pages-out) so any stage can be re-implemented without touching
neighbors.

## 5. Integration — the digest pass

Events are **accepted into the inbox only** — the wiki keeps consuming raw
feeds (no producer-side digests). On a **cron-fired schedule** (default daily;
cadence is config; **no volume trigger**), a batch integrator compiles
everything pending into a **digest** and gives it one integration pass —
aggregation is itself knowledge creation ("the deal closed after three weeks"
is wiki-worthy; the 14 underlying events are not).

The digest **reuses the document pass from resolve onward**; its only new
machinery is **compile**, which plays extract's role for event piles:

```
document pass:  bytes      → extract ─┐
digest:         event rows → compile ─┴─→ resolve → manifest → merge → commit
```

- **Compile targets extract's output schema directly** (subjects with
  typed/kinded names, aliases, claims, `occurred_at`) and enters the shared
  pipeline at resolve. **No prose-digest artifact exists** (it would be a lossy
  re-extraction round-trip). Compile is the same call shape as extract: one
  structured, tool-less, golden-testable call.
- **`occurred_at` is resolved in compile** from event payloads (envelope time,
  per-known-event-type extractors).
- **Per-claim citations.** Events are presented to compile with their inbox ids
  visible; each claim carries `cites: [inbox ids]` naming the specific events
  it rests on. Risk accepted: the model can mis-attribute an id (golden-testable).
- **Citation semantics stay uniform** — every page citation points at an
  outside-world arrival (a document claim cites its document row; a digest
  claim cites raw event rows). The compiled output is **never** re-`Accept`ed
  as a synthetic row.
- **The manifest generalizes** so merge can't tell which integrator ran: every
  claim is `{text, cites[]}` (the document pass fills `cites` with its one row
  id; compile fills it per-claim).
- **No journal page** — a digest leaves no day-shaped residue; the time axis is
  served by `type=event` subjects + `occurred_at` + the inbox.

The compile prompt is **extract's six-section skeleton with four deltas**: task
framing swaps "the document is a carrier" for aggregation ("narrate outcomes,
not deltas"); salience presumes per-event micro-facts are noise; claims
discipline gains the `cites` and `occurred_at` obligations; the worked example
is event-shaped. Compile **concludes across events** but never infers beyond
what the cited events jointly assert.

**Digest concurrency rules** (in addition to optimistic commit on pages):
**at most one in-flight run per batch-table entry name** (the job-name `TryLock`
forces same-entry runs into the serial order where re-fire idempotency works);
**stamp by id list, never by selector** (re-evaluating the selector at commit
would stamp mid-run arrivals never compiled — silently lost knowledge, the
worst failure class). Selectors must **partition** event rows: overlapping
selectors → refuse to boot; consumed sources matched by no selector → surface
it. A cron row is a tiny fan-out: the worker looks up the bound entries for the
trigger and runs each bound digest as its own `runs` row with
`caused_by = cron-row-id`.

## 6. Lint — a family of named jobs on the same spine

Lint is **a family of named jobs, not one monolithic sweep** — the chores
differ wildly in shape and cost. Lint jobs run on the existing spine
(self-selected by workers, one `runs` row per attempt, failure policy applied
to the causing row verbatim). Triggers are **rows from any door**: the cron
consumer produces a trigger row on schedule, and a manual MCP verb (e.g.
`lint_run(job)`) is just another front door that `Accept`s a trigger row — so
lint is cron-triggered *and* on-demand. Three named jobs:

**`lint-dups` — dup-queue consumption.** A `dup_flags` row is *evidence, never
a verdict*; the job's first step is a dedicated LLM identity judgment on the
pair (better-informed than any flagger: both full pages + complete alias
lists). Three outcomes: **merge** (perform the merge, row → `merged`),
**dismiss** (definitely different, row → `dismissed`, permanent — blocks
re-flagging), **can't-tell-yet** (status stays `open`; the only write stamps
the page versions examined into `judged_version_a/_b`, and the work-list query
skips open rows where neither page's `version` advanced past the stamp — new
evidence is the only license to re-judge). Subject-merge mechanics: the
**older ULID wins** mechanically (the judge picks only the canonical *name*,
never the surviving id); the loser is hard-deleted in one transaction (repoint
aliases → winner; rewrite open `dup_flags`; fold loser's prose into winner's
page; delete loser's page + subjects row; set winner's `canonical_name`, mark
the dup row `merged`). No `merged_into` tombstone — `dup_flags` is the audit
record; a stale loser id reachable anywhere is a bug to crash on. Run shape:
one run per trigger sweeping eligible pairs serially, **one transaction per
pair** (per-pair recovery via the queue itself). **Judge and fold are two
separate tool-less calls** (judge match-shaped and cheap; fold runs only on a
merge verdict, both bodies in → one body out, inheriting the merge craft
obligations).

**`lint-sweep` — semantic duplicate sweep.** Its own named job, **flag-only**
(inserts `dup_flags` via `FlagDup`, never judges or merges). It is the
proactive walker that finds duplicates built from disjoint streams (Bob from
emails, Robert from CRM) that integration-time writers never co-examine. It is
**fully mechanical — zero LLM calls**: for each subject, run the same two
candidate FTS queries; any pair above a flag threshold → `FlagDup`. The pair
UNIQUE makes it idempotent and polite (settled pairs bounce off). Wide scan,
rare cadence (weekly/monthly cron, or manual). The flag threshold is an
eval-harness tuning knob.

**`lint-stale` — staleness repair.** Backed by a `stale_notes` side channel
(flag-only writers during other work — the merge agent or fold, noticing a
read-only neighbor page that contradicts what it just wrote, appends a note in
its existing commit; the `cites` column carries the new evidence that makes the
repair *legal*). The fixing job's work list = open notes; the repair call is
**tool-less, one call per subject** batching all that subject's open notes
(page body + notes + cited payloads in; rewritten page + per-note disposition
out, one transaction), inheriting the craft obligations.

**`dup_flags` storage:** every pair in canonical order (`subject_a < subject_b`,
the smaller ULID), enforced by three pieces — one helper `FlagDup(x,y)` (sorts,
`INSERT … ON CONFLICT DO NOTHING`), `UNIQUE(subject_a, subject_b)`, and
`CHECK(subject_a < subject_b)`. (`status: open|merged|dismissed`, plus the
`judged_version_a/_b` columns.)

Two chores were **removed from lint**: **citation preservation** is a
commit-time gate (section 6.1), and **unresolved relative time** is struck
entirely (lint can't improve on extract's keep-it-relative choice — following
the citation reaches the same inbox row with no new evidence; the genuine fix
is a later document that supersedes via normal merge). **Page growth: no split,
ever** — one subject = one page is an invariant (load-bearing for addressing);
growth is managed by merge's compression mandate; a condense job is a contained
later shape if measured.

**One `jobs` config** unifies digests and lint (config, not a table): `name`,
`trigger`, and `select` (legal only for digest entries; lint entries bind name
→ trigger with no selector). Selection treats both kinds identically.
Vocabulary: a **job** is anything a worker can select and start; **integrators**
(document pass, digests) consume inbox rows; **lint jobs** maintain existing
content; a **run** is one execution of one job.

```yaml
jobs:
  - name: crm-digest        # integrator (digest)
    trigger: cron.daily
    select: source LIKE 'crm:%'
  - name: lint-dups         # lint job — no selector
    trigger: cron.weekly
  - name: lint-sweep
    trigger: cron.monthly
  - name: lint-stale
    trigger: cron.weekly
```

### 6.1 Citation preservation — a commit-time gate

Every LLM call that rewrites a page (document-pass merge, the fold, the stale
repair) also outputs a `superseded` list — the citation ids it deliberately
dropped, one reason line each. At commit, pure set arithmetic:
`old citations − new citations` must exactly equal the declared list. Undeclared
loss = the model paraphrased away evidence = a **failed call**, retried in-run,
never committed. This is a gate, not a lint job, because the invariant has a
judgment clause ("survives *or was deliberately superseded*") knowable only at
write time; it is nearly free (two regex scans + a set difference), and the
`superseded` declarations are the audit trail for vanished citations.

## 7. Failure policy — bounded retries + dead-letter

The frame is **bounded retries + dead-letter** (the only shape where transient
failures self-heal, persistent failures stop spending money, and the row stays
durable and visible for a human). Three nullable epoch-ms columns on `inbox`,
each meaning one independent fact; the pending predicate is their conjunction:

```
pending = integrated_by = '' AND dead_at IS NULL
          AND (ineligible_until IS NULL OR ineligible_until <= now)
```

- **`ineligible_until` — backoff, set on every failed run** (one delay
  mechanism, no failure-type dispatch — code can't reliably classify
  transient vs. persistent). The workers gain a fourth wake source: a timer
  armed to the earliest future `ineligible_until`. Formula:
  `now + random(2–4) × avg_run_duration × 2^(failures−1)`, where
  `avg_run_duration` is the recent average from `runs` floored at **60s**,
  `failures` is the since-re-queue attempt count, and the jitter desynchronizes
  failed rows. It grows with attempt count so a one-hour API outage doesn't
  burn the whole budget (≈5m, 10m, 20m, 40m, 80m); the threshold bounds the
  exponent, so no cap is needed.
- **`dead_at` — the dead mark.** No separate table, the row never moves;
  re-queue = set `dead_at` back to NULL. Dead-lettering clears
  `ineligible_until` in the same UPDATE (a dead row is parked, not waiting).
- **`requeued_at` — retry counter scope.** Policy queries count only attempts
  since the last re-queue
  (`COUNT(*) FROM runs WHERE caused_by=? AND status IN ('failed','crashed') AND
  started_at > COALESCE(requeued_at, 0)`), so re-queue grants a fresh budget,
  not one slow attempt. `runs` stays the single source of truth (no denormalized
  counter), and the column preserves *that a human intervened, and when*.

**All failures count toward the threshold, including conflict-retry
exhaustion** (exempting a pre-judged-innocent type is the self-classification
we don't trust; an exempt type retries forever at full pipeline cost). The
error string still names conflict-retry exhaustion for the human. **The check
lives at failure time**, applied by whatever marks the run (the boot sweep does
the identical thing when marking `crashed`): compute the since-re-queue count;
at the threshold set `dead_at` (and emit the notification event), otherwise set
`ineligible_until`. Selection stays policy-free. **Threshold
`WIKI_RUN_ATTEMPTS_MAX`, config, default 5** (five exponentially-spaced attempts
span ~2.5h — rides out common transients while capping poison input).

**Batch failures:** a failed batch run leaves the cron row pending (its
all-succeeded join doesn't fire), which *is* the retry mechanism; the policy
acts on `runs.caused_by` (the cron row) with zero new machinery. **Dead-letter
granularity = the causing row only, never event rows** — no whole-batch
dead-letter (would park 199 innocents), no poison-row hunting (compile rarely
dies on one row; realistic failures are call-shaped). A true poison event row
surfaces as recurring loud cycle deaths; the human fix is to set `dead_at` on
the offending event row manually.

**Notification: dead-lettering notifies; individual attempts don't** (attempts
are self-healing; pushing on every transient failure is alarm fatigue). The
mechanism is the eventplane producer.

## 8. Eventplane surface — producer of exactly two events

The wiki is an eventplane producer via the standard appkit outbox / `/feed`
(the suite's only sanctioned notify mechanism — publish a fact, let **notify**
consume it; a bespoke side-channel would be the suite's first private API
chain). **Exactly two events, nothing else:**

- **`wiki.row_dead_lettered`** — payload: inbox id, source, title, last error.
  Emitted by the failure-path code that sets `dead_at`, in that transaction.
- **`wiki.ingest_refused`** — payload: door, source, size, cap. Refusal is
  pre-accept, so a plain outbox write at the door.

Both are consumed by notify; a hosted script can bind later for free. No
`wiki.page_updated` / `wiki.subject_created` / per-run events — nobody consumes
them; a new event is one outbox write, added when a real consumer exists.

## 9. The read side

### 9.1 Hosted ask first

The read surface is **hosted-ask-first**: the wiki runs the retrieval loop
server-side, as its own agent; the caller spends one tool call and gets a cited
answer, never a navigation session. The caller's context window is the scarce
resource — context isolation as a service (the design's spend-at-server-time
principle applied to context instead of money). The retrieval-quality research
(iteration beats index sophistication) survives — the iteration just runs
server-side where prompt and citation discipline are ours to control.

**Ask is strictly read-only** — it writes nothing (the old `synthesis/<slug>.md`
file-back is removed: an answer carries no new evidence, would cite pages not
arrivals, and would rot with no repairer). Payoff: a read-only ask needs no
flight lock, no optimistic commit, no transaction — it runs fully parallel with
integration. The escape hatch: an answer worth keeping is produced as a
document and `ingest_text`-ed through the full pipeline. **Ask is synchronous**
— the call blocks and returns the cited answer (the async poll loop burns
exactly the caller context hosted-ask protects; read-only ask contends with
nothing, so the queueing justification is gone). The inner agent gets a
server-side budget (max turns / tokens / wall-clock).

**Public read surface = ask + search + timeline.** A zero-LLM `search` verb
survives as a **side door** for exact/known-item fetch (ask's inner agent needs
the primitive anyway, and "show me the Acme page" through an LLM loop is pure
overhead). The risk — callers reaching for the cheap verb when they should ask
— is mitigated by **steering tool descriptions** (`search` = exact/known-item
fetch; `ask` = the default for any question, "answers come back cited; do not
assemble answers from search results yourself"); description text is part of the
design surface.

### 9.2 Ask's inner toolset — six read tools

1. `search(query)` — the same FTS the public search verb uses;
2. `lookup(name)` — registry alias → subject (exact identity resolution, the
   corpus's structural advantage over generic RAG);
3. `read_page(subject)` — full page body;
4. `read_source(inbox_id)` — follow a citation to the original arrival's payload
   (`ReadPayload`, size-capped) — the designed payoff of citation discipline
   (raw truth one hop away);
5. `timeline(from, to)` — zero-LLM registry query: event subjects with
   `occurred_at` in the window;
6. `related(subject)` — neighbors in the derived page-mention link graph;
   **goldens-gated, not built until the cross-subject goldens show bare ask
   failing.**

Tools 1–3 are the public primitives, identical implementation (one code path
serves both surfaces). The tool list *is* the write-license enforcement
(read-only by construction). No list-all/browse tool.

**Cross-subject and temporal-span queries** (the query classes per-subject
prose answers worst, per `wiki-prior-art-research.md`) are answered by **views
rendered on demand over the registry, never stored derived knowledge** (the
journal/index rejection pattern). `timeline(from, to)` is **built now** (event
subjects whose `occurred_at` falls in the window; ISO-8601 prefix args,
lexicographic range) and **exposed as a public verb** — its description must
frame it as "list event subjects in a date window," never "answer questions
about a period." `related(subject)` + the derived link graph (pages mention
other subjects; the normalize machinery makes mentions mechanically resolvable;
zero LLM; always derived and rebuildable, never the representation) is
**goldens-gated** and stays **internal**. **Stored communities / cached
thematic summaries are rejected** (derived prose that rots — killed for the
fourth time); if the link graph proves insufficient, the honest option is
raising ask's budget, not caching.

**The answer contract — page-level citations.** Ask's answer cites pages
(subject id + title); inbox/arrival ids appear only when the inner agent pulled
the original via `read_source` and drew on it directly. A citation must be
followable by its reader (the caller can fetch a cited page via the public
search verb; an inbox id is caller-opaque). The chain reaches bedrock in one
hop: answer cites page → caller fetches page → the page's statements carry their
inbox citations. A contradiction found on a page is *surfaced* (both sides, both
citations), never silently resolved.

**Ask accounting — its own `asks` table** (not a `runs` row — ask has no
causing row, no in-flight claim, no retries; jamming it into `runs` would bend
`caused_by` and the failure-path policy): `id, owner, question, status,
started_at, finished_at, usage, error`. Lifecycle mirrors runs where honest
(insert `running`, finalize at end, boot sweep marks orphans `crashed`).
Storing `question` is golden gold for the eval engine.

The ask prompt has six sections (task framing — **answers come from pages, not
the model's world knowledge**, "the wiki has nothing on this" over fabrication;
corpus model; procedure — names → `lookup` first, else `search`, `read_source`
only when pages disagree or exact wording is needed, **reformulate and retry
before concluding absence**; answer craft; budget discipline; output schema +
worked example).

### 9.3 The index — hybrid by design

The index is **hybrid — FTS5 (BM25) + an embedding lane**. The embedding lane
is a **committed component designed now**, not evidence-gated; FTS5-first is
build sequencing only. The lexical side already covers exact semantic
resolution for every judged name (aliases) and much phrasing variance (ask's
iteration); the embedding lane covers the residual (claim-prose phrasing
variance, question-shaped queries sharing no tokens with any page).

**One hybrid retriever primitive, two lanes (BM25, vector), rank-fused, serving
three call sites** — the search verb / ask's search tool, resolution's
candidates step, and lint-sweep. The vector lane is **independently switchable
per call site, turned on only when measurement shows the lift** (all three are
the same recall problem feeding a downstream precision filter — ask's agent,
the match judge, the dup judge — that absorbs embedding false-positives).

- **Embedding unit — one vector per page** (the subject is the unit; pages are
  already the compressed artifact a chunk approximates). Embedded text =
  canonical name + alias list + page body (truncated at the model's input
  limit) — baking the registry's identity judgments into the vector. One
  embedding call per page write, one row per page.
- **Write path — asynchronous catch-up, never in the integration commit.** The
  vector row records the `embedded_version` it embedded; the work list is a
  query (vector missing, or behind the page's current `version`, or wrong
  `model`); a small in-process embedder wakes on run completions (the nudge
  pattern), sweeps stale vectors, batches the API calls. Vector staleness is
  benign (the lexical lane is transactionally current); keeps the embedding
  provider out of the integration failure story (a provider outage pauses
  catch-up; runs never see it). Not a spine job — a goroutine, not machinery.
  **Read-side degradation:** when a read-time embed call fails, the retriever
  falls back to lexical-only.
- **Vector storage — plain table, brute-force cosine scan in Go, no extension**
  (pure-Go chassis rules out sqlite-vec; a full scan over ~5k × 1024-dim
  vectors is ~20MB of dot products, <10ms — ANN is for millions). Brute force
  is *exact*, trivially testable, zero tuning surface.

  ```sql
  page_vectors(
    subject           TEXT PRIMARY KEY,   -- = pages.subject
    embedded_version  INTEGER NOT NULL,   -- pages.version at embed time
    model             TEXT NOT NULL,      -- provider/model id + dims, e.g. text-embedding-3-large@1024
    vector            BLOB NOT NULL       -- float32 array
  )
  ```

  **Stale vectors serve until replaced** — reads never compare
  `embedded_version` to `pages.version` (filtering stale vectors would make a
  page vanish from the lane right when it just gained information);
  `embedded_version` drives only the catch-up work list. **Model match *is* a
  read-side validity condition** (cross-model cosine is meaningless garbage;
  absence beats it) — the read lane uses only `model = current` rows.
- **Provider — OpenAI `text-embedding-3-large` @ 1024 dims**, one model for
  documents and queries (OpenAI models don't share an embedding space). Config
  `WIKI_EMBED_MODEL` + `WIKI_EMBED_DIMS`; secret `OPENAI_API_KEY` presence-gated
  at the composition root — absent key disables the lane (lexical-only), the
  `ANTHROPIC_API_KEY` pattern. The deciding reason: the suite is adding OpenAI
  as a future agent engine (provider alignment); cost is a non-factor (~cents/
  month). **A model/dims change is just a config change** the catch-up worker
  absorbs (the work list gains the `model != current` clause; first deploy and
  model change are the same code path).
- **Fusion — RRF (k=60, top ~50 per lane in) for the ranked consumers** (search,
  cut to the caller's limit; candidates, cut to ~5) — rank positions, no
  normalization, one conventional parameter. **Sweep does not fuse** — it
  thresholds evidence per lane (existing FTS threshold + a cosine threshold,
  both eval-harness knobs); over-flagging stays the cheap direction.

**The search verb contract:** input `query` (required) + `limit` (default
**3**, cap **10**); **resolution order registry-first** (an exact alias match
pins that subject's page at rank 1, the hybrid retriever fills the remainder);
**a hit is the whole page** (subject id, type, kind, title, full body,
citations intact); **rank order only, no scores** (RRF scores are fusion
artifacts); **nothing prepended** (no `index.md`).

## 10. LLM call-site discipline (the config-injection constraint)

This is a hard constraint on the implementation, stated so the out-of-band
evaluation engine can drive tuning later: **every LLM call site takes its
prompt, model, and effort from injected config, never from constants.** The
call sites are extract, match, merge, compile, ask, and the three lint LLM
calls (the dup judge, the fold, the stale repair). None may hard-code a prompt
string, a model id, or an effort/budget setting; all three are supplied by
configuration resolved at the composition root and threaded to the call site.
The evaluation engine itself has **zero CI/CD role** and is **not** a
precondition or blocker for this work — its only footprint on this design is
this constraint. Nothing else about the engine belongs here.

## 11. Open items

These are genuinely undecided or deferred; do **not** treat them as resolved.

- **Schema finals — the consolidated DDL is not yet assembled.** Riders
  scattered through the design must be folded into one final set of `CREATE`
  statements: `version INTEGER` on `pages` (optimistic commit); `conflicts` on
  `runs` (and the `runs.integrator` → `runs.job` rename); `ineligible_until`,
  `dead_at`, `requeued_at` (all nullable epoch-ms) on `inbox`; `occurred_at`
  (nullable TEXT ISO-8601 prefix, events only) and confirming `kind` on
  `subjects`; `judged_version_a/_b` (nullable) on `dup_flags`; the
  `stale_notes` table; the `page_vectors` table; the `asks` table; the FTS5
  tables over `pages`; the eventplane system-identity literal (the `owner`
  value for consumer doors) and the two events' payload shapes.

- **Exact prompts.** Only rough section-shapes exist for extract, match, merge,
  compile, and ask, and for the three lint calls (dup judge, fold, stale
  repair). The literal prompt text and worked examples are deferred — as is
  whether a shared invariants block (lead discipline, citation rules, salience
  polarity) is factored out across prompts.

- **Exact models per call site**, and **config defaults.** Which model serves
  each call site (match is small-and-cheap shaped; extract, merge, ask likely
  differ — ask is agent-shaped, likely merge-class) is open. So are the config
  defaults not yet pinned: lint job cadences, ask budget knobs (max turns /
  tokens / wall clock), and the embed model/dims defaults (recorded as
  `text-embedding-3-large` @ 1024 but eval-arbitrable).

- **The one genuine build-time fork — the digest claimable unit.** *Framing 1*:
  a worker grabs the **cron row** (one claim) and runs all its bound entries
  sequentially (simplest selection; needs the leave-pending-if-locked-out
  handling). *Framing 2*: the claimable unit is a single **`(cron-row, entry)`
  pair**, so two workers naturally run two entries of one trigger concurrently
  and the stamp is a pure completion-time join with no leave-pending case
  (costs a selection step expanding cron rows into entry candidates). Both are
  correct and contention-equivalent — **decide at build.**

(The benchmark / eval harness and the read-side **retrieval goldens** — and the
cross-subject / temporal-span ask goldens that gate `related` — are tracked in
the decision log but are out of scope here per the evaluation-engine boundary;
they consume this design, they do not gate it.)
