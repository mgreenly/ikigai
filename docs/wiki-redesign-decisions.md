# wiki redesign — running decisions

> Status: **in-progress decision log**. Captured from the design walk-through of
> the wiki's running process, step by step. This supersedes the approach in
> `wiki-rewrite-design.md` where they conflict. When the walk is complete this
> feeds a proper `<slug>-design.md` per `docs/README.md`. Covered so far: the
> **ingest side** (front doors → acceptance → inbox → self-selecting workers →
> integrators)
> and the **document pass** (extract → resolve → merge, the registry, the page
> store), the **digest pass**, and the **concurrency rule** (N identical
> self-selecting workers — no central dispatcher — over optimistic commit),
> and the **failure policy** (bounded retries +
> dead-letter, settled), and **lint** (three jobs on the spine, settled),
> and **search/ask** (the read side: hosted-ask-first + hybrid index,
> settled), and **re-integration** (rejected — stamps permanent, settled),
> and the **open-list sweep** (compile prompt shape, `occurred_at`, source
> pages, `index.md`, size cap / no chunking, transcripts, `owner`,
> eventplane producer — all settled), and the **cross-subject/temporal
> response** to the prior-art study (`timeline` + goldens-gated
> `related`, settled — see `docs/wiki-prior-art-research.md`).
> Remaining open: schema finals, eval harness, exact prompts, exact
> models, config defaults.
>
> The execution model is the **dispatcher-free identical-worker pool**: N
> identical worker goroutines self-select pending work under one in-flight-set
> mutex, in place of a single dispatcher goroutine handing work to a dumb pool.
> The two are the *same design under two concurrency idioms* (goroutine-confined
> selection state + channel signaling vs. mutex-guarded shared state), shown
> correctness-, crash-, and contention-equivalent; the worker model was chosen on
> **legibility** (explicit over implicit, mechanical verification). This was
> worked out in a design detour and folded in here; the dispatch/concurrency,
> stamping, and failure-backoff sections below describe the worker model.

## Vocabulary

| term | meaning |
|---|---|
| **ingest / acceptance** | receiving input and durably recording it in the inbox. Cheap, deterministic, never touches an LLM. The *only* contract of ingest: "your input is recorded and will eventually be reflected." |
| **front door** | any code path that receives input and calls `Accept`: the MCP verbs (`ingest_text`, `ingest_url`) and the eventplane consumer loops. Thin adapters, not a layer. |
| **inbox** | the queue of accepted-but-not-yet-integrated arrivals: one SQLite table + a blob directory. Pending = `integrated_by = ''`. |
| **worker** | one of N identical goroutines (pool size `WIKI_INTEGRATION_WORKERS`, default 4). Each runs the same loop: self-select the next eligible unit of work under the in-flight-set mutex, run the integrator, commit. No central decider role exists — workers are homogeneous. |
| **selection** | the mutex-guarded critical section in which a worker picks the highest-priority eligible inbox row and claims it in the in-memory in-flight set. *This is where "what runs next" is decided* — the lone dispatcher of the earlier design, re-expressed as a lock. Wakes on the same signals the dispatcher's `select` watched; never reads feeds itself. |
| **integrator** | code that turns inbox rows into wiki content. A worker, having selected a row, runs the matching integrator. Two policies: eager (document pass) and batch (digests). |
| **run** | one execution of one integrator. A row in a runs table (ULID id, integrator name, status, timestamps, usage, error). The provenance key. |
| **digest** | a batch integrator's compiled summary of accumulated events, given one integration pass. |

Naming discipline: **ingest = into the inbox; integrate = out of the inbox.**
"Episode" (Zep/Graphiti vocabulary) is rejected — we say inbox rows / arrivals.
"Idle-time / sleep-time" is rejected — integrators always work when there is
work; the only throttles are external (API limits, spend).

## Core decision — acceptance is decoupled from integration

Both prior designs coupled arrival to processing (the current code runs an
agent job per document immediately; the first rewrite projected events at
ingest time). We decouple completely:

- Acceptance writes the inbox and returns. Synchronous, transactional, no LLM.
- Integration is a downstream consumer of the inbox, running on its own
  policy (eager or batch), restartable and re-runnable.
- This is the only shape that handles all front doors (pasted note, dropbox
  burst, event stream, transcript) with one contract, and it lets every later
  question (what to derive, when, with which model) change without touching
  the write path.

## Events: accept raw, integrate as digests

Ingesting every tiny event into the knowledge layer is a structural problem:
the suite's event stream is dominated by activity, not knowledge; per-event
integration fragments the wiki into micro-facts and duplicates the services'
own systems of record, at maximum LLM cost for minimum knowledge.

Decision:

- The wiki keeps consuming raw feeds (no producer-side digests, no new
  producer work). Events are **accepted into the inbox only**.
- On a cron-fired schedule, a batch integrator compiles everything pending
  into a **digest** and gives it one integration pass. Aggregation is itself
  knowledge creation ("the deal closed after three weeks" is wiki-worthy; the
  14 underlying events are not).
- Default cadence: daily. Cadence is config, not code. **No volume trigger** —
  batch runs on schedule, full stop; a heavy day just makes a bigger digest.

## The inbox

### Storage rule — small payloads in the row, large to disk

- `len(bytes) <= WIKI_INBOX_INLINE_MAX` (default **4096**) → bytes stored in
  the row's `content` column.
- Larger → content-addressed blob file at `blobs/<aa>/<sha256>` (2-hex-char
  fan-out, `WIKI_BLOB_FANOUT = 2`); row stores the reference.
- Rationale: suite events are hundreds of bytes — one file per event is ~20×
  block-waste on ext4 plus per-file syscall/backup overhead, and SQLite reads
  small blobs faster than the filesystem. Documents/PDFs go to disk so the
  database grows by a row per item, not by content volume.
- Invariants that make the threshold tunable later with no migration:
  - `sha256` computed and stored **always**, both paths.
  - The row records which path was taken; readers dispatch on the row, never
    on the current threshold.
  - One accessor (`ReadPayload(row)`) is the only code that knows two paths
    exist.
  - Blob write order: write → fsync → insert row. Orphan blobs are legal and
    sweepable; rows pointing at missing blobs are a bug.
- Deliberately **not** a parameter: any mime/binary rule. Size alone decides.

### Size cap — `WIKI_INGEST_MAX_BYTES` at Accept; no chunking (locked)

**Accept refuses oversized input loudly at the front door; ingest-side
chunking is not built.** Config `WIKI_INGEST_MAX_BYTES`, default
**131072** (128 KiB ≈ ~33k tokens) — deliberately quite conservative:
comfortably inside any extract model's window with room for prompt,
context header, and output; real documents (memos, contracts, exports)
sit far below it; the cap is config when evidence says otherwise.

- **Boundary validation doing its job.** Extract is one full-context
  call; a document that can't fit fails every attempt identically and
  would dead-letter after `WIKI_RUN_ATTEMPTS_MAX` — five LLM-call
  attempts to discover what `LENGTH(bytes)` knew at the door. Refusal at
  Accept never makes the ingest promise ("will be reflected") that the
  pipeline provably can't keep.
- **Rider — non-interactive doors notify on refusal.** An MCP caller
  sees the error; an eventplane consumer loop (e.g. a huge dropbox file)
  has nobody watching, so a front-door refusal there must notify —
  folds into the notification-mechanism item.
- **Chunking is speculative structure.** If oversized-but-valuable
  documents ever appear, the contained switch is the already-noted
  fan-out extract (split → extract per window → union manifests),
  entirely behind extract's data contract. Until then the human remedy
  is honest: split the document and ingest the pieces.
- Scope: ingest-side only — page-side chunking was settled separately
  in the embedding-unit lock.

### Schema

```sql
inbox(
  id            TEXT PRIMARY KEY,            -- ULID, identifies the ARRIVAL
  owner         TEXT NOT NULL,               -- X-Owner-Email; attribution, never isolation
  kind          TEXT NOT NULL,               -- document | event
  source        TEXT NOT NULL,               -- one prefixed string, see below
  sha256        TEXT NOT NULL,               -- content identity, always computed
  size          INTEGER NOT NULL,
  mime          TEXT NOT NULL DEFAULT '',
  content       BLOB,                        -- inline payload (≤ 4KB), NULL if spilled
  blob          INTEGER NOT NULL DEFAULT 0,  -- 1 = bytes at blobs/<aa>/<sha256>
  title         TEXT NOT NULL DEFAULT '',    -- caller-supplied, optional
  tags          TEXT NOT NULL DEFAULT '[]',  -- JSON array, caller-supplied
  received_at   INTEGER NOT NULL,            -- epoch ms; when the inbox accepted it
  integrated_by TEXT NOT NULL DEFAULT ''     -- run id; '' = pending
)
-- indexes: (integrated_by), (sha256)
```

Decisions baked into that schema, with reasoning:

- **`kind` is binary: `document | event`** — it classifies what the content
  *is* (routing), not how it arrived (provenance). text/url/file collapsed
  into `document`: they all route to the same integrator. A `transcript` kind
  is added only if/when its routing actually diverges.
  - **Transcripts — settled: they are documents.** One arrival, one blob,
    one coherent text — mechanically the document pass's shape exactly;
    not an event pile, so digest/compile has nothing to offer. The real
    transcript concern is extraction *quality*, not routing: dialog is
    low-density and asserts things differently (decisions vs discussion,
    who said what). That is a prompt problem — the contained fix is a
    dialog-aware clause in the extract prompt (claims attribute speakers
    where it matters: "Bob proposed X" vs "X is true"; conversational
    back-and-forth dies at the salience gate) — rider to exact-prompts.
    `source` already preserves what it was (`prompts:…`, `mcp:…`). The
    escalation trigger stands unchanged: `transcript` earns a kind value
    only if routing genuinely diverges someday.
- **`source` is one prefixed string** — `url:https://…`, `dropbox:/notes/x.md`,
  `mcp:<caller-supplied>`, `crm:contact.created`. Door + origin, for humans
  and integrator prompts; not split into columns, not indexed.
- **Timestamps are INTEGER epoch milliseconds, not RFC3339 TEXT.** On merit:
  8 bytes vs ~25 on the highest-volume table; native comparisons; one
  canonical representation (the codebase already has RFC3339 *and*
  RFC3339Nano — the TEXT failure mode in miniature: two valid formats that
  sort wrong against each other). TEXT's only real win is eyeballing rows,
  recoverable via `datetime(x/1000,'unixepoch')`.
- **`received_at` is the ONLY timestamp.** `occurred_at` (world time) was
  considered and *removed*: it is interpretation, not arrival fact — derived
  per-door with fallbacks and trust caveats (e.g. dropbox events stamp
  apply-time, which degrades to "today" on a re-bootstrap). Integrators
  resolve world time downstream from the stored payload (envelope `time` as
  default; per-known-event-type extractors where we've taught it; document
  dates for files) and record it on the facts/pages they produce. The inbox
  knows when things *arrived*; the knowledge layer knows when things
  *happened*. Integration logic must never order knowledge by `received_at`.
- **No UNIQUE on sha256.** Two arrivals of identical content are two rows
  (different `received_at`, possibly different source); bytes dedup via the
  hash, arrivals don't. Feed re-delivery dedup is the consumer cursor's job.
- **`integrated_by` instead of a processed boolean.** `''` = pending;
  otherwise the ULID of the run that consumed the row. Same pending query,
  plus audit ("which inputs fed digest X?") for free. (The stamp is
  permanent — never cleared; see "Re-integration — rejected". Re-queue of
  a *dead-lettered* row clears `dead_at`, not this column — a dead row
  was never stamped.)
- **ULID id, not content hash** — id identifies the arrival, sha256 the
  content. Provenance threads on the id; dedup reasons on the hash.
- **`collection` is CUT** (was in GOALS.md as a future-multi-wiki key,
  defaulted `'default'`). Speculative structure; if multi-wiki is ever wanted
  it's an ALTER TABLE + directory move.
- **`owner` stays — as attribution, never isolation (locked).** The account
  is a business; many users share the box, so `X-Owner-Email` varies per
  request — the column answers "who ingested this" / "who asked this"
  (audit, provenance, per-user golden candidates from `asks`). It is NOT
  an auth scope: **knowledge is shared per box** — pages and subjects
  carry no owner, everyone's documents merge into the business's one
  wiki, ask reads everything. The `(owner, integrated_by)` index dropped
  to `(integrated_by)` — owner-scoped pending queries don't exist; the
  workers' selection predicate is global. Eventplane consumer doors
  stamp a system identity (no human acted; exact literal → schema
  finals).
- `attrs` JSON catch-all deliberately omitted until something needs it.

## Acceptance — one function

```go
Accept(owner, kind, source, mime, title, tags string, bytes []byte)
    (id, sha256 string, dup bool, err error)
```

The only code that writes the inbox: hash, size, inline-vs-spill, insert row,
nudge the workers (broadcast on the wake `sync.Cond`/channel). Front doors are
thin adapters doing door-specific work
*before* calling it:

- `ingest_text` → `Accept` directly with the bytes.
- `ingest_url` → fetch + extract server-side, then `Accept`.
- eventplane consumers → see below.

There is **no synthetic event envelope** wrapping MCP-arrived content. The
inbox row *is* the common shape; each door just fills the columns from what it
natively has. Selection never reads `content`; `kind` is the entire
interface between "what arrived" and "what happens next."

### MCP return contract — a receipt, not a job

`ingest_text`/`ingest_url` return at acceptance: **inbox id + sha256 +
duplicate flag**. Meaning: "recorded, will be integrated." No job id exists at
that moment (integration may run seconds or hours later). A caller that cares
about integration polls a status verb against the inbox id (`integrated_by`
empty = pending; set = done, and the run row says succeeded/failed).

### Provenance chain

The inbox id flows through the whole system: answer → page (cites inbox ids) →
inbox row (source, hash, bytes) → original input. In reverse: "what did the
wiki do with what I ingested?" via `integrated_by` → run → its outputs. The
Provenance invariant becomes a mechanical key instead of a convention.

## Process topology

```
N consumer goroutines (one SSE connection per subscribed feed:
dropbox, crm, ledger, …, cron)
        │  handler per event:
        │    domain event            → Accept(kind=event, envelope JSON verbatim)
        │    dropbox file event      → fetch content_url → Accept(kind=document)
        │    cron.<name>             → Accept(kind=event, source="cron:<name>")
        │  cursor commits only after Accept returns (at-least-once; dedup via hash)
        ▼
N identical worker goroutines (WIKI_INTEGRATION_WORKERS), each looping:
   select the next eligible row under the in-flight-set mutex (see "Selection")
   → run the integrator → commit → drop the in-flight claim
   an idle worker blocks on a wake `sync.Cond`/channel, broadcast on:
   arrival nudge (sent by Accept) │ run completion │ ineligible_until timer │ shutdown
        ▼
integrators (runs)
```

- Consumer loops cannot share a select (blocking SSE reads aren't channels);
  one goroutine per feed is the existing eventplane pattern.
- Workers have no SSE connections and no feed knowledge — they read only the
  inbox.
- The **nudge is an optimization, not the truth**: signals carry no data, may
  be lost or duplicated. On any wake (and at boot) a worker re-runs selection
  against the inbox for pending work. Crash between acceptance and integration
  loses nothing.
- **Cron events go through `Accept` like every other event** (revised from
  an earlier trigger-message design). Consumers are fully uniform — every
  handler ends at `Accept`, every cursor commits after it — and the workers'
  wake has exactly one signal type, the contentless nudge.
  The decisive argument: a pending `cron:<name>` row is *durable batch
  authorization*. If every worker is busy, the row waits in the table; if
  the process crashes before the digests run — including this suite's
  scheduled-downtime deploys, which make "restart near the nightly tick"
  routine — the boot scan finds the row and fires the digests late instead
  of skipping a cycle. No in-memory owed-work state anywhere. The earlier
  design's "a lost trigger skips one cycle" contract is gone because
  triggers can no longer be lost. (The nudge remains expendable: signals
  decide *when to look*, the table decides *what exists*.)
- The dropbox case clarified: the *event* is delivery mechanism, the *file* is
  the knowledge — the consumer fetches the bytes and stores a `document` row;
  the envelope is used and discarded. Only suite domain events are stored *as*
  events.

## Selection — the per-worker decision loop

There is no central dispatcher. Each of the N identical workers runs the same
loop; when it finishes a run (or wakes idle) it re-runs **selection** — the
critical section under the in-flight-set mutex that picks and claims the next
unit of work. The priority and eligibility are exactly what the lone dispatcher
applied; the only change is that the selecting goroutine is whichever worker is
free, guarded by a lock instead of being the sole goroutine:

```
lock(inflight)            -- the selection critical section
  -- highest-priority eligible unit, in order; take the first, claim it,
  -- release the lock; the claim is in-memory only (see "Concurrency",
  -- "Item 2 — crash-resume"):
  1. a pending cron row whose bound job name(s) aren't already in flight
        → claim it (TryLock the bound job name(s)) — will run the bound digest(s)
  2. else the oldest pending document row not already in the in-flight set
        → claim its row id — will run one document pass
  3. else nothing eligible
        → block on the wake `sync.Cond` (arrival / completion / timer)
unlock(inflight)

run the claimed work (no lock held)  →  commit  →  drop the claim  →  loop
```

- **Cron before documents** (decided): cron rows are scheduled work that is
  already late by definition; documents are queue work with no deadline.
  The reverse ordering can starve a digest for hours behind a 50-file
  dropbox burst draining serially. Cron rows are rare and consumed in one
  cycle each, so documents cannot starve under this ordering.
- **At-most-one-in-flight is a `TryLock` per work key**, not central
  enforcement: key = **job name** for digests/lint (their work is chosen by a
  shared selector, so two concurrent runs of one entry would read the same
  pending rows), key = **inbox row id** for the document pass (so the pool
  isn't serialized — many document passes run at once, the guard only stops two
  workers grabbing the *same* row). Details under "Concurrency — Item 1".
- Event rows are invisible to selection — no step asks about them. They are
  touched only *inside* digest runs, via the batch table's selectors.
- Authorization is unchanged in substance: a document row authorizes work by
  existing; batch work is still authorized by schedule — the cron row exists
  only because the schedule fired. Arbitrary pending event volume still
  never starts a digest.
- Three row roles fall out: **document** causes work immediately; **cron**
  causes work on schedule; **event** never causes anything — it waits to be
  swept up.
- A cron row no batch entry binds is stamped immediately by the selecting
  worker as a no-op (otherwise it would sit pending forever); config knowledge
  lives only in the workers' `jobs` config, never in consumers.

### Stamping — who clears `integrated_by`, and when

The principle: **a row is stamped by whatever fulfills its promise, at the
moment the promise becomes true — never earlier.**

- **Document and event rows** promise "this content will be reflected."
  The run that consumes them stamps them **inside its end-of-run
  transaction**, atomically with the pages/registry writes. The stamp
  exists if and only if the content does; a crash on either side of the
  commit leaves a consistent world (both or neither). Stamping at claim
  time would let a crash mid-run orphan the row as "done" with nothing
  written.
- **Cron rows** promise "the digests bound to this trigger will run." No
  run consumes them (they match no selector), and one trigger can legally
  bind two digests — two runs, two separate transactions — so there is no
  single transaction the stamp could ride in. Instead the cron row is
  stamped by a **worker-local completion-time join**: the worker finishing
  a bound digest run queries "do all bound entries for this `caused_by` now
  have a *succeeded* run?"; if yes, it stamps the cron row with
  `UPDATE inbox SET integrated_by=… WHERE id=? AND integrated_by=''`. The
  `WHERE integrated_by=''` makes a double-stamp race (two workers finishing
  the last two entries at once) a harmless no-op. The stamp fires only once
  **all** bound digest runs have **succeeded** (not merely completed — a
  failed run leaves the join unsatisfied and the cron row pending, which is
  the retry authorization, see "Failure policy → Batch failures").
- The procedure is uniform — a worker runs the claimed work (an iteration of
  one for a document), and on completion stamps the causing row *if still
  unstamped*. For documents the run's atomic stamp already happened inside
  the commit, so the after-stamp is a no-op; the completion-time join only
  ever bites for rows no single run consumes (cron). This is identical to
  the earlier design's "stamp once all bound runs succeeded," run as a query
  at completion instead of from central tracking.
- The cron stamp's crash window (digests committed, stamp not yet written)
  is accepted because **re-firing is mechanically idempotent**: boot finds
  the cron row pending → re-runs the bound digests → their selectors query
  *pending* event rows and find zero (the first runs' commits stamped
  them) → empty runs, then the stamp. Partial completion recovers the same
  way: if crm-digest crashed while ledger-digest committed, the re-fire
  finds ledger's selector drained and only crm's rows still pending — only
  the unfinished work re-runs. Worst case of any crash here is an empty
  run row, never double-processing. This fallback is acceptable *only*
  where re-firing is mechanically harmless — for content rows it is not
  (a re-run document pass re-mutates pages on the merge agent's judgment,
  "LLM-idempotent" at best), which is why content rows get the atomic
  stamp instead.

### Re-integration — rejected; stamps are permanent

**`integrated_by` is never cleared. There is no replay of successful
runs.** Re-integration was never a built feature — only a possibility
that falls out of `pending = integrated_by = ''` — and it is now
explicitly forbidden rather than given semantics.

- **Corrections enter through the front door.** Bad or stale page content
  is fixed by ingesting a new document that asserts the correction. Merge
  is already built for exactly this: order-tolerance is locked, claims
  carry their own dates, contradictions keep both sides, corroboration
  adds citations. No new machinery. (A correction is new content with a
  new sha256, so dedup doesn't block it — unlike re-submitting the same
  bytes, which dup-flags.)
- **Retry never needed it.** A failed run never writes the stamp (it
  rides the end-of-run transaction), so the row stays pending and re-runs
  under the failure policy. Replay of *successful* runs was the only
  thing re-integration could mean, and that is what's rejected.
- **Provenance stays honest.** `integrated_by → runs.id` always tells the
  true and complete story of what happened to a row. Every replay
  semantics considered (double-cite, supersede, page rollback) either
  muddied that join or demanded heavy machinery (per-run page edit
  tracking, page history) the design doesn't have.
- **Pipeline upgrades don't replay either.** Better prompts/models later
  improve documents going forward; existing pages are repaired at the
  page layer by lint, not by re-folding old documents.

Accepted cost, stated plainly: if a run *succeeds* but its merge judgment
was bad, the remedy is corrective ingestion or page-layer repair — never
replay.

Eager rules: **one document per run** — no debounce, no combined
multi-document passes; quality stays per-document. Runs execute
concurrently on the worker pool (see "Concurrency — the worker pool";
this replaces the earlier strictly-serial single-flight proposal, which
was never agreed). Selection order stays oldest-first, so a burst
(50-file dropbox dump) is still *selected* as an orderly queue in
arrival order; commits may complete out of order (forfeited explicitly,
see Concurrency).

### Batch table (config)

Multiple batch integrators bind trigger → integrator → row selector:

```
batch:
  - name: crm-digest
    trigger: cron.daily
    select: source LIKE 'crm:%'
  - name: ledger-digest
    trigger: cron.weekly
    select: source LIKE 'ledger:%'
```

- The wiki subscribes to the cron feed once; *which* cron events matter is
  purely this table. New digest = config line + user creates the schedule in
  the cron service. No code.
- Selectors must **partition** event rows. Startup checks: overlapping
  selectors → refuse to boot; consumed sources matched by no selector →
  surface it (else rows sit pending forever).
- Two entries on one trigger is legal.

### Concurrency — the worker pool

Integration runs execute on a pool of `WIKI_INTEGRATION_WORKERS` **identical
workers** (config, default **4**). This **replaces the single-flight proposal**
(which was recorded as a proposal and never agreed). There is no central
dispatcher: each worker self-selects its next unit of work in the selection
critical section (see "Selection"), and selection is the *only* serialized
point — the run itself holds no lock. The mutex-guarded selection critical
section **is** the lone dispatcher of the earlier design, re-expressed as a
lock; the two are correctness-, crash-, and contention-equivalent (Items 1–3
below), and the worker model was chosen on **legibility** (explicit over
implicit, mechanical verification — see the status block). Merge buffers page
writes in memory during the run; the database is touched only by the
milliseconds-long end-of-run commit, never while an LLM thinks. Safety rests on
the mechanisms below, each matched to a hazard.

**Item 1 — at-most-one-in-flight is a `TryLock` per work key, not central
enforcement.** The rule is a non-blocking `TryLock` keyed by the work item,
which every (identical) worker computes from the item's kind:

- **Digests and lint jobs → key = job name** (`crm-digest`, `lint-dups`). Their
  work is chosen by a *shared selector*, so two concurrent runs of the same
  entry would read the same pending rows before either stamps. A worker tries
  the lock; if held, that job "is not a current option" and the worker moves
  on. Different job names run concurrently (selectors partition).
- **Document pass → key = inbox row id**, NOT the integrator. A single lock for
  the whole document pass would serialize the very pool we built to run
  documents concurrently. The dedup is per-row: many document passes run at
  once, the guard only stops two workers grabbing the *same* row.

This is the in-flight set re-expressed as locks; it costs one mutex where a
lone dispatcher cost zero, and a mutex is trivial.

**Item 2 — crash-resume rides one rule already in the design: stamp only at
commit, never at claim.** There are three row states but only **two are
durable** — pending (`integrated_by=''`) and done (`integrated_by=<run id>`,
written only inside the end-of-run commit). "In progress" is **not durable**:
it is membership in the in-memory in-flight set (RAM only). Because "done" is
stamped only inside the closing transaction (atomic with the page/registry
writes), a crash can only ever leave a row **pending** — the partial run wrote
nothing, so restart simply re-selects it. Crash-resume is automatic and needs
no cleanup; dedup is in RAM precisely so a crash ignores it (the set wipes on
crash, restart begins empty, the DB is the sole truth). The durable `running`
row in `runs` is kept — but for **accounting, not dedup or resume**: it is the
status record the MCP poll resolves through, and it is how *process death* gets
counted — a clean failure marks its own run `failed`; a process that dies
mid-run writes nothing, so the **boot sweep flips orphaned `running` →
`crashed`** and a crashed run counts one attempt toward `WIKI_RUN_ATTEMPTS_MAX`
(see "Runs and the commit"). The `running` row does **not** gate re-selection
(selection keys off `integrated_by` + the in-memory set), so the boot sweep
reconciles *accounting*, not *resume*.

**Item 3 — SQLite contention is identical to the lone-dispatcher model.** The
per-run DB write footprint is the same in both: one `running` insert at start,
one end-of-run commit, and *no* DB write for the dedup claim (it is the
in-memory set). Selection is a serialized critical section (one worker selects
at a time under the mutex) — the same one-at-a-time selection the lone
dispatcher did, not N concurrent select-reads; under WAL reads don't block the
writer regardless. The only real write contention in *either* model is N
workers' commits occasionally queuing on SQLite's single writer —
milliseconds-long, "never while an LLM thinks," a non-event at this scale.
Both need WAL + `busy_timeout` equally; there is no SQLite-contention argument
that distinguishes the two models.

**Lost page updates → optimistic commit.** Two runs touching one page is
the classic lost update: each reads, thinks for minutes, rewrites the
*whole* page (merge never appends), commits — the second commit would
silently erase the first's claims and citations, violating citation
preservation with no error raised. Mechanism: pages carry a `version`
INTEGER bumped on every write; the manifest records the version each page
had when merge read it; the end-of-run transaction writes via
`UPDATE pages SET body=?, version=version+1 WHERE subject=? AND version=?`
and treats zero rows affected as a conflict → roll back the whole
transaction, re-read the changed page, re-run **merge only** for that page
(extract and resolve outputs cannot go stale — claims and identities
don't, prose does), recommit. Rejected alternatives: pessimistic page
locks (held over LLM-minutes; degenerate to single-flight exactly under
correlated bursts — the case the pool exists for) and admission control
(impossible: the write set is the manifest's pages, unknown until
extract+resolve finish, mid-run).

**Duplicate subject minting → the alias UNIQUE is the guard.** Two runs
both extract a not-yet-registered "Acme Corp"; both lookup-miss, both
decide create-it. Page versioning can't catch this (two *new* pages, no
shared row). The guard is already in the schema: every create-it queues
alias rows for all surface forms, SQLite's single writer serializes the
two commits, and the loser's alias insert hits `UNIQUE(type, norm)` → the
whole transaction rolls back → restart at **resolve** for the colliding
subject only: the lookup replays and now *hits* the winner's
freshly-committed alias rows (or goes to match on a many-id bridge, like
any bridge), the claims retarget the winner's existing page, merge re-runs
for that page. **Extract is never re-entered for any conflict type** —
nothing another run does can invalidate what *this document said*. Safety
is an equivalence argument: the retry replays exactly the path serial
execution would have taken (doc B's lookup hitting after doc A's mint);
concurrency adds retry cost, never a different answer. Edge: a *found-it*
alias attachment can also hit UNIQUE — if the existing row points at the
same subject_id it's a harmless race (`ON CONFLICT DO NOTHING`); a
different subject_id is bridging evidence, routed through the same
conflict arm.

**The conflict loop — shared bookkeeping for both conflict types.**
Statements execute one at a time inside the commit transaction, so the
failing statement itself identifies the colliding page/subject — no
diagnosis step; any conflict rolls back everything, leaving the database
exactly as if the run had never reached commit. The retry re-enters the
*existing* stage functions (resolve, merge) with the existing extract
output — no special-case code path. Bounds: **cap of 3 commit attempts,
then fail cleanly** — `status=failed`, error names **conflict-retry
exhaustion** (the in-run commit-retry budget is exhausted, by conflicts —
distinguishable from real failures; the term is deliberately not plain
"retry exhaustion," which would blur it with the run-level retry
budget, the dead-letter threshold: two different budgets, 3 commit
attempts inside one run vs. N runs per causing row), inbox row stays pending, the normal
re-selection machinery takes over (one more run row; the eventual re-run
re-pays extract+resolve — accepted, it's the rare path and the
already-designed recovery shape). In-run retries are deliberately
**unspaced**: each retry *is* a merge call — minutes long, high natural
latency variance — already spaced and jittered for free; scheduled sleep
on top burns a worker exactly when workers are scarce. Post-exhaustion
re-selection **is** delayed: oldest-first selection would otherwise send the
exhausted row straight back to the front of the queue, into the same storm
that beat it, at full pipeline cost. The delay mechanism and formula are
**finalized in "Failure policy"** (`ineligible_until` on inbox, set on
every failed run — every transient failure wants the same delay, not a
conflict-specific one; the formula there generalizes this case's original
flat `random(2–4) × run duration` proposal to grow with attempt count, and
attempt 1 is identical to it).

**Conflict metrics — a requirement, not an option.** `conflicts INTEGER
DEFAULT 0` on `runs`: how many collisions this run retried through. "Is
the pool creating problems" must be a query, not archaeology: rate =
`SUM(conflicts)/COUNT(*)` over any window; worst offenders = runs ordered
by `conflicts`; watch whether the rate climbs with pool size. Log lines
alone rejected — the question arrives weeks after the evidence scrolled
away. The run is already the unit of accounting (it has `usage`); retries
are another cost the run paid.

**Digests in the pool — two rules.** A digest's *page* writes are covered
by optimistic commit like everyone else's; event rows need their own
rules:

- **At most one in-flight run per batch-table entry name**, enforced by the
  job-name `TryLock` of Item 1 (a worker that can't take the lock skips that
  entry and moves on — no central check-then-start). Two concurrent runs of
  the *same* entry — legal whenever two of its cron rows are pending, e.g.
  boot finds yesterday's alongside today's — would both select the same
  pending event rows *before either stamps* and compile them into pages twice;
  the page version check cannot catch it (both commits look valid — it catches
  stale prose, not duplicate claims). Run *serially* the same pair is
  already safe: the second run finds a drained selector and completes as
  an empty run — the locked re-fire idempotency. The `TryLock` adds no new
  safety property; it forces same-entry runs back into the serial order
  where the idempotency the design already paid for actually works. The
  locked-out cron row just stays pending (durable authorization waiting is
  its designed behavior) and fires on a later wake — when re-picked, the
  other run has drained that entry's selector, so it succeeds as a cheap empty
  run and the all-succeeded join then stamps the cron row. Different entries
  concurrent = fine: selectors partition event rows.
- **Stamp by id list, never by selector.** The end-of-run transaction
  stamps exactly the row ids compile read at run start — never
  `UPDATE … WHERE <selector> AND pending` re-evaluated at commit, which
  would stamp rows that arrived mid-run (Accept runs concurrently with
  every run) and were never compiled: silently lost knowledge — nothing
  pending, nothing to retry, no error anywhere, the worst failure class
  in the design. Unread mid-run arrivals stay pending and sweep into the
  next cycle, which is the batch contract anyway. (The hazard pre-dates
  the pool — Accept was always concurrent — promoted from implementation
  accident to stated invariant; it is the stamping principle applied:
  a row compile never read had no promise fulfilled.)
- **A cron row is a tiny fan-out, not one run.** The worker that claims a cron
  row looks up the bound job entries for that trigger (config; every worker has
  it) and runs each bound digest; each digest run is its own `runs` row with
  `caused_by = cron-row-id`, stamps its event rows by id-list at commit, and
  the cron row itself is stamped by the all-succeeded join (see "Stamping").
  Crash mid-digest leaves the cron row pending → restart re-picks it → bound
  digests re-run → already-stamped event rows are drained (cheap empty runs):
  the same re-fire idempotency, uniform with documents.
- **Open sub-question (not yet decided): the claimable unit for digests.**
  *Framing 1* — a worker grabs the **cron row** (one claim, like any row) and
  runs all its bound entries sequentially; simplest selection, matches the
  "cron row is just another claim" model, needs the leave-pending-if-locked-out
  handling above. *Framing 2* — the claimable unit is a single
  **`(cron-row, entry)` pair**, so two workers naturally run two entries of one
  trigger concurrently and the stamp is a pure completion-time join with no
  leave-pending case; costs a selection step that expands cron rows into entry
  candidates. Both are correct and contention-equivalent; decide at build.

**Event-consumption semantics (confirmed).** Two distinct guarantees, two
mechanisms: **partition is the routing guarantee** — selectors are required to
partition the event rows and boot refuses to start on overlap (and surfaces any
consumed-but-unmatched source), so two digests never *target* the same row;
**the stamp is the once-only guarantee** — a set `integrated_by` removes the row
from all future *pending* queries, making consumption final. Exactly one
integrator consumes each content row, permanently (the stamp is one run id,
never an array, never cleared). Cron rows are the exception to "consumed by an
integrator": no run consumes a cron row as content — it is *authorization*,
stamped by the all-bound-runs-succeeded join, not a content sweep.

**Ordering — forfeited explicitly.** Single-flight integrated documents
in strict arrival order; the pool commits out of order (doc A, arrived
first with a long merge, can land after doc B). Accepted with no
mechanism, because the property was already worthless: "integration logic
must never order knowledge by `received_at`" is locked (the inbox knows
arrival, not world time), serial integration already processed
world-history out of order routinely (a re-bootstrap delivers a 2019 memo
today), and merge is therefore already required to be order-tolerant —
claims carry their own resolved dates, corroboration adds citations,
contradictions keep both sides. The pool reordering commits by minutes
adds nothing serial hadn't already imposed at the scale of years. What
the pool does *not* reorder: selection stays oldest-first, so arrival
order remains the fairness order for who gets a worker — no starvation.

## Integration — the document pass

The document pass is a fixed pipeline per inbox row:

```
inbox row → extract → resolve → merge → commit
            (LLM)    (mostly    (LLM    (one SQLite
                      mechanical) agent)  transaction)
```

### Vocabulary (locked)

| term | meaning |
|---|---|
| **subject** | a thing the wiki has a page about. Three types, closed set: `entity` (has identity — person, org, product, place; `kind` is freeform subtype), `event` (happened at a time), `concept` (idea / topic / method). |
| **claim** | a short prose statement a document asserts about one subject. The unit merge folds into a page, always cited to the inbox id. |
| **registry** | the `subjects` + `aliases` tables — every subject ever minted plus every name it has ever been known by. |
| **lookup** | deterministic resolution: normalized name → `aliases` table → subject_id. |
| **candidates** | deterministic recall on a lookup miss: FTS shortlist of plausible existing subjects. |
| **match** | the one LLM call in resolution: judge the candidates, answer **found it** (`same(id)`) or **create it** (`no_match`). |
| **manifest** | the in-memory work order for one run: every extracted subject annotated with its resolved subject_id + target page + claims. Never persisted; the run id is its durable identity. |

Rejected terms: "verdict"/"determination" (match's answer is binding for the
run but not final — lint can still merge later); "uncertain" as a match
outcome (see below).

### Subject taxonomy — the three types

The type set is closed and each type carries a decision test, not just
examples:

| type | test | examples |
|---|---|---|
| `entity` | has identity — it **is** something | a person, a company, an office, a product |
| `event` | happened — it **occurred at a time** | a meeting, a deal closing, an outage |
| `concept` | an idea — topic, method, practice | double-entry bookkeeping, the deploy process, RAG |

### `occurred_at` — a registry column on event subjects (locked)

**`subjects.occurred_at` — nullable TEXT, ISO-8601 prefix, populated only
for `type=event`, written in the end-of-run commit from the manifest.**
This is where the value extract/compile emit finally lands (the inbox
locked `received_at` as its only timestamp; world time resolves
downstream — this column is the downstream).

- **Registry, not frontmatter.** "Query events by date" (which the
  no-journal-page decision leans on) needs a SQL-queryable column. The
  thin-frontmatter lock exists precisely to keep facts that need
  querying out of page text. And the registry already holds this class
  of fact — identity-level attributes of a subject (type, kind, names);
  "when it happened" is identity-level for an event the way "what kind"
  is for an entity.
- **ISO-8601 prefix, not epoch-ms.** Events have honest variable
  precision ("the 2019 reorg" vs "Tuesday's standup"); the prefix form
  (`"2019"`, `"2019-06"`, `"2019-06-11T14:00"`) encodes precision
  without a second column and sorts/ranges correctly lexicographically.
  Epoch-ms would force fake precision onto fuzzy dates.
- **First-writer-wins.** A later run emitting a different value does not
  overwrite the column — that is a contradiction, and contradictions are
  page territory (corralled in the body). Mechanical rule, no new
  judgment call site.

Rider: the column itself lands in schema finals.

### Substrate — prose pages, stored as SQLite rows

- The knowledge layer is **prose subject pages**: one page per subject,
  agent-curated, readable, compressed. Structured alternatives (facts tables,
  graphs) re-import the GraphRAG/Zep ontology complexity already rejected at
  ingest. Prose is the representation an LLM reads/writes natively, and the
  digest decision already committed us to prose artifacts.
- Pages live in a **`pages` table in the service's SQLite, not loose files**,
  with FTS5 over it. Decisive argument: the end-of-run transaction (pages +
  registry + run row + `integrated_by`) is only airtight if pages are rows —
  files can't join a transaction. Also: one backup story, no separate index
  to skew, no path-confinement code. The merge agent's read/write tools serve
  pages by path; storage is invisible to it.
- **`raw/` dies.** The inbox (rows + `blobs/`) is the durable original-bytes
  store; a second sha-keyed copy would be a parallel identity scheme. Pages
  cite inbox ids.

### Source pages — die; the inbox row is the document's sole representation (locked)

**No per-document page exists.** A document's knowledge is dismembered
into subject pages; the document itself is represented only by its inbox
row + blob, reached via citations + `read_source`.

- Every argument that killed the journal page applies verbatim: a
  per-document page never compounds (no future claim lands on "the memo
  from June 3rd"), re-narrates bytes the inbox already holds durably,
  and pollutes FTS with never-revisited pages.
- The access path already exists and is uniform: page claim → citation
  `[inbox id]` → `read_source`. A source page would be a second, staler
  way to see the same bytes.
- It would also be the only page type with no write license after
  creation — the rot-with-no-repairer shape that killed synthesis pages.
- Carve-out needing no machinery: a document that genuinely **is** a
  subject ("the Acme master contract," a policy doc referred to by name)
  gets a page the ordinary way — extract emits it as an `entity` when it
  passes the salience gate. A page *about* the document as a thing in
  the world, earned on merit, not a routine per-document artifact.

### `index.md` — dies; the registry is the index (locked)

**No index page exists, and merge carries no index obligation.**

- Its navigation role is fully obsoleted: the registry (`subjects` +
  `aliases`) is a complete, queryable catalog; search (FTS + exact-alias
  pin) and ask's inner lookup/search tools are the discovery paths. A
  prose index re-narrates the `subjects` table — the exact shape that
  killed the journal and source pages.
- The maintenance cost is uniquely bad under the concurrency lock: an
  index merge must update on every subject-minting run makes one global
  page a write hotspot every concurrent run contends on — guaranteed
  optimistic-commit conflicts, manufactured by design, on the least
  valuable page in the system.
- If "browse what the wiki knows" is ever wanted as a product surface,
  it is a **view rendered on demand from the registry** — same
  resolution as "show me my day": a view, never stored knowledge.

(This closes the rider left open in the search verb contract's
"Nothing prepended" decision.)

### Extract — one full-context structured call

- Input: the whole document + the subject schema. Output: JSON — subjects,
  each `{type, kind, name, aliases, claims[], occurred_at (events)}`. **No
  tools, no wiki access, no loop** — single-pass extraction over full context
  is the model's native competence, and tool-free calls are golden-testable.

  ```json
  {
    "subjects": [
      {
        "type": "entity",         // closed set: entity | event | concept
        "kind": "organization",   // freeform; prompt anchors person/organization/place/thing
        "name": "Acme Corp",      // as the document states it
        "aliases": ["Acme"],      // other surface forms in this document
        "claims": ["Signed a three-year supply agreement with us on …"],
        "occurred_at": null       // events only
      }
    ]
  }
  ```

  `kind` is **freeform, prompt-anchored**: the schema enforces nothing, but
  the prompt names a few exemplar kinds so the model converges on a small de
  facto vocabulary without us maintaining a controlled one.

#### Extract prompt — rough shape (six sections)

1. **Task framing.** Extracting knowledge for a personal/business wiki:
   the subjects this document carries information about and what it asserts
   about each. The framing to nail: **the document is a carrier, not the
   topic** — split its content by subject, don't summarize the document.
2. **The subject schema.** The three types with their tests, one or two
   examples each. Kind: freeform, entity anchors named (person,
   organization, place, thing), explicit permission to be more specific.
   One classification per subject.
3. **Salience — what earns extraction.** The gate, both halves:
   - *Identifiable*: the Wikipedia-article test, plus a concrete never-list
     (Graphiti's, nearly verbatim — field-tested): no pronouns, no bare
     generic nouns ("the team", "work", "the meeting" unless a particular
     nameable meeting), no dates-as-subjects, no unnamed instances.
   - *Claim-bearing*: a subject must have at least one claim.
     Mentioned-but-nothing-asserted = not extracted.
   - Polarity stated in words: **when in doubt, do not extract.**
     Sub-salient information is not lost — state it as a claim on the
     subject it's really about ("Bob's dad drives a Tesla" → claim on Bob).
4. **Identity discipline.** The co-reference/disambiguation pair:
   - One real-world thing mentioned five ways = one subject; all surface
     forms in aliases.
   - One string meaning two things = two subjects (Apollo the program,
     Apollo the launch).
   - Names verbatim as the document states them — no expansion, no
     canonicalization. If the doc only ever says "AWS," emit "AWS."
5. **Claims discipline.** The section that most determines merge quality:
   - A claim is a short prose statement asserting one thing about one
     subject.
   - **Self-contained**: read later on its subject's page, far from this
     document — no pronouns, no "the company," no document-relative
     references; names spelled out.
   - Asserted by the document, not inferred beyond it — extract is
     transcription-with-splitting, not analysis. Synthesis is merge's job.
   - Resolve relative time inside claims ("last Tuesday" → the date, using
     the context header) and set `occurred_at` on events.
   - Document contradicts itself → emit both claims; extract doesn't
     referee.
6. **Output schema recap + one worked example.** One small input→output
   pair — the researched extraction-prompt lineages (GraphRAG, Graphiti)
   lean on few-shot more than instruction text for format conformance.

#### Context header

The extract call's input is three concatenated parts: the prompt (above) +
a **context header** + the document bytes. The header is a few lines built
mechanically from the inbox row — `source`, `title` (if any), `tags` (if
any), `received_at` rendered as a date:

```
source: dropbox:/clients/acme/meeting-notes.md
title: Acme kickoff
received: 2026-06-11
```

Used only by the extract model during that one call; never stored, never
seen by resolve or merge. Why it exists: the bytes alone lack facts extract
needs — a date anchor for resolving relative time, and identity evidence
("the meeting" + path `/clients/acme/meeting-notes.md` → maybe nameable).

The date is framed as **"received on", never "today is"**: the inbox knows
arrival time, not world time (a re-bootstrap can deliver a 2019 memo
today). The prompt instructs: prefer dates stated inside the document; fall
back to the received date only when the document gives no better anchor;
if a relative reference can't be confidently resolved, keep it relative
rather than guess. Same polarity as match — doubt produces the cheap,
lint-visible failure (an unresolved "last Tuesday"), not the poisonous one
(a confident wrong absolute date).

Deliberately excluded: `owner`, `mime`, the inbox id (extract doesn't
cite; the pipeline attaches the id mechanically downstream).
- Output budget is **not** capped small; full-context responses. The
  depth-dilution argument for per-subject fan-out calls thus mostly
  evaporates; fan-out (identify subjects, then one call per subject) stays a
  back-pocket internal upgrade behind the same subjects-out contract, adopted
  only if goldens show tail-thinning on dense documents.
- Extract resolves **within-document co-reference**: "Amazon Web Services" /
  "AWS" / "Amazon's cloud" in one document = one subject with three surface
  forms. Cross-document identity is the registry's job, not extract's.
- Names are emitted **as the document states them** — never pre-canonicalized
  by the model; normalization is the registry's job and "helpful" model-side
  cleanup destroys the evidence lookup needs.
- A claim in any document can land on any subject's page — the document is a
  carrier; extract splits its content by subject. This is why the wiki
  compounds: a page is built from every claim about its subject ever seen,
  not from "documents about" it.

### Resolution — mechanical first, LLM second

The extracted name is never an address; it's a lookup key. The address is a
registry subject_id.

```sql
subjects(id ULID, type, canonical_name, page, created_by_run)
aliases(norm TEXT, subject_id, UNIQUE(type, norm))
```

Per subject: build the key set `normalize(name) ∪ normalize(aliases)`
(normalize = NFKC, casefold, trim, collapse whitespace, strip diacritics —
pure string code, versioned, rebuildable). One query:
`SELECT DISTINCT subject_id FROM aliases WHERE type=? AND norm IN (keys)`.

- **One id** → resolved, free, no LLM. (Companion surface forms not yet in
  the table are added as alias rows.)
- **Many ids** → the document's own aliases bridge two registry subjects.
  The conflicting subjects **are** the candidate set — straight to match, no
  search. Bridging evidence also dup-flags the pair (see lint queue). A
  create-it answer is still legal here: known names don't make the subject
  known (this doc's "Bob Smith" may be a third person).
- **Zero ids** → candidates: two FTS queries, same type only, top ~5,
  deterministic (pinned tie-break), no score threshold initially —
  (1) name/alias tokens vs. registry names (lexical near-miss);
  (2) claim text vs. page bodies (catches zero-token-overlap synonyms like
  AWS / "Amazon Cloud"). Loose recall is fine; precision is the judge's job.
  **Zero candidates → create it, no LLM** (rare on a warm wiki — accepted).
  Thresholding is a later tuning knob, verified against resolution goldens;
  the failure direction matters: over-filtering silently mints duplicates,
  under-filtering just spends a cheap call answering "no match".
- **Match** — one structured, tool-less call judging the whole shortlist at
  once: subject (name/aliases/claims) + candidate excerpts in, **binary** out:
  `same(id)` or `no_match`. Prompted: match only on clear evidence; **doubt
  is no_match** — false-split is cheap and lint-repairable, false-merge
  poisons pages. "Uncertain" was removed from the contract: it differed from
  no_match only in bookkeeping, and uncertainty options in LLM output schemas
  become confidence sinks. (A dup-flag side effect on low-confidence
  no_match can return later without widening the contract.)
- **Actions** (all queued for the end-of-run transaction):
  - *found it* — alias rows for all incoming surface forms → found id (at
    match level they're new by construction; lookup-hit needed none).
  - *create it* — new subject row; all surface forms (canonical name
    included) become its first alias rows; fresh page planned.
- Every judgment is written back as alias rows, so **judgment converts to
  mechanism**: each name is LLM-decided at most once, then it's a string
  lookup forever. The aliases table is the accumulated history of "this
  string was judged to mean that subject."

#### Match prompt — rough shape (five sections)

1. **Task framing.** An identity judge for the wiki registry: one incoming
   subject (extracted from a document) vs. a shortlist of existing registry
   subjects of the same type. The question is whether the incoming subject
   is *the same real-world thing* as one of the candidates. The framing to
   nail: **identity, not similarity** — "very alike" is not "the same."
2. **The evidence.** Incoming side: type, kind, name, aliases, claims (the
   document's own words). Candidate side, per candidate: subject id,
   canonical name, all registry aliases, and a page excerpt. Claims are
   evidence, often decisive — two "Bob Smith"s split on whether the claims
   fit the candidate's page (different employer, different city).
3. **Decision rule + polarity.** Binary, exactly one answer over the whole
   shortlist: `same(id)` or `no_match`. Match only on clear evidence of
   shared identity; **doubt is no_match**, with the reason stated in the
   prompt: false-split is cheap and lint-repairable, false-merge poisons
   pages. Name equality alone is not clear evidence; claim coherence plus
   name is.
4. **The side channel.** If two *candidates* appear to be the same thing as
   each other, report the pair — independent of the main answer. Feeds
   `dup_flags` (writer #2); does not widen the verdict contract.
5. **Output schema + one worked example.**
   `{decision: "same"|"no_match", subject_id?, dup_pairs?: [[id,id]]}` —
   few-shot for format conformance, same lesson as extract.

The two sides are structurally asymmetric: the incoming subject has raw
claims and no prose (its page may not exist yet); the candidate has prose
and no claims (claims are never persisted — merge compressed them into the
page, which *is* the candidate's knowledge record). Match therefore
compares uncompressed evidence against compressed evidence — acceptable
because identity ("who/what is this") survives compression well.

**Candidate excerpt** = canonical name + full registry alias list + the
first `WIKI_MATCH_EXCERPT_CHARS` characters of the page body (config,
default **600**). Lead prose is where identity facts concentrate; full
bodies blow up the small-cheap call for no identity signal. If goldens
show truncation-driven identity errors, the knob turns.

**Cross-prompt invariant this creates:** the merge prompt must keep every
page's lead identity-establishing — who/what it is, kind, key
relationships up front, recent activity below. If merge buries identity
under activity, match degrades silently. This is an obligation the merge
prompt owes the match call site.

### Dup flags — lint's work queue (sketch, details parked)

`dup_flags(subject_a, subject_b, run_id, status: open|merged|dismissed)`.
A row = evidence that two **existing** subjects may be the same. Writers so
far: many-id lookups (document bridged the pair), match noticing look-alikes
among candidates (`same` with an "also" side channel). **Resolution never
merges subjects inline** — merging (repoint aliases, fold pages, retire the
loser) is lint's job, done completely or not at all. Incoming *names* are
cheap to attach now; existing *subjects* only ever get flagged now, merged
later. Dismissed rows persist to stop re-flagging. Pair-order normalization +
UNIQUE, and lint's own semantic sweep as a fourth writer, are parked for the
lint discussion.

### Merge — one agent run per document, prose in

- Input: the manifest. **One agent run per document** (not per page, not per
  subject): cross-page coherence (the event page and its entity pages tell
  one consistent story) and exactly one run row meaning "this document's
  integration." Per-page calls remain a contained later switch.
- **Write set = the manifest's pages, exactly.** New evidence is the only
  license to write, and extract decides where the evidence points. No
  link-following expansion (unbounded, invites unlicensed "improvements").
  Read set is looser: the agent may read neighbors for context. Staleness
  noticed on a non-manifest page cannot be fixed in-run; that's lint's
  ground (a note-for-lint side channel is a possible later addition).
- Per page, fold **that subject's claims only** into the body as **prose,
  not a ledger**: new info woven in; already-known gets the new citation on
  the existing statement (corroboration); contradictions are flagged, both
  statements and citations kept. Compression-at-write-time *is* the
  knowledge creation that justifies the merge agent; an appended claim
  ledger is just the inbox re-sorted by subject, re-digested by every
  reader forever. Raw truth stays one citation-hop away in the inbox.
- Tools: read + write pages, nothing else.
- **Merge sees the manifest only — never the original document.** Extract
  already split the document into self-contained claims (claims discipline
  exists precisely so claims survive away from their source). Giving merge
  the raw document invites re-extraction: finding things the salience gate
  deliberately excluded and writing them anywhere, dissolving both the gate
  and the write-set license. Cost accepted: merge can't recover nuance
  extract dropped — the correct fix location is extract's goldens, not
  merge freelancing.

#### Merge prompt — rough shape (six sections)

1. **Task framing.** The curator of a personal/business wiki. A document
   was ingested; its content is already split into claims, each assigned
   to a subject and a page. Fold each page's claims into that page so it
   reads as the current, confident account of its subject. The framing to
   nail: **writing knowledge, not logging input** — compression at write
   time is the job; an appended claim-ledger is failure.
2. **License and write set.** The manifest lists the pages — write those,
   exactly those. New evidence is the only license to write; there is no
   license to "improve" other pages read for context. Reading neighbors
   for consistency is allowed. Staleness noticed elsewhere is not yours to
   fix.
3. **Page craft.** Per page, fold that subject's claims only:
   - New information → woven into the prose where it belongs, not
     appended.
   - Already-known → add the new citation to the existing statement
     (corroboration strengthens, not repeats).
   - Contradiction → both statements kept, both cited, corralled in the
     marked contradictions section — never interleaved as hedging.
   - **Lead discipline (the match obligation):** the first paragraph
     states who/what this subject is — identity, kind, key relationships.
     Recent activity lives below the lead. Every rewrite leaves the lead
     identity-establishing.
4. **Citations.** Every statement carries inline inbox-id citations
   `[01HX4…]`. Hard invariant: every citation in the old body survives the
   rewrite, or the statement it supported was deliberately superseded —
   citations are never lost by paraphrase.
5. **New pages.** When the manifest says create: frontmatter from the
   registry row (subject, type, kind, title — nothing more), then a lead
   built from the claims. A one-claim page is legitimately one sentence —
   don't pad.
6. **Procedure + worked example.** Read the page, rewrite it whole, write
   it back. One small before/after pair (old page + claims in → new page
   out) — few-shot for conformance, same lesson as extract and match.

### Page anatomy

```markdown
---
subject: 01H2X…   # registry id — the page's true key
type: entity
kind: company
title: Amazon Web Services
---
Body: current, confident prose. Every statement carries inline inbox-id
citations like [01HX4…]. Contradictions corralled in a marked section,
not interleaved as hedging.
```

- Frontmatter is thin: subject, type, kind, title. Aliases live in the
  registry (not duplicated to rot); provenance lives in citations; history
  lives in runs. No per-page edit log.
- Citation preservation is mechanically checkable: every inbox id in the old
  body survives a rewrite or its statement was deliberately superseded.

### Runs and the commit

- A **run** is one execution of one integrator — a row in `runs`. It is the
  provenance key (`inbox.integrated_by`), the in-flight lock that survives
  restarts, and what the MCP status verb resolves through. Descendant of
  `wiki_jobs`.

  ```sql
  runs(
    id          TEXT PRIMARY KEY,  -- ULID
    integrator  TEXT NOT NULL,     -- 'document-pass' | 'crm-digest' | …
    caused_by   TEXT NOT NULL,     -- inbox id of the causing row
    status      TEXT NOT NULL,     -- running | succeeded | failed | crashed
    started_at  INTEGER NOT NULL,  -- epoch ms
    finished_at INTEGER,
    usage       TEXT,              -- token/cost accounting
    conflicts   INTEGER NOT NULL DEFAULT 0, -- optimistic-commit retries
                                            -- (see "Concurrency")
    error       TEXT NOT NULL DEFAULT ''
  )
  ```

- The two tables cross-reference, one column each way:
  `inbox.integrated_by → runs.id` ("which run consumed this arrival") and
  `runs.caused_by → inbox.id` ("which arrival caused this run"). Together
  they make provenance, the post-ingest status poll, and retry counting
  (`COUNT(*) WHERE caused_by = ? AND status IN ('failed','crashed')`) all
  single queries.
- **One row per attempt.** A document that fails twice and succeeds on the
  third try has three run rows; the history of the struggle is preserved,
  and the inbox stamp points at the one that succeeded.
- Lifecycle: the `running` row is inserted **before** the run executes —
  the one write outside the end-of-run transaction; it is the durable
  in-flight marker. Then exactly one of:
  - **succeeded** — set inside the end-of-run transaction, atomic with
    pages, registry, and input stamps;
  - **failed** — clean failure, status + error updated, transaction never
    committed, inbox row still pending;
  - **crashed** — the process died and could report nothing; the row is
    stuck at `running` until the **boot sweep** marks every orphaned
    `running` row `crashed`. After the sweep, crashed and failed are
    indistinguishable to retry logic: one more failed attempt for that
    causing row.
- Failure taxonomy this supports (policy since settled — see "Failure
  policy"): clean
  failures split into *transient* (API down — retry works) and
  *persistent* (poison input — retries loop forever, politely). Perfect
  error handling in code does not remove the need for a retry/dead-letter
  policy; it only changes how failures are reported. Process death is the
  one failure code cannot turn into a valid path, and it costs exactly one
  mechanism: the boot sweep.
- 1:1:1 — one inbox row, one manifest, one merge run.
- **One SQLite transaction at end of run**: updated/created pages, registry
  inserts, dup flags, the run row, `integrated_by`. Mid-run there are zero
  partial writes anywhere; a crash leaves the document untouched and pending.
- Known seam, deliberately deferred: "pages touched by run X" requires
  scanning bodies for citation ids (updates aren't column-stamped); a
  `page_edits(run_id, page)` table at commit is the fix if the query is ever
  needed.

### Determinism ledger

| step | nature |
|---|---|
| accept → inbox | deterministic |
| extract | LLM, structured call, no tools |
| normalize / lookup / candidates | deterministic |
| match | LLM, structured call, only on ambiguity |
| merge | LLM, agent, read/write pages only |
| registry writes, citations, commit | deterministic |

Clean boundaries are the point: each LLM stage hides behind a data contract
(subjects-out, found-or-create, pages-in-pages-out) so any stage can be
re-implemented (fan-out extract, per-page merge) without touching neighbors.

## Integration — the digest pass

The digest integrator reuses the document pass from resolve onward; its only
new machinery is **compile**, which plays extract's role for event piles:

```
document pass:  bytes      → extract ─┐
                                       ├─→ resolve → manifest → merge → commit
digest:         event rows → compile ─┘
```

- **Compile targets extract's output schema directly** — subjects with
  typed/kinded names, aliases, claims, `occurred_at` — and the digest enters
  the shared pipeline at resolve. (Refines the earlier hunch, which had
  compile writing a prose digest fed through the *whole* pipeline including
  extract: that prose step was a lossy round-trip — flattening what the
  compile model already knows into prose, then paying a second call to
  re-extract it. No prose digest artifact exists at all.)
- Compile is the same call shape as extract: **one structured, tool-less,
  golden-testable LLM call**. Its prompt is where the locked
  "aggregation is knowledge creation" decision lives: fourteen
  `deal.stage_changed` events in, one claim out ("the Acme deal closed after
  three weeks of negotiation"). The salience gate works even harder here —
  most event minutiae must die in compile. Swept ≠ became knowledge: every
  selected row gets stamped; only salient aggregate claims survive.
- **`occurred_at` is resolved in compile** from the event payloads (envelope
  time, per-known-event-type extractors) — this is where the "integrators
  resolve world time downstream" promise from the inbox design lands.
- **Per-claim citations.** Events are presented to compile with their inbox
  ids visible; each claim carries `cites: [inbox ids]` naming the specific
  event rows that support it. A digest's claims have no single source, and
  "cite all 200 swept rows" is meaningless — only the compile model knows
  which events support which claim. New risk accepted: the model can
  mis-attribute an id (the document path structurally cannot); this is a
  golden-testable behavior.
- **Citation semantics stay uniform**: every citation in every page points
  at an arrival from the outside world. A document claim cites its document
  row; a digest claim cites raw event rows. Rejected alternative: `Accept`
  the compiled output back as a synthetic inbox row and cite that — it
  makes "follow a citation" two-cased (sometimes primary evidence, sometimes
  the wiki quoting itself) for every consumer, forever.
- **The manifest generalizes to make this invisible to merge**: every claim
  is `{text, cites[]}`. The document pass fills `cites` mechanically with
  its one row id; compile fills it per-claim. Merge weaves text and appends
  the ids it was handed — it cannot tell which integrator ran.
- **No journal page.** A digest leaves no day-shaped residue: by the time
  merge runs, the day is dismembered into per-subject claims. A daily
  journal page was considered and rejected: it never compounds (no future
  claim lands on "June 10"), it re-narrates a log the system already has
  (the inbox ordered by `received_at`), and it pollutes FTS with
  never-revisited pages. The time axis is served by `type=event` subjects
  carrying `occurred_at` plus the inbox itself; if "show me my day" is ever
  wanted as a product surface, it's a *view* rendered on demand by
  search/ask, never stored knowledge.

### Compile prompt — rough shape (extract's skeleton + four deltas)

**Locked: compile is not a new prompt species.** Its output contract is
identical to extract's, and four of extract's six sections (subject
schema, identity discipline, most of claims discipline, output recap)
apply verbatim — so the compile prompt is the extract prompt's
six-section skeleton with these deltas:

1. **Task framing** swaps "the document is a carrier" for the
   aggregation framing: the input is a pile of routine machine events;
   the job is *compression into the few durable claims a human would
   still care about next month* — narrate outcomes, not deltas.
2. **Salience** gains the harder polarity: per-event micro-facts ("stage
   changed X→Y at 14:02") are presumed noise; a claim must aggregate or
   conclude. Swept ≠ extracted is stated in words.
3. **Claims discipline** gains two obligations: every claim carries
   `cites` naming the specific supporting event ids (and only those),
   and `occurred_at` is resolved from event payload / envelope time.
4. **Worked example** is event-shaped: a dozen events in → two or three
   aggregate claims out, with correct cites — id mis-attribution is the
   risk the few-shot teaches against.

One deliberate divergence on the transcription rule: extract is locked
to transcription-without-inference; compile *concludes across events*
(that is "aggregation is knowledge creation") but still never infers
beyond what the events support. The line moves from "don't synthesize"
to "synthesize only what the cited events jointly assert."

Riders: literal text + worked examples → exact-prompts item; model →
exact-models item (compile is extract-class shaped).

## Failure policy

**The frame is locked: bounded retries + dead-letter.** A failing causing row
gets a bounded number of automatic attempts; at the threshold it is parked
dead — visible, alertable — and only a human re-queues it. Rationale: the
alternatives lose on the design's own principles. Unbounded retry loops
forever on poison input, politely burning LLM spend — silent budget
corruption, not failing loudly. Drop-on-first-failure breaks the ingest
contract ("your input is recorded and will eventually be reflected") for
transient blips like an API outage. Bounded-then-dead-letter is the only
shape where transient failures self-heal, persistent failures stop spending
money, and the row stays durable and visible for a human — matching the
inbox's existing recovery shape (the row never moves; re-queue is clearing a
column). Details (threshold, backoff, the dead mark, counting rules, digest
granularity, notification) follow below.

### Backoff — `ineligible_until`, set on every failed run

**Locked: every failed run sets `ineligible_until` on its causing row — one
delay mechanism, no failure-type dispatch.** The mechanism is as proposed in
the concurrency walk: a nullable epoch-ms column on `inbox`; the pending
predicate gains `AND (ineligible_until IS NULL OR ineligible_until <= now)`;
the workers gain a fourth wake source — a timer (the wake `sync.Cond`/channel
broadcast) armed to the earliest future `ineligible_until`. Rationale for the
uniform rule: the argument that motivated the delay for conflict-retry
exhaustion ("oldest-first sends the failed row straight back to the front of
the queue, into the same conditions that beat it") applies verbatim to every
transient failure — an API outage that failed the run at minute 2 is still down
at minute 3 when oldest-first re-selects it; immediate re-selection burns a
worker and a counted attempt for nothing. And dispatching on failure type is impossible
anyway: code cannot reliably tell transient from persistent — that is
exactly why the dead-letter threshold exists (persistent failures reveal
themselves by exhausting retries, not by self-identifying). Poison input
pays a few delayed retries on its way to dead-letter; accepted as cheap.

**The formula (locked):**

```
ineligible_until = now + random(2–4) × avg_run_duration × 2^(failures−1)
```

- `avg_run_duration` = recent average from `runs`, floored at **60s** (a
  cold database has no run history).
- `failures` = the per-row attempt count, the already-designed single query
  (`COUNT(*) FROM runs WHERE caused_by=? AND status IN ('failed','crashed')`).
- `random(2–4)` is the jitter, unchanged from the conflict-retry-exhaustion
  proposal, so two failed rows don't synchronize their return.

Why it grows with attempt count: the flat form was sized for conflict-retry
exhaustion, where the thing being waited out is *other runs finishing* —
run-duration multiples are exactly right, and attempt 1 of the exponential
is identical to the flat proposal, so that case is unchanged. But for the
other big transient class — API outages — the wait target is the outage's
duration, which has nothing to do with run duration and routinely lasts an
hour or more. With runs measured in minutes, flat 2–4× re-attempts every
~5–15 minutes; a one-hour outage burns the whole retry budget and
dead-letters perfectly good rows a human then re-queues in bulk. Doubling
per failure stretches the same budget across hours (≈5m, 10m, 20m, 40m, 80m
for five attempts) at zero extra mechanism; the dead-letter threshold
bounds the exponent, so no cap is needed.

### The dead mark — `dead_at` on inbox

**Locked: a nullable epoch-ms `dead_at` column on `inbox`.** No separate
table; the row never moves. Pending becomes
`integrated_by = '' AND dead_at IS NULL AND (ineligible_until IS NULL OR
ineligible_until <= now)`. Re-queue = set `dead_at` back to NULL. Rationale:
it is the same shape as every other state in this design — `integrated_by`,
`ineligible_until`, and `dead_at` are each one nullable/empty-able column
meaning one independent fact about the row, and the pending predicate is
their conjunction. A separate dead-letter table would split the arrival's
identity across two tables and make re-queue a move instead of an UPDATE. An
explicit column beats a sentinel in `integrated_by` because "was integrated"
and "was given up on" are independent facts — a sentinel would conflate them
and break the provenance join (`integrated_by → runs.id`) with a non-run
value. The timestamp (rather than a boolean) is free and answers "when did
we give up" for the human deciding whether to re-queue.

Rider: **dead-lettering clears `ineligible_until` in the same UPDATE**
(set `dead_at`, null `ineligible_until`) — a dead row isn't waiting, it's
parked; each column keeps meaning exactly one thing.

### Retry counter scope — since re-queue, via `requeued_at`

**Locked: the policy queries count only attempts since the last re-queue.**
Re-queue is a human explicitly judging "try this again" (presumably after
fixing something). With all-history counting, a re-queued row is already
*at* the threshold, so its first new failure instantly re-deads it — and
the backoff exponent is maxed, so that one attempt comes at worst-case
delay. Re-queue would grant exactly one slow attempt; scoped counting makes
it mean what it says: a fresh retry budget. Audit loses nothing — every run
row persists; only the policy queries narrow their window.

Mechanism: a nullable epoch-ms **`requeued_at`** column on `inbox`, set by
the re-queue operation. Both policy queries (dead-letter threshold check
and backoff exponent) use the same scoped count:

```sql
COUNT(*) FROM runs WHERE caused_by = ?
  AND status IN ('failed','crashed')
  AND started_at > COALESCE(requeued_at, 0)
```

Rejected alternatives: deleting/retargeting old run rows (destroys the
locked one-row-per-attempt audit history and the `caused_by` provenance
chain); a resettable `attempts INTEGER` counter on inbox (workable but
denormalized — it must be correctly incremented at every failure site,
including the boot sweep's crashed-marking, or the two sources skew: three
write sites maintaining a cache versus one column written once by one
human verb). `requeued_at` keeps `runs` the single source of truth and
preserves a fact a reset would erase: *that a human intervened, and when*.
A row dead-lettered and re-queued twice is interesting precisely because
it doesn't look new.

### What counts toward the threshold — all failures, no exceptions

**Locked: every failed/crashed run counts toward the dead-letter threshold,
including conflict-retry exhaustion.** This overturns the recorded instinct
(no — transient by construction). Rationale:

- **The same uniformity argument as backoff.** Failure-type dispatch was
  rejected there because code cannot reliably classify failures; exempting
  the one type we pre-judged innocent is exactly the self-classification we
  said we wouldn't trust. "Transient by construction" really means
  "transient *assuming the storm passes*" — an equivalence argument, not a
  guarantee. A row that keeps conflict-retry-exhausting across returns has
  empirically falsified that assumption (a hot-page pathology, a bug making
  every commit collide), and the threshold is precisely the instrument that
  detects falsified transience.
- **An exempt type reopens the hole the frame closes.** Each re-run
  re-pays extract + resolve + merge; a failure type that never counts
  retries forever at full pipeline cost — unbounded LLM spend with no human
  signal, the silent budget burn dead-letter was locked to stop.
- **The burst scenario the instinct protected is already defused by the
  exponential backoff.** Reaching a threshold of ~5 via conflicts alone
  means ~15 collisions spread across hours of doubling delays; bursts don't
  last hours, and bad luck doesn't survive five exponentially-spaced
  returns. Dead-letter is not a verdict of "poison" — it is "automatic
  retries stopped, human attention requested," which fits.

The error string still names conflict-retry exhaustion, so the human
reading the dead row knows what kind of struggle it was.

### Where the check lives — at failure time, by whatever marks the run

**Locked: the code that records a failure applies the policy in the same
transaction.** When a run is marked `failed`, that same code computes the
since-re-queue failure count; at the threshold it sets `dead_at`, otherwise
it sets `ineligible_until` by the formula. The boot sweep does the identical
thing when it marks orphaned `running` rows `crashed` — a crash is just a
failure whose policy step was deferred to boot. Rationale:

- **Failure is the only moment the inputs change.** The count moves only
  when a run fails, so that is the one honest decision point. A
  selection-time check would re-run the count query on every wake for every
  pending row — paying repeatedly to re-derive a fact knowable once.
- **The row's state stays coherent.** With a selection-time check, a row
  past the threshold sits "pending" in every query until a worker
  happens to select it — the dead-letter view, alerting, and the status
  verb would all lie in the gap. Failure-time marking means `dead_at` is
  true the moment it becomes true.
- **Selection stays policy-free.** The worker loop remains pure mechanism —
  the in-flight set, ordering, the pending predicate. Retry policy lives in one
  place (the failure path), not smeared into selection.
- **Notification (since settled — see "Eventplane producer") gets a
  natural home for free**: dead-lettering notifies via the
  `wiki.row_dead_lettered` outbox event, fired from the same spot,
  atomically with the mark.

### The threshold — `WIKI_RUN_ATTEMPTS_MAX`, default 5

**Locked: config, default 5.** When the since-re-queue failed/crashed count
for a causing row reaches the value, the failure path sets `dead_at`
instead of a backoff — a row gets at most 5 full pipeline attempts per
human authorization. Config-not-code follows the established pattern
(`WIKI_INTEGRATION_WORKERS`, `WIKI_INBOX_INLINE_MAX`): a spend/patience
dial shouldn't need a deploy. The default is chosen against the locked
backoff, not in isolation: with minutes-long runs, five
exponentially-spaced attempts span roughly 2.5 hours of wall clock — rides
out the common transient class (API outages, conflict storms) while
capping poison input at five paid pipeline runs. Three would dead-letter
through an ordinary hour-long outage; ten buys almost no extra outage
coverage (attempts 6–10 land days out) while doubling the poison spend.
The name says "attempts" because that is what `runs` rows are — one row
per attempt is already the locked vocabulary.

### Batch failures — a failed run blocks the cron row's stamp

**Locked: the cron row's all-succeeded join stamps it only when all bound
batch-integrator runs have *succeeded*, not merely completed — and the blocked
stamp is itself the retry mechanism, uniform with documents.** A batch
run's causing row (`runs.caused_by`) is the cron row, so the locked policy
applies to it with zero new machinery: a failure sets `ineligible_until`
on the cron row (the timer wake re-selects the batch run within the
cycle), `WIKI_RUN_ATTEMPTS_MAX` failures set its `dead_at`. Rationale:

- **Releasing the stamp orphans the retry.** A pending cron row is the
  locked "durable batch authorization" — the only thing that makes a
  worker select batch work. Stamp it after a failure and nothing
  re-runs the work: the unswept event rows sit pending until the *next*
  scheduled cycle, so a transient API blip silently costs a whole day's
  cadence. Blocking lets the existing backoff machinery retry within the
  cycle, which is what it was built for.
- **Partial success is already handled.** Two entries on one trigger, A
  succeeds, B fails → the cron row stays pending; the re-fire re-runs
  both, but A's selector is drained so A is a cheap empty run — exactly
  the re-fire idempotency the stamping section already paid for. Only B
  does real work.
- **Uniformity.** "Failure leaves the causing row pending; the policy
  delays and eventually dead-letters it" is now one sentence true of every
  integrator. The cron-row stamp rule (the worker-local all-succeeded join)
  changes one word: all bound runs *succeeded*.

### Batch dead-letter granularity — the cron row only, never event rows

**Locked: when a batch run exhausts its attempts, what dead-letters is
exactly the causing row — the cron row. Event rows are never automatically
marked; there is no whole-batch dead-letter and no poison-row hunting.**
This is what the locks already imply (the policy acts on `runs.caused_by`);
the decision is that no extra machinery is added on top:

- **Whole-batch dead-lettering parks innocents wholesale.** One bad row
  among 200 would mark 199 good events "given up on," breaking the ingest
  contract for all of them — and the batch is not a causing row, so
  marking it would be a second, parallel dead-letter semantics.
- **Hunting (bisecting the batch through repeated compile runs) is a
  complexity cliff aimed at the wrong failure.** Compile is one tool-less
  structured call over text payloads — a single event row rarely *can*
  kill it the way poison input kills a parser. Realistic batch failures
  are call-shaped: API outage, context overflow from a huge pile,
  schema-validation flakes. Bisection would orchestrate multiple LLM runs
  with partial stamping to hunt something that usually isn't there.
- **The escape hatch already exists for free.** `dead_at` lives on every
  inbox row and the pending predicate (which selectors use) excludes dead
  rows. A true poison event row surfaces as *recurring* cycle deaths —
  loud via the dead cron rows — and the human fix is to manually set
  `dead_at` on the offending event row(s), identified from the error
  string and run history. No new mechanism; the next scheduled cycle
  sweeps the survivors automatically.

Accepted cost, stated honestly: a true poison event row costs
`WIKI_RUN_ATTEMPTS_MAX` attempts per cycle until a human intervenes —
bounded per cycle and loud; notification is what makes "until a human
intervenes" short.

### Notification — dead-lettering notifies; attempts don't

**Locked (policy): dead-lettering notifies; individual failed attempts do
not.** Intermediate failures are self-healing by design — backoff and
retry need no human, and pushing on every transient failure is alarm
fatigue that trains the owner to ignore the channel. Dead-lettering is
definitionally "automatic handling has stopped, human attention
requested" — the one moment the system asks for a human, so the one moment
that earns a push. The locked check location gives it a clean home: the
same failure-path code that sets `dead_at` emits the notification.

**Mechanism — settled (see "Eventplane producer"):** the wiki publishes
`wiki.row_dead_lettered` to its outbox and the **notify** service — already
the suite's event-plane push consumer — subscribes. No direct push
integration in the wiki. A published event also leaves room to tie other
consumers to it later — e.g. a hosted script triggered on
`wiki.row_dead_lettered`.

## Eventplane producer — yes, with exactly two events (locked)

**The wiki is an eventplane producer via the standard appkit
outbox/`/feed`.** Producer status was implied, not really open: two locked
decisions require push notifications (dead-lettering notifies; oversized
refusal at a non-interactive door notifies), and the suite has exactly one
sanctioned way to notify — publish a fact, let notify consume it. A
bespoke side-channel would be the suite's first private API chain, the
architecture's named anti-pattern. The chassis already provides `/feed`,
so producing costs near-zero machinery.

- **Initial event set, exactly two:**
  - `wiki.row_dead_lettered` — payload: inbox id, source, title, last
    error. Emitted by the same failure-path code that sets `dead_at`,
    outbox row in that transaction.
  - `wiki.ingest_refused` — payload: door, source, size, cap. Refusal is
    pre-accept, so this is a plain outbox write at the door.
  Both consumed by notify; a hosted script can bind later for free.
- **Nothing else is emitted.** No `wiki.page_updated`, no
  `wiki.subject_created`, no per-run completion events — nobody consumes
  them today; "wiki activity as a feed" is the speculative-structure
  shape that keeps dying in this design. The escalation path is cheap and
  known: a new event is one outbox write wherever the fact occurs, added
  when a real consumer exists.
- Payload shapes finalize in schema finals.

## Lint

### The frame — a family of named jobs

**Locked: lint is a family of named jobs, not one monolithic curator sweep.**
The chores the design routes at lint — dup-queue consumption (merging flagged
subject pairs), the semantic duplicate sweep, staleness repair on non-manifest
pages, page growth/split, citation-preservation checking, unresolved-relative-
time cleanup — differ wildly in shape: some are mechanical checks with no LLM
(citation preservation), some are one targeted LLM judgment (merge a flagged
pair), some are open-ended sweeps over the whole page store (semantic dups,
staleness). Rationale:

- The design's method is matching mechanism to hazard. A monolithic "lint
  pass" would bundle a free mechanical check, a cheap targeted judgment, and
  an expensive whole-wiki LLM sweep behind one trigger and one failure story —
  exactly the coupling rejected everywhere else.
- The locked failure policy acts on *causing rows*. A dup-merge job has a
  natural causing row; a whole-wiki sweep doesn't. One frame can't serve both;
  per-job design can.
- Walk order falls out: settle the family's shared machinery (how lint jobs
  run, where their runs live) once, then walk the jobs one by one.

### Execution — lint rides the spine, triggers are rows from any door

**Locked (re-confirmed after the jobs were walked: every job's recovery,
concurrency, and failure story came from machinery already paid for; the
only adaptation needed was selector-less config entries).** Lint jobs
run on the existing spine: self-selected by the workers, executing on the
worker pool, one `runs` row per attempt, failure policy applied to the
causing row verbatim. Jobs bind to **trigger names**; a trigger is a **row**,
regardless of door:

- The cron consumer produces a trigger row on schedule (`source:
  cron:<name>`) — unchanged.
- A manual MCP verb (e.g. `lint_run(job)`) is just another thin front door:
  it `Accept`s a trigger row (`source: mcp:<caller>`) and returns the usual
  receipt. Selection keys on which trigger the row authorizes, never on
  which door delivered it. Lint is therefore cron-triggered *and*
  on-demand — not exclusively scheduled.

Rationale: durable authorization (a manual trigger survives crash/deploy as
a pending row, same as a cron tick); same failure policy and status poll
(the trigger row is the causing row; poll its inbox id); the at-most-one-
in-flight rule already handles manual-during-scheduled overlap (the second
row waits, then runs cheap/empty if the work is drained); and it
generalizes for free — the same verb shape can fire a digest early
("run crm-digest now"). Keeps the invariants: every front door ends at
`Accept`; the workers' only wake signal is the contentless nudge.

The config and vocabulary questions this raised are settled below ("Config
and vocabulary — one `jobs` config").

### Job: dup-queue consumption — judge first, three outcomes, version gate

**Locked: the flag is evidence, never a verdict — the dup-merge job's first
step is a dedicated LLM identity judgment on the pair.** The flag writers are
deliberately low-precision byproducts (match's "doubt is no_match" polarity
intentionally under-merges at integration time and defers the careful call to
lint; if lint merged on the flag alone, the careful judgment would happen
nowhere). The lint judge is strictly better-informed than any flagger: both
*full* pages and complete registry alias lists, vs. match's mid-run 600-char
excerpts. Subject merge is irreversible surgery; it gets the best evidence
available.

**Locked: three outcomes — `merge` / `dismiss` / can't-tell-yet (row stays
`open`), with a page-version re-judge gate.**

- **merge** — same thing; perform the merge (mechanics walked separately),
  row → `merged`.
- **dismiss** — definitely different things; row → `dismissed`, permanent:
  the persisted row blocks the pair from ever being re-flagged (already
  locked). Dismiss means *judged different*, never "not sure" — two meanings
  must not share one status.
- **can't-tell-yet** — status untouched (`open`); the only write is stamping
  the page versions the judge examined: `judged_version_a/_b` columns on
  `dup_flags` (NULL = never judged). The work-list query skips open rows
  where neither page's `version` (the optimistic-commit column) has advanced
  past the stamp — re-judging identical bytes is paying twice for the same
  answer. Any later write to either page bumps its version and the pair
  re-enters the work list automatically. No timers, no events: two integers
  and a WHERE clause.

```sql
SELECT f.* FROM dup_flags f
JOIN pages pa ON pa.subject = f.subject_a
JOIN pages pb ON pb.subject = f.subject_b
WHERE f.status = 'open'
  AND (f.judged_version_a IS NULL
       OR pa.version > f.judged_version_a
       OR pb.version > f.judged_version_b)
```

Rejected alternative: binary merge/dismiss with re-flagging as the reopen
path. Fails twice: (1) referencing a subject does **not** re-flag the pair —
the flag writers fire only on single-document alias bridging or shared
candidate shortlists; ordinary page growth flows through clean one-id
lookups that never examine the pair, so the decisive evidence ("Bob Smith
works at Acme") arrives silently and the question never reopens. (2)
Doubt-as-dismiss either buries real dups forever or forces weakening
dismissed-blocks-reflag, losing the stop-asking guarantee for genuinely
different pairs. The version gate is the user's instinct mechanized — "park
it until more information arrives on one of the subjects" — with the page
version as the arrival detector, the only signal firing on *every* way
information can arrive. This is the write-license rule applied to judgment:
new evidence is the only license to re-judge.

Schema rider for schema finals: `judged_version_a INTEGER`,
`judged_version_b INTEGER` (nullable) on `dup_flags`.

### dup_flags pair storage — canonical order + UNIQUE (locked)

**Locked: every pair is stored in one canonical order — `subject_a` is
always the smaller ULID (`subject_a < subject_b`).** Otherwise
(`01AAA`,`01BBB`) and (`01BBB`,`01AAA`) are different column values, the
UNIQUE never fires, and one real-world question becomes two rows with
independently drifting statuses. Three pieces make the rule unbreakable:

1. One helper — `FlagDup(x, y)` — is the only code that inserts; it sorts
   the two ids, then `INSERT … ON CONFLICT DO NOTHING`.
2. `UNIQUE(subject_a, subject_b)` — the second notice bounces off the
   first row.
3. `CHECK (subject_a < subject_b)` — a mis-ordered direct insert crashes
   instead of silently minting a duplicate row.

The aliases-table pattern again: normalize at the boundary, let UNIQUE
absorb repeats, one helper is the only code that knows the rule. Side
effect (pleasant, not load-bearing): ULIDs sort by mint time, so
`subject_a` is always the older subject — the mechanical merge winner.

### Job: subject merge — mechanics

**Locked: the surviving subject is chosen mechanically — the older ULID
wins, always. The judge picks the canonical *name*, never the surviving
*id*.** A subject id is an opaque address; nothing user-visible rides on
which survives, and where judgment adds nothing the design prefers
determinism. The older subject has existed longer, so id references
(`created_by_run`, other dup_flags rows, anything future) skew toward it —
older-wins minimizes repointing beyond the aliases that must move anyway.
The real judgment nearby is a different one: which name becomes
`canonical_name` on the survivor ("Robert Smith" vs "Bob Smith") and the
merged page's title — prose-level, the judge's call, independent of id
survival. Mechanical winner keeps merge mechanics golden-testable: given a
verdict, the database effect is deterministic except the folded prose.

**Locked: the loser is hard-deleted; the `dup_flags` merged row is the
audit record.** One transaction, completely or not at all:

1. Repoint the loser's alias rows → winner.
2. Rewrite any *open* `dup_flags` rows referencing the loser (re-normalize
   the pair; the UNIQUE absorbs collisions — a duplicate pair row is
   dropped). Closed rows keep their original ids — they are history, and
   stale ids in history are correct history.
3. Fold the loser's page prose into the winner's page (citation
   preservation applies — every citation from both bodies survives the
   fold).
4. Delete the loser's page row and the loser's `subjects` row.
5. Set the winner's `canonical_name` per the judge's pick; mark the dup
   row `merged`.

Rationale: only three things point into `subjects` — `aliases.subject_id`,
`pages.subject`, `dup_flags.subject_a/b` — and the transaction repoints or
deletes all three, so no live code path can reach the loser afterward and
**no reader anywhere checks for repointed subjects** (code landing on a
stale id is a bug to crash on, not a case to handle). A `merged_into`
tombstone was considered and rejected as redundant: every merge goes
through a dup row (resolution never merges inline), so "where did 01BBB
go?" is already a one-query answer against `dup_flags`
(`status='merged'`, winner = the id still in `subjects`, `run_id` for
free). After commit the loser id appears nowhere except immutable history
(closed dup rows, run records) — correct. No schema rider.

**Locked: run shape — one run per trigger sweeping all eligible pairs
serially, with one database transaction per pair (never one for the whole
run).** Rationale: one-run-per-trigger is the digest's existing shape, and
per-pair runs would force selection to read `dup_flags` — lint
internals leaking into the policy-free selection component. Per-pair transactions
put "completely or not at all" at the right grain: each merge commits
alone (judge → fold → the five steps + close the dup row); if pair 7 of 10
fails, pairs 1–6 stay merged — each was a complete operation. Recovery is
the queue itself: the failed run triggers standard backoff on the trigger
row; the re-run's work-list query no longer sees closed pairs and resumes
where it stopped — no checkpoint bookkeeping. Optimistic commit applies
per pair: each merge transaction version-checks both pages it read; a
conflict re-folds that one pair (same loop as everywhere, counted in
`runs.conflicts`).

**Locked: judge and fold are two separate tool-less calls.** The judge is
an identity decision — small, cheap, match-shaped (`merge` / `dismiss` /
can't-tell). The fold runs only on a merge verdict: both page bodies in,
one merged body out. Rationale: different work in different shapes — a
combined call forces one model/prompt to do judgment and prose
composition, and pays the fold's output budget on every verdict though
most need no fold; separate calls slot into the per-call-site model
choice. Tool-less rather than an agent because the write set is known
before the call starts (exactly two inputs, one output — nothing to
navigate); golden-testable, and cannot wander into unlicensed reads or
writes. The fold's prompt inherits the merge prompt's craft obligations:
lead discipline (identity-establishing first paragraph — the match
obligation), contradictions corralled not interleaved, and every citation
from *both* bodies survives or its statement was deliberately superseded.

### Job: semantic duplicate sweep — its own job, flag-only

**Locked: the sweep exists as its own named lint job, and it is a
flag-only producer — it inserts `dup_flags` rows via `FlagDup` and never
judges or merges anything itself.** Why it must exist: the two integration-
time writers only notice a pair when the subjects co-occur in one
document's resolution (alias bridging, shared candidate shortlist); two
duplicates built from disjoint streams (Bob from emails, Robert from CRM
events) are never co-examined and would coexist forever unflagged. The
sweep is the proactive walker. Rationale for flag-only:

- Everything downstream already exists — judge, three outcomes, version
  gate, merge surgery. The sweep needs zero consumption machinery.
- The pair UNIQUE makes it idempotent and polite: open/merged/dismissed
  pairs bounce off — the sweep can run forever and never resurrect a
  settled question (dismissed-rows-persist paying off).
- Separate from the dup-consumption job because the cost shapes differ:
  consuming is cheap and frequent; sweeping is a wide scan with a rare
  natural cadence (weekly/monthly cron, or the manual trigger — "I just
  imported 500 CRM contacts, go look now").
- Uniformity: everything that suspects a dup — resolution, match, sweep,
  a future human verb — speaks one sentence (`FlagDup`); exactly one
  judge ever decides.

**Locked: the sweep is fully mechanical — zero LLM calls.** For each
subject, run the same two candidate FTS queries resolution already uses
(name + alias tokens vs. other registry names; page lead vs. other page
bodies, same type only); any pair scoring above a flag threshold →
`FlagDup`. The entire subject list never goes near a model — N FTS
queries, and the only paid consequence is downstream judge calls on
flagged pairs. Rationale:

- The candidates step's division of labor, reused: "loose recall is fine;
  precision is the judge's job" is locked for resolution; the sweep is the
  same recall problem over the same corpus, and the dup judge is the same
  precision filter. An LLM screen inside the sweep would be a second,
  redundant judge.
- Over-flagging is a bounded one-time cost: a wrongly-flagged pair is
  judged once, dismissed, and the UNIQUE blocks it forever. Under-flagging
  is the bad direction (silent permanent dups) — the same polarity already
  recorded for candidate thresholding.
- Full scan every run, to start: re-flagging settled pairs bounces off the
  UNIQUE, so a full scan is harmless FTS work. If N ever makes it matter,
  gating by pages-changed-since-last-sweep is a contained later
  optimization (the version-gate trick again).
- The flag threshold is a tuning knob for the eval harness goldens, not
  designed in the abstract.

### Citation preservation — a commit-time gate, not a lint job

**Locked: citation-preservation checking is struck from lint's chore list.
It is a commit-time gate on every page write, with the writer declaring
its drops.** Every LLM call that rewrites a page (document-pass merge, the
fold) must also output a `superseded` list — the citation ids it
deliberately dropped, one reason line each. At commit, pure set
arithmetic: `old citations − new citations` must exactly equal the
declared list. Undeclared loss = the model paraphrased away evidence = a
failed call → retried in-run, never committed. Rationale:

- A lint audit cannot work, because the invariant has a judgment clause —
  "survives *or was deliberately superseded*." Weeks later the
  deliberateness no longer exists anywhere; the only moment it is knowable
  is when the writer can declare it. Audit-later inspects a fact that has
  evaporated.
- Validate at boundaries: a lost citation caught at commit costs one
  retried call; caught by lint a month and five rewrites later, repair is
  archaeology.
- Nearly free: two regex scans and a set difference per page write — no
  run, no trigger, no LLM, no queue.
- Side benefit: the `superseded` declarations are the audit trail for
  "why did this citation disappear?" — answerable from the run record
  instead of by diffing page history.

### Staleness — the `stale_notes` side channel (locked)

**Locked: the note-for-lint side channel exists, as a `stale_notes` table
mirroring the dup_flags pattern: flag-only writers during other work, a
dedicated lint job that fixes.**

```sql
stale_notes(
  subject,   -- whose page looks stale
  note,      -- what the writer observed, one or two sentences
  cites,     -- inbox id(s) of the new evidence that exposed it
  run_id,    -- who noticed
  status     -- open | repaired | dismissed
)
```

Recording side: the merge agent or fold, mid-run, notices a *read-only
neighbor* page contradicting what it just wrote; its write license
doesn't cover that page, so it appends a note row in its normal end-of-run
commit — a few bytes in a transaction already happening, no extra LLM
call. One more output field on existing calls, like match's dup side
channel. Writers so far: document-pass merge (neighbor reads), the fold.

Fixing side: a third lint job (alongside dup-consumption and the sweep);
work list = open notes; per note read the page + cited evidence, decide
stale-or-not, repair with the carried citations, mark `repaired` /
`dismissed`. Made almost entirely of locked parts: the dup job's run
shape (one run per trigger, per-note transactions), the spine's failure
policy, and repair-as-page-write (optimistic commit + citation gate apply
automatically). New surface: one table, one prompt, one job-config entry.

Key points: the `cites` column is what makes the repair *legal* — "new
evidence is the only license to write," and the note carries its evidence,
so the repair applies specific cited claims, never a hunch. A sibling
table, not a generalized `lint_queue` with a `kind` column: dup rows are
pair-shaped with pair machinery (canonical order, UNIQUE,
judged-versions); notes are subject+note-shaped — same pattern, separate
tables, no nullable-column/JSON-blob genericism. Rejected alternatives:
inline fixing (write-license violation — the unlicensed-improvement door
the design slammed shut) and letting observations evaporate (silent rot
until a far more expensive whole-wiki staleness sweep).

**Locked: the repair call is tool-less, one call per *subject*, batching
all of that subject's open notes** — page body + notes + cited payloads
(via `ReadPayload`) in; rewritten page + per-note disposition
(`repaired` / `dismissed`) out, all applied in one transaction. Tool-less
for the fold's reason: the write set is exactly one page, known before
the call starts — golden-testable, can't wander; the prompt inherits the
craft obligations (lead discipline, contradictions corralled,
`superseded` declarations for the citation gate). Per-subject because two
notes about one page must land in one rewrite — separate processing
means needless conflict pressure and two sequential rewrites where one
suffices; grouping gives one read, one judgment over all evidence, one
write, each note marked in that transaction. This also absorbs duplicate
observations: `stale_notes` has no UNIQUE (notes aren't canonical pairs —
two runs can legitimately notice the same staleness with different
evidence), and per-subject batching just merges duplicates into one
repair.

Schema rider for schema finals: the `stale_notes` table.

### Page growth — no split, ever; compression is the policy

**Locked: one subject = one page is an invariant; there is no split
policy, and no condense job or size threshold is built today.** One
subject/one page is load-bearing for addressing everywhere: claims land
on *the* page, match excerpts *the* lead, the fold folds pages singular —
a split breaks all three with no answer to "which half?". Growth is
managed by what already exists: merge's compression-at-write mandate
(woven prose, corroboration as added citations, superseded detail
declared — never an appended ledger). A page too big is either
under-compressed (fix: the merge prompt compresses harder) or genuinely a
subject with enormous current knowledge — which splitting doesn't reduce,
only mis-addresses. If condensation beyond per-write compression is ever
needed, the shape is obvious and contained — a lint *condense* job
(size-flagged mechanically, rewrite tighter, citation gate enforcing
nothing lost undeclared), same pattern as every lint job — but building
it now is speculative structure. The deciding evidence stays one query
away (`SELECT subject, LENGTH(body) FROM pages ORDER BY 2 DESC`).
Resolves the open item ("split policy, or lint's problem?"): neither,
yet — it's the merge prompt's problem.

### Config and vocabulary — one `jobs` config, "integrator" narrows

**Locked: the batch table generalizes into one `jobs` config list**
(config, not a database table — the name/trigger/selector binding that
selection reads): `name`, `trigger`, and `select` legal only for
digest-kind entries. Lint entries bind name → trigger with no selector
(their inputs are `dup_flags` / `stale_notes` / the registry, never inbox
rows).

```
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

Rationale: selection treats both kinds identically — pending trigger
row → look up bound jobs → run them, at-most-one-in-flight per name,
stamp when all succeeded; two config lists would give one mechanism two
parallel registries. The startup checks (selector partition/coverage)
apply to the subset with selectors. Manual triggering needs no extra
config — the MCP verb fires any named trigger.

**Locked vocabulary:** a **job** is anything a worker can select and start.
**Integrators** (document pass, digests) are the subset that consume
inbox rows; **lint jobs** are the subset that maintain existing content.
A **run** is one execution of one job; `runs.integrator` renames to
`runs.job` (schema rider for schema finals).

### Unresolved relative time — struck from lint entirely

**Locked: lint does nothing about an unresolved "last Tuesday." No
resolve job, no detect-and-report job.** Extract's keep-it-relative
choice was deliberate polarity: a cheap visible failure over a
confidently wrong absolute date. Lint cannot improve on it — following
the statement's citation leads back to the same inbox row extract
already saw (same payload, same `received_at`): no new evidence, so
"resolving" would just override the safe answer with the guess extract
refused — the polarity silently inverted at the back door. The genuine
fix flows through existing machinery: a later document that establishes
the date is a new claim with new evidence, and normal merge supersedes or
corroborates. Even detect-and-report fails the "then what?" test — a list
of relative phrases with no evidence to resolve them is noise. An
unresolved relative is honest, cited imprecision; it stays until real
evidence supersedes it.

## Search / ask — the read side

### The frame — hosted ask first

**Locked: the read surface is hosted-ask-first. The wiki runs the retrieval
loop server-side, as its own agent; the caller spends one tool call and gets
back a cited answer, never a navigation session.** Considered and rejected:
primitives-first (expose cheap zero-LLM tools — lookup/search/read — and let
the caller's agent iterate, the Claude Code / Zep-read-path shape current
research favors). Rationale:

- **The caller's context window is the scarce resource.** Turning navigation
  over to the caller burns caller context on intermediate search results and
  page reads. The caller can protect itself only by wrapping the wiki in a
  subagent — and if every caller must always do that, the subagent belongs in
  the service, done once, by us. Context isolation as a service.
- This is the design's standing principle applied to context instead of
  money: spend at write/server time so the reader doesn't pay (the same
  argument that bought compile-time digests and merge-time compression).
- The retrieval-quality research (iteration beats index sophistication)
  survives intact — the iteration just runs server-side, where the harness,
  prompt, and citation discipline are ours to control and golden-test.

Not decided by this lock: ask's internals (tools, model, sync/async,
whether it writes anything back), or the index strategy under it.

### The search verb — survives, demoted to side door

**Locked: a zero-LLM `search` verb stays on the surface alongside ask.**
Ask is the front door; search is the side door for exact-shaped needs.
Rationale:

- Ask's inner agent needs the retrieval primitives anyway (FTS search,
  registry/alias lookup, page read) — they exist as internal code
  regardless; exposing them as one MCP verb is nearly free.
- Some queries are exact-shaped, not question-shaped: "show me the Acme
  page" through an LLM loop pays seconds and tokens for what the alias
  table answers in one query. Known-item fetch returns a page the caller
  *wants* in context — the context-burn argument bites only on
  intermediate navigation, which only ask involves.

**Rider (the lock's stated risk): caller agents will reach for the cheap
verb when they should ask.** Mitigation is the tool descriptions — they
must steer hard: `search` described as exact/known-item fetch ("you know
the page exists and roughly its name"), `ask` as the default for any
actual question ("answers come back cited; do not assemble answers from
search results yourself"). Description text is part of the design surface
here, not an afterthought; it lands with the MCP verb specs.

### Ask is strictly read-only — synthesis pages die

**Locked: ask writes nothing. The old design's `synthesis/<slug>.md`
file-back (answers stored into the wiki and reindexed) is removed.**
Rationale — three already-locked principles decide it:

- **Write license.** An ask answer contains no new evidence — it is
  derived entirely from existing pages; "new evidence is the only license
  to write" rules it out directly.
- **Citation uniformity.** A stored synthesis cites *pages*, not inbox
  arrivals — the two-cased "wiki quoting itself" citation semantics
  explicitly rejected in the digest design.
- **Knowledge that rots with no repairer.** A synthesis is stale the
  moment any underlying page changes; no lint job, version gate, or
  citation gate covers it — a fourth content class with zero maintenance
  machinery. Pages are the compounding layer; re-asking over current
  pages is strictly fresher than a cached answer.

Payoff: a read-only ask needs **no flight lock, no optimistic commit, no
transaction** — it runs fully parallel with integration with zero
coordination (the old design's shared single-flight gate with ingest is
gone).

**The escape hatch is the front door:** an answer worth keeping is
produced as a document and ingested (`ingest_text`) — it becomes an inbox
arrival (citable, like any outside document) and integrates through the
full pipeline with every gate intact. Caller's choice, zero new
mechanism.

### Ask is synchronous — the call returns the answer

**Locked: `ask` blocks and returns the cited answer in the tool result.
The async job shape (return job_id, caller polls job_status) is removed
for ask.** Rationale:

- The poll loop burns exactly what hosted-ask exists to protect: every
  `job_status` round-trip is a tool call + result in the caller's
  context, and the caller must remember to poll. One call → one answer
  is the point.
- The async shape existed for queueing reasons now gone (ask shared the
  ingest single-flight lock; read-only ask contends with nothing). Job
  rows, status wiring, and TTLs for a read are complexity with no
  remaining justification.
- Agent callers tolerate long tool calls — normal MCP behavior. The
  bound is infrastructure (nginx proxy timeout, client patience); tens
  of seconds is well inside both.

Rider: the inner agent gets a server-side budget (max turns / max
tokens / wall-clock cap) so the call can't run unbounded. Accepted
caveat, stated honestly: if asks ever routinely exceed ~2 minutes, sync
gets brittle (proxy timeouts; client retries re-pay the whole spend) —
treated as a budget-tuning problem, not a reason to keep job plumbing.

### Ask's inner toolset — the public primitives + the citation hop

**Locked: six read tools, nothing else** (revised from four by the
cross-subject/temporal decision below — tools 5 and 6 added there, with
tool 6 goldens-gated).

1. `search(query)` — the same FTS the public search verb uses;
2. `lookup(name)` — registry alias → subject (exact identity
   resolution — the corpus's structural advantage over generic RAG);
3. `read_page(subject)` — full page body;
4. `read_source(inbox_id)` — follow a citation to the original
   arrival's payload (`ReadPayload`), size-capped (an arrival can be a
   whole PDF);
5. `timeline(from, to)` — zero-LLM registry query: event subjects with
   `occurred_at` in the window (see "Cross-subject and temporal-span
   queries");
6. `related(subject)` — neighbors by page-mention in the derived link
   graph; **goldens-gated, not built until the cross-subject goldens
   show bare ask failing** (same section).

Rationale:

- **Tools 1–3 are the public primitives, identical implementation** —
  one code path serves both surfaces; primitive goldens cover ask's
  tools for free. No bash; no glob (a file-tree idiom — registry + FTS
  are the navigation now).
- **Tool 4 is the designed payoff of citation discipline.** Pages are
  compressed; when pages disagree or the question needs exact
  wording/figures, raw truth is one hop away — that hop is what makes
  answers trustworthy rather than paraphrase-of-paraphrase.
- **Read-only by construction** — the tool list *is* the write-license
  enforcement.
- Rejected: a list-all-subjects/browse tool — FTS + lookup covers
  discovery; browsing invites whole-wiki trawls that blow the inner
  budget. (`timeline` is not that tool: it is bounded by an explicit
  date window and returns only event subjects — a range query, not a
  catalog walk.)

### Cross-subject and temporal-span queries — registry views as ask tools (locked)

**The weakness, named:** the prior-art study
(`docs/wiki-prior-art-research.md`) identified cross-subject thematic
questions ("what are the themes") and temporal-span questions ("what
changed between March and May") as the query classes per-subject prose
pages answer worst — they are why GraphRAG built stored community
summaries, HippoRAG built per-query PageRank, and MIRIX built an
episodic store. Our answer follows the design's own locked pattern
(journal/index rejections): **views rendered on demand over the
registry, never stored derived knowledge.**

**Locked: `timeline(from, to)` — built now, designed into schema
finals.** A zero-LLM registry query: event subjects whose `occurred_at`
falls in the window (name, kind, `occurred_at`; ISO-8601 prefix args,
lexicographic range — the precision-prefix lock makes this correct for
free). It is the "show me my day is a view" promise made real as a
tool. Ask gains time-addressed recall to complement its name- and
relevance-addressed tools; the agent narrates the window the way it
narrates anything else — mechanical recall, agent precision. Dependency
already flagged for the eval harness: digest/compile must put enough
event-time into `occurred_at` and claims for window queries to have
substance.

**Locked: `related(subject)` + the derived link graph — goldens-gated.**
The mechanism: pages mention other subjects; the registry's normalize
machinery makes mentions mechanically resolvable (scan page bodies for
alias matches — zero LLM). The derived page-to-page mention graph
gives: `related(subject)` (neighbors — ask's substitute for graph-hop
association), on-demand clustering for "themes" questions (computed per
query, discarded after), and free zero-LLM lint signals (orphans,
fragile bridges) as a side effect. **Not built until the cross-subject
eval goldens show bare ask (the other five tools) failing** — value is
measured, not asserted, per the standing rule. The graph is always
derived and rebuildable from pages + registry; never the
representation, so nothing to keep current and nothing to rot.

**Rejected: stored communities / cached thematic summaries** (the
GraphRAG shape). Derived prose that rots with no repairer — the exact
shape killed three times already (synthesis pages, index.md, the
journal page). If the link graph proves insufficient, the remaining
honest option is raising ask's budget for these question shapes, not
caching derived knowledge.

**Public surface: `timeline` is exposed as a public MCP verb; `related`
stays internal.** The argument that kept search alive as a side door
applies verbatim to timeline — the primitive exists anyway, and "list
the events of March" from a caller that wants the list is exact-shaped;
an LLM loop would be pure overhead. `related` is held back: its public
use case is a step in answering a question (ask's job), not a terminal
answer, and every exposed primitive worsens the locked side-door risk
(callers assembling answers themselves instead of asking). Adding it
later is trivial since the implementation exists either way. The read
surface is therefore **ask + search + timeline**.

**Rider — the steering obligation extends to `timeline`.** Its tool
description must frame it as "list event subjects in a date window,"
never "answer questions about a period," or callers will use it to
dodge ask — same description-as-design-surface rule as search.

**Rider — eval harness:** golden questions of exactly these two shapes
(multi-hop cross-subject association; date-range narrative) are
first-class deliverables; the cross-subject goldens are the gate on
`related`.

### The answer contract — page-level citations

**Locked: ask's answer cites pages (subject id + title); inbox/arrival
ids appear only when the inner agent pulled the original via
`read_source` and drew on it directly.** Shape: answer text with inline
page references + a structured `sources` list (subject id, title, any
arrival ids used directly). Rationale:

- **A citation must be followable by its reader.** The caller can fetch
  a cited page via the public search verb; an inbox id is caller-opaque
  (no public payload verb — and the surface is not widened just to make
  answer-citations resolvable).
- **The chain reaches bedrock in one hop.** Answer cites page → caller
  fetches page → the page's statements carry their inbox citations.
  Page-level citation in answers is honest *because* page-level
  citation discipline is enforced underneath.
- The citation-uniformity lock ("every citation points at an arrival")
  governs *stored pages*; the answer is ephemeral output, not stored
  (locked above) — no violation.
- The arrival-id exception is honesty, not symmetry: a direct quote
  attributed only to a page would credit prose that may paraphrase it.

### Index strategy — hybrid by design; FTS5 first in build order only

**Locked: the index is hybrid — FTS5 (BM25) + an embedding lane. The
embedding lane is a committed component designed now, not an
evidence-gated option; FTS5-first is build sequencing, nothing more.**
(Overrides the recommendation to defer embeddings behind eval-harness
evidence — the user holds embeddings non-optional.) What the lexical
side already covers, for the record: the aliases table gives exact
semantic resolution for every judged name (entities, events, concepts
alike), and hosted ask's iteration covers much phrasing variance. The
embedding lane covers the residual: claim-prose phrasing variance and
question-shaped queries that share no tokens with any page. Design of
the lane (unit, write-path mechanics, storage, provider, fusion,
reindex story) follows below.

### The hybrid retriever — one primitive, three call sites, wired by measurement

**Locked: one hybrid retriever primitive with two lanes (BM25, vector),
rank-fused, serving all three mechanical recall sites — the search
verb / ask's search tool, resolution's candidates step, and lint-sweep.
The vector lane is independently switchable per call site, and each
site turns it on only when measurement shows the lift.** Rationale:

- All three sites are the same recall problem feeding a downstream
  precision filter (ask's agent, the match judge, the dup judge) — the
  locked division of labor absorbs embedding false-positives at
  machinery already paid for. Candidates/sweep are where the lexical
  blind spot does the most damage (a missed candidate silently mints a
  permanent duplicate subject; the motivating example for the second
  FTS query — AWS / "Amazon Cloud" — is exactly the zero-token-overlap
  case BM25 cannot see).
- **But value is asserted nowhere — it is measured.** "We're making
  assumptions about its value; until we can measure it in our system we
  don't know it" (the same rule as the sweep threshold: a tuning knob
  for the eval harness, not designed in the abstract).
- **The eval harness gains retrieval goldens as a first-class
  deliverable**: per call-site query shapes (question-shaped searches;
  candidate resolution with synonym/zero-overlap cases; sweep
  pair-discovery over a seeded corpus), scored recall@k, lexical-only
  vs hybrid side by side. The same goldens expose the cost side
  (shortlist bloat feeding paid judge calls).
- Committed work now: the lane itself (unit, write path, storage,
  provider, fusion) + the measurement rig. Wiring decisions follow
  numbers.

### Embedding unit — one vector per page

**Locked: one vector per page; the subject is the unit. Embedded text =
canonical name + alias list + page body (truncated at the model's input
limit).** Rationale:

- **The chunking rationale doesn't apply**: chunks approximate the
  coherent fragment inside rambling raw documents; our pages are
  already the compressed, curated artifact — a page *is* what a chunk
  tries to be. (Personal scale, thousands of pages, reinforces this.)
- **One subject = one page = one vector** keeps the lane
  subject-addressed like everything else: candidates and sweep compare
  subjects (subject-grain serves them natively); search returns pages
  (hits are exactly the unit the caller receives). No
  fragment-to-page mapping layer.
- Prepending name + aliases bakes the registry's accumulated identity
  judgments into the vector — the embedding inherits every synonym
  ever judged.
- Write path stays trivial: one embedding call per page write, one row
  per page.
- Contained upgrade if retrieval goldens show long-page dilution:
  paragraph-grain vectors *in addition*, rolled up to page identity —
  schema addition, not redesign; not built until measured.

### Embedding write path — asynchronous catch-up, derived from state

**Locked: vectors are computed by an asynchronous catch-up worker, never
inside the integration commit.** The vector row records the
`page_version` it embedded; the work list is a query (vector missing, or
behind its page's current `version`); a small in-process embedder wakes
on run completions (the nudge pattern), sweeps stale vectors, batches
the API calls. Rationale:

- The design's own pattern verbatim: **signals decide when to look, the
  table decides what exists** — `embedded_version < pages.version` is
  `integrated_by = ''` again. No queue bookkeeping; crash-proof and
  idempotent for free.
- Vector staleness is benign and mechanically detectable: worst case is
  marginally weaker fuzzy recall for seconds, with the lexical lane
  transactionally current (a page is findable the instant it commits).
  Nothing like a lost citation; doesn't earn atomicity.
- Keeps the embedding provider out of the integration failure story
  entirely: a provider outage pauses catch-up; runs never see it.
  Synchronous embedding would put a second external API on every run's
  critical path and burn the retry/dead-letter budget on healthy
  content.
- **Read-side degradation rider:** query-side embedding at read time
  still needs the provider; when the embed call fails, the hybrid
  retriever falls back to lexical-only — for candidates that is
  exactly the recall level the design had before the lane existed,
  within the locked polarity.
- Not a spine job: no causing row, no LLM, no failure policy — "stay
  stale, retry next wake" is the whole story. A goroutine, not
  machinery.

### Vector storage — plain table, brute-force scan, no extension

**Locked: vectors live in a plain table; queries are an exact
brute-force cosine scan in Go. No sqlite-vec, no ANN index.**

```sql
page_vectors(
  subject           TEXT PRIMARY KEY,   -- = pages.subject
  embedded_version  INTEGER NOT NULL,   -- pages.version at embed time
  model             TEXT NOT NULL,      -- provider/model id + dims
  vector            BLOB NOT NULL       -- float32 array
)
```

Rationale:

- **The pure-Go chassis rules out sqlite-vec**: the suite runs
  `modernc.org/sqlite` (CGo-free) and ships one static binary —
  sqlite-vec is a C extension fighting both, to buy ANN that
  thousands of pages cannot use (ANN exists for millions of vectors;
  a full scan over ~5k × 1024-dim float32 vectors is ~20MB of dot
  products, well under 10ms).
- Brute force is *exact* (no approximation recall loss), trivially
  testable, zero tuning surface.
- Contained upgrade if the corpus ever grows 100×: the scan hides
  behind the retriever primitive; swapping in an index touches no
  call site.

**Rider — stale vectors serve until replaced.** The read side never
compares `embedded_version` to `pages.version`; reads use whatever
vector exists. The version comparison drives exactly one thing: the
catch-up worker's work list. Rationale: filtering stale vectors out
of queries would make a page vanish from the vector lane on every
write — precisely when it just gained new information and is most
likely to be queried; a slightly-outdated similarity beats absence.
Old vectors are barely wrong anyway (pages change incrementally;
identity and bulk content dominate the embedding). And it keeps each
column meaning one thing: `embedded_version` is the catch-up cursor,
not a read-side validity flag. The only true absence is a brand-new
page before its first embed — seconds, covered by the transactionally
current lexical lane.

Schema rider for schema finals: the `page_vectors` table.

### Embedding provider — OpenAI

**Locked: OpenAI, `text-embedding-3-large` at 1024 dims, one model for
both documents and queries.** Config `WIKI_EMBED_MODEL` (default
`text-embedding-3-large`) + `WIKI_EMBED_DIMS` (default 1024); secret
`OPENAI_API_KEY` via `~/.secrets` → `.envrc`, presence-gated at the
composition root — absent key disables the lane (lexical-only), the
`ANTHROPIC_API_KEY` pattern. Rationale:

- **The deciding reason: the suite will add significant OpenAI support
  as a future agent engine** — provider alignment with where the suite
  is heading. (Voyage's retrieval specialization and shared embedding
  space were the alternative's draw; vendor-coherence inverts once
  OpenAI is in the stack anyway.)
- Cost is a non-factor at our scale on any provider: ~2M tokens/month
  steady state ≈ $0.26/month on 3-large; a full 5k-page reindex ≈
  $0.65. The 30× market price spread is irrelevant when the base is
  cents — provider choice is quality-and-operational, not cost.
- One model both sides because OpenAI models don't share an embedding
  space (no asymmetric doc/query trick); simpler config anyway.
- 3-large over 3-small: the quality leader is free at our volume; the
  retrieval goldens can arbitrate later — switching is a config change
  plus a re-embed the catch-up worker already knows how to do.

### Model change — a config change the catch-up worker absorbs

**Locked: changing the embedding model (or dims) is just a config
change; existing catch-up machinery handles the re-embed. The `model`
string encodes dims (`text-embedding-3-large@1024`) since a dims change
has identical consequences.** Three rules:

1. **Work list gains one clause**: vector missing, OR
   `embedded_version < pages.version`, OR `model != current`. The
   worker re-embeds the corpus in the background — no tooling, no
   migration, no operator verb.
2. **Model match is a validity condition on reads** (unlike version,
   which reads ignore): the vector lane uses only `model = current`
   rows. Cross-model cosine is not stale, it is *meaningless* — silent
   garbage similarity, the design's worst failure class; absence beats
   it.
3. **The transition window is accepted degradation**: vector lane
   covers a growing fraction while lexical covers everything — the
   same degradation class as the provider-outage fallback. At our
   scale the window is short (~5M tokens, ~$0.65, minutes-to-an-hour
   under rate limits); blue/green double-storage would be machinery
   without a hazard.

Pleasant property: first deploy and model change are the same code
path — boot with an empty or mismatched `page_vectors` table is just a
big work list.

### Fusion — RRF for ranked consumers; per-lane thresholds for sweep

**Locked: Reciprocal Rank Fusion (k=60, top ~50 per lane in) for the
ranked-shortlist consumers — search (cut to caller's limit) and
candidates (cut to shortlist size ~5). Sweep does not fuse: it flags
when *either* lane exceeds its own threshold.** Rationale:

- BM25 scores and cosine similarities are incomparable scales; naive
  score-mixing weights one lane arbitrarily. RRF operates on rank
  positions — no normalization, one conventional parameter (k=60,
  leave it). A doc both lanes like rises; a doc one lane loves still
  surfaces — the point of two lanes. Weighted fusion / learned
  rerankers are upgrades the goldens must license, not defaults.
- **Sweep is different in kind**: it thresholds evidence ("suspicious
  enough to flag?"), not ranks for a reader — and RRF ranks have no
  absolute meaning to threshold. Per-lane thresholds (existing FTS
  threshold + a cosine threshold), both eval-harness tuning knobs per
  the locked rule. Over-flagging stays the cheap direction: a wrong
  flag costs one judge call, then the pair UNIQUE blocks it forever.

### The search verb contract

**Locked:**

- **Input**: `query` (required), `limit` (default **3**, cap **10**).
- **Resolution order: registry first** — a query that exact-matches an
  alias (after normalization) pins that subject's page at rank 1; the
  hybrid retriever fills the remainder. The highest-precision signal
  the system owns; "show me Acme Corp" is deterministic, no ranking
  involved.
- **A hit is the whole page**: subject id, type, kind, title, full
  body, citations intact. The side door's purpose is delivering the
  wanted page into caller context — a snippet would force a second
  round-trip through a fetch verb we'd then have to invent. The small
  default limit is the context protection (3 full curated pages, vs
  the old 10).
- **Rank order only, no scores**: RRF scores are fusion artifacts;
  exposing numbers invites callers to build on meaningless values (as
  the old negated-BM25 did). Order is the contract.
- **Nothing prepended**: the old always-include-`index.md` behavior
  dies — registry + FTS + vectors are the navigation. (Since settled:
  `index.md` doesn't exist at all — see "`index.md` — dies".)

### Ask accounting — its own `asks` table, not a runs row

**Locked: ask executions are recorded in a separate `asks` table.**

```sql
asks(
  id          TEXT PRIMARY KEY,  -- ULID
  owner       TEXT NOT NULL,
  question    TEXT NOT NULL,
  status      TEXT NOT NULL,     -- running | succeeded | failed | crashed
  started_at  INTEGER NOT NULL,
  finished_at INTEGER,
  usage       TEXT,              -- token/cost accounting
  error       TEXT NOT NULL DEFAULT ''
)
```

Rationale:

- **Jamming ask into `runs` bends two locked semantics**: `caused_by`
  (NOT NULL, the provenance key) would need a '' sentinel — the same
  join-breaking sentinel rejected for dead-letter — and the
  failure-path policy code, which fires on every failed run, would
  need an "unless it's an ask" branch. Two special cases to save one
  table.
- The dup_flags/stale_notes precedent verbatim: same pattern, separate
  tables, no genericism for things different in kind (ask has no
  causing row, no selection/in-flight claim, no retries).
- Lifecycle mirrors runs where honest: insert `running` at start,
  finalize at end, boot sweep marks orphans `crashed` — no policy
  step, just truthful status; spend on crashed asks stays visible.
- **Storing `question` is quiet gold**: real questions are golden
  candidates for the eval harness, and "what do I actually ask my
  wiki" is a product question the table answers for free.

Schema rider for schema finals: the `asks` table.

### Ask prompt — rough shape (six sections)

1. **Task framing.** A research librarian answering one question from a
   curated personal/business wiki. The framing to nail: **answers come
   from pages, not from the model's world knowledge** — the wiki is the
   universe; if it doesn't know, the answer is "the wiki has nothing on
   this," stated plainly. Same polarity as match and extract: the cheap
   honest failure over the confident fabricated one.
2. **The corpus model.** What a subject/page is (one page per subject,
   curated prose, identity-establishing lead), what the registry is,
   what citations mean (statements cite arrivals), contradictions
   corralled in marked sections.
3. **Procedure.** Names → `lookup` first (exact, free); everything
   else → `search`; read pages; `read_source` only when pages disagree
   or the question needs exact wording/figures; **reformulate and
   retry before concluding absence** — the iteration that makes the
   lexical lane work is a prompt obligation, not a hope.
4. **Answer craft.** Direct answer first, detail after; every claim
   carries its page reference per the locked answer contract; a
   contradiction found on a page is *surfaced* — both sides, both
   citations — never silently resolved by the agent picking a winner.
5. **Budget discipline.** Bounded turns; stop reading when marginal
   pages stop changing the answer; an honest partial answer with
   citations beats a complete-looking one without.
6. **Output schema + one worked example.** Answer text + structured
   `sources` — few-shot for conformance, the standing lesson.

Riders: literal text → exact-prompts item; model → exact-models item
(ask is agent-shaped — likely the merge-class model, not match-class);
budget knobs (max turns / tokens / wall clock) → config defaults.

## Open / not yet discussed

- **Digest integrator** — settled (see "Integration — the digest pass"),
  including the compile prompt's rough shape (extract's skeleton + four
  deltas, see "Compile prompt — rough shape"). Nothing remaining.
- **`occurred_at` for event subjects** — settled (see "`occurred_at` — a
  registry column on event subjects"): nullable TEXT ISO-8601 prefix on
  `subjects`, events only, set at commit, first-writer-wins (later
  disagreement is page-level contradiction). Remaining: the column lands
  in schema finals.
- **Source pages** — settled (see "Source pages — die"): no per-document
  page; the inbox row + blob is the document's sole representation,
  reached via citations + `read_source`; a document that is itself
  salient becomes an ordinary extracted subject. Nothing remaining.
- **`index.md`** — settled (see "`index.md` — dies; the registry is the
  index"): no index page, no merge obligation; browsing, if ever wanted,
  is an on-demand view over `subjects`. Nothing remaining.
- **Failure handling** — settled (see "Failure policy"): the frame
  (bounded retries + dead-letter); `ineligible_until` set on every failed
  run, exponential jittered formula; the `dead_at` mark; counter scope =
  since re-queue via `requeued_at`; all failures count (conflict-retry
  exhaustion included); check at failure time by whatever marks the run;
  threshold `WIKI_RUN_ATTEMPTS_MAX` (config, default 5); failed batch run
  blocks the cron row's stamp; dead-letter granularity = the causing row
  only; dead-lettering notifies, attempts don't. Vocabulary:
  "conflict-retry exhaustion" (renamed from "conflict exhaustion").
  Remaining: the notification *mechanism* (outbox event + notify
  subscription) is finalized in the eventplane-producer item; the inbox
  columns (`ineligible_until`, `dead_at`, `requeued_at`) land in schema
  finals.
- **Concurrency rule** — settled (see "Concurrency — the worker pool"):
  **N identical self-selecting workers, no central dispatcher** (config,
  default 4) replacing the never-agreed single-flight proposal; at-most-one-
  in-flight is a `TryLock` per work key (job name for digests/lint, inbox row
  id for documents) over an in-memory in-flight set; crash-resume rides
  stamp-only-at-commit (in-flight is RAM, wiped on crash); SQLite contention
  identical to the lone-dispatcher model; optimistic commit with page versions;
  alias-UNIQUE as the duplicate-minting guard; shared conflict loop (cap 3,
  fail cleanly, `conflicts` counter on runs); two digest rules (one in-flight
  run per batch entry; stamp by id list); arrival-order integration explicitly
  forfeited. Remaining: the `ineligible_until` mechanism is finalized in
  failure policy; the `pages.version` column lands in schema finals; and the
  claimable unit for digests (Framing 1 cron-row vs Framing 2
  `(cron-row, entry)` pair) is an open build-time sub-question.
- **Subject taxonomy depth** — mostly settled (type tests, freeform
  prompt-anchored `kind`, salience gate in the extract prompt shape).
  Remaining: confirm `kind` column lands in the `subjects` schema final.
- **Re-integration semantics** — settled (see "Re-integration — rejected;
  stamps are permanent"): `integrated_by` is never cleared, no replay of
  successful runs; corrections enter as new documents through the front
  door; failed-run retry was never affected (stamp rides the end-of-run
  transaction). Nothing remaining.
- **Page growth** — settled (see "Page growth — no split, ever"): one
  subject = one page is an invariant; growth is the merge prompt's
  compression mandate; a lint condense job is the contained later shape
  if ever needed. Nothing remaining.
- **Chunking threshold** — settled (see "Size cap — `WIKI_INGEST_MAX_BYTES`
  at Accept"): no ingest-side chunking; hard size cap at Accept (config,
  default 128 KiB), refused loudly at the front door. Remaining: the
  notify-on-refusal rider rides the notification-mechanism item.
- **Exact prompts** — rough shapes for all five (extract, match, merge,
  compile, ask) are now recorded in their sections. Remaining: the literal
  prompt text + worked examples, and whether a shared invariants block is
  factored out.
- **Exact models** per call site (match is small-and-cheap shaped; extract
  and merge likely differ too).
- **Schema finals**: `pages`, `runs`, `subjects`/`aliases`, `dup_flags`,
  FTS tables. Riders from the concurrency walk: `version INTEGER` on
  `pages` (optimistic commit), `conflicts` on `runs` (already in the runs
  block above). Riders from the failure policy: `ineligible_until`,
  `dead_at`, `requeued_at` on `inbox` (all nullable epoch-ms; locked, see
  "Failure policy"). Rider from the taxonomy walk: `occurred_at` on
  `subjects` (nullable TEXT ISO-8601 prefix, events only; locked).
- **Benchmark/eval harness**: goldens for extract, match, merge — where the
  first rewrite's scorecard thinking lands. Rider from search/ask:
  **retrieval goldens** are a first-class deliverable — per call-site
  query shapes (search, candidates, sweep), recall@k, lexical-only vs
  hybrid side by side; they gate the vector lane's per-site wiring and
  tune the sweep's two thresholds. The `asks.question` column feeds
  golden candidates from real usage. Riders from the prior-art study:
  **cross-subject and temporal-span ask goldens** (multi-hop
  association; date-range narrative) — the cross-subject set gates
  `related`; verify compile puts enough event-time into digest claims
  for the date-range class; and a three-way fusion comparison
  (lexical-only / RRF / convex combination per arXiv 2210.11934) before
  RRF k=60 calcifies.
- **Lint** — settled (see "Lint"): family of three named jobs (lint-dups,
  lint-sweep, lint-stale) on the existing spine (re-confirmed), triggers
  as rows from any door (cron or manual MCP verb); dup-queue consumption
  (judge first; merge/dismiss/open with page-version re-judge gate);
  dup_flags canonical pair order + UNIQUE + `FlagDup`; subject merge
  (older ULID wins, hard-delete loser, five-step transaction); semantic
  sweep (flag-only, fully mechanical FTS); stale_notes channel + repair
  job (tool-less, per-subject); citation preservation moved to a
  commit-time gate (not lint); page growth = compression, no split;
  unresolved relative time struck; one `jobs` config; job/integrator/
  lint-job vocabulary. Remaining riders: `judged_version_a/_b` on
  dup_flags, the `stale_notes` table, `runs.integrator` → `runs.job` →
  schema finals; judge/fold/repair prompts → the exact-prompts item;
  judge/fold/repair models → the exact-models item; lint job cadences →
  config defaults at build time.
- **Transcripts** — settled (see the `kind` bullet in "Schema"): they are
  `kind=document` through the ordinary document pass; no third kind;
  dialog-awareness (speaker attribution in claims) is an extract-prompt
  rider. Nothing remaining beyond that rider.
- **Search / ask — settled** (see "Search / ask — the read side"):
  hosted-ask-first (the wiki runs the retrieval loop server-side);
  search survives as a zero-LLM side door (steering tool descriptions);
  ask is strictly read-only (synthesis pages die; escape hatch =
  ingest the answer), synchronous, six read tools (search / lookup /
  read_page / read_source / timeline / goldens-gated related — see
  "Cross-subject and temporal-span queries"; public read surface =
  ask + search + timeline), page-level answer citations; index is
  hybrid by design (FTS5 first in build order) — one retriever, three
  call sites (search, candidates, sweep), vector lane wired per site
  only on measured lift; one vector per page; async catch-up embedder
  (stale vectors serve until replaced); plain `page_vectors` table +
  brute-force scan (pure-Go rules out sqlite-vec); OpenAI
  `text-embedding-3-large`@1024 (`OPENAI_API_KEY` gates the lane);
  model change = config change the catch-up worker absorbs; RRF k=60
  for ranked consumers, per-lane thresholds for sweep; search contract
  (registry-pinned rank 1, whole pages, default 3/cap 10, no scores,
  no index.md prepend); `asks` accounting table; ask prompt rough
  shape. Remaining riders: `page_vectors` + `asks` tables → schema
  finals; ask prompt literal text → exact-prompts; ask model →
  exact-models; ask budget knobs + embed model/dims → config defaults;
  **retrieval goldens** (per-call-site, lexical vs hybrid) → eval
  harness.
- **`owner` column** — settled (see the `owner` bullet under the inbox
  schema): stays on `inbox` and `asks` as attribution (the account is a
  business; many users per box), never isolation — knowledge is shared
  per box; pending index drops to `(integrated_by)`. Remaining: the
  system-identity literal for eventplane doors → schema finals.
- **Eventplane producer** — settled (see "Eventplane producer — yes, with
  exactly two events"): standard appkit outbox/`/feed`;
  `wiki.row_dead_lettered` + `wiki.ingest_refused`, both consumed by
  notify; new events only with a real consumer. Remaining: payload shapes
  → schema finals.
