# wiki — Research: the `ask` retrieval upgrade

**Status: informational, non-contractual.** This doc feeds the author of
`project/design/README.md` and nothing downstream consumes it (the autonomous
build reads only product, design, plan). It is a single coherent statement of
current research — edited in place as the goal evolves, never appended to.

**Goal of this research.** The wiki service is fully built; `ask` today is
deliberately narrow. This research scopes the **planned, previously-postponed
retrieval upgrade**: replace exact-subject-name lookup with a **fuzzy hybrid
search over pages** (OpenAI embeddings + FTS5/BM25, fused), so a question finds
the right pages even when it does not name a subject verbatim.

> This doc **replaces** the earlier phase-1 *build* research (scaffolding the
> service over appkit/agentkit, porting from `wiki.bak`). That work shipped; its
> research no longer informs the current goal. The two still-load-bearing pieces
> of it — the `Retriever`/`Hit` seam (§8) and the anti-collapse invariant (§10) —
> are carried forward below. Git history holds the rest.

---

## 0. The frame (decided with the owner before research)

These bound everything below; the research operated inside them.

1. **This is the postponed retrieval work, now scheduled** — not a reversal of
   product. The "no fuzzy/semantic matching" lines in `product.md` were "not
   yet," and this is the "yet." `product.md` will be re-authored after design.
2. **Relax retrieval; keep the integrity guarantees.** The three guarantees that
   give the wiki its trust contract stay: answers drawn **only** from ingested
   wiki content, **every answer cited**, and an honest **"nothing here"** when
   retrieval genuinely finds nothing. Only the *matching* step (exact-name →
   fuzzy hybrid) changes.
3. **Pages are the search surface.** The compiled, deliberately-lossy per-subject
   pages (≤12,000 chars) are what we search — not raw claims, not source text.
   Lossiness is the point of the wiki.
4. **Hybrid = OpenAI embeddings (semantic) + FTS5/BM25 (keyword), fused.** A
   **local embedding model is ruled out, permanently.** An external embedding API
   is accepted; the owner chose **OpenAI**.
5. **Query-side decomposition is for the research to settle** — whether to extract
   subjects, claims, expand, or just embed the raw question. (Settled in §6.)
6. **Retrieval evaluation is out of scope** for this work — a later effort. Design
   the mechanism; do not build a recall/precision harness now. (Leave the seam
   clean so it can land later — the `extract` eval harness in `internal/eval/` is
   the template to copy when it does.)
7. **Scale ceiling: 100,000 pages.** SMB knowledge base; typical far less. One
   query at a time, low QPS. Every storage/latency number below is sized to this
   ceiling.

---

## 1. Where `ask` is today, and what changes

`internal/ask/ask.go` — current flow (`Ask`, lines 77–117):

1. **`ask-subject`** call (`extractPrompt`): LLM extracts literal subject *names*
   from the question.
2. **`gatherPages`** (lines 145–187): each name → `Resolver.ResolveByName`
   (**exact normalized name**) → `PageStore.GetBySubject` (one page per subject,
   deduped). An unresolved name contributes nothing.
3. Zero pages → `honestEmpty()` (no synthesis call).
4. **`ask-synthesis`** call (`synthPrompt`): answer from the gathered page bodies
   only, returning `{found, text, citations}`; `validateCitations` (lines
   214–231) requires every citation to map to a gathered page.

**Reuse / rebuild map** (from the codebase agent):

| Component | Disposition |
|---|---|
| `Answer` / `Citation` types | **Keep** — return contract unchanged. |
| `honestEmpty()` gate | **Keep** — but its trigger moves from "0 names resolved" to "0 hits clear the relevance floor" (§7). |
| `ask-synthesis` call + `validateCitations` | **Keep** — citations still resolve to pages; only *which* pages enter the candidate set changes. |
| `gatherPages` (exact-name resolution) | **Rebuild** — replace with a `Retriever` (§8) returning `[]Hit`; drop name-only resolution. |
| `ask-subject` extract call | **Repurpose** — into query analysis (§6), not deleted. |

**Hook points** (file:line, for the design to target):
- Replace retrieval: `internal/ask/ask.go:145–187` (`gatherPages` → consume
  `[]Hit` from a `Retriever`).
- Re-embed a page on write: `internal/wiki/service.go` immediately **after**
  `pages.Upsert(...)` inside the ingest integrate tx (~line 334) **and** in
  `mergeSubjects` (~lines 469–533) — but the network call goes *after commit*,
  not inside the tx (§3).
- New call sites: `internal/wiki/config.go:17–22` (`CallSites` struct +
  `resolveCallSite`).
- Widen `llm_calls.stage` CHECK: a **new** table-rebuild migration (§9).

**Build-loop guard (process constraint).** Per `wiki/CLAUDE.md`, none of this is
hand-coded in an interactive session. Everything lands as design Decisions
(`project/design/DNN.md` + INDEX) and plan phases (`project/plan/phase-NN.md` +
STATUS line); the ralph loop builds it. Migrations are forward-only and
immutable — never edit `20260622001058_drop_pages_fts.sql` or the existing
`llm_calls` migrations; add new ones.

---

## 2. Embeddings via agentkit (already available, already pinned)

**The single biggest de-risking finding.** wiki imports the **external**
`github.com/ikigenba/agentkit v0.1.0` (enforced by `cmd/wiki/module_wiring_test.go`
— a versioned require, **no `replace`**). The pinned **v0.1.0 tag already
contains the embeddings API** (`embed.go`, `embedding.go`,
`internal/openaicompat/embedding.go`). So embeddings are consumable from wiki's
*current* dependency — **no version bump, no `replace`, no off-box `bin/ship`
risk.** The old research's "publish/vendor the external agentkit" open question
is **closed**. (Newer tags v0.1.1–v0.1.3 exist if a bump is ever wanted, but
v0.1.0 suffices.) The **in-repo** `agentkit/embed/` module is the dead one — do
**not** target it.

### The API (concrete)
```go
// composition root (cmd/wiki/main.go): read key yourself, inject explicitly
provider := openai.NewEmbedder(os.Getenv("OPENAI_API_KEY"))   // agentkit never reads env
embedder := &agentkit.Embedder{
    Provider:   provider,
    Model:      "text-embedding-3-small",
    Dimensions: 512,                       // optional reduction; omit for native
    Retry:      agentkit.RetryPolicy{},    // zero value = sane backoff
}
res, err := embedder.Embed(ctx, []string{pageBody}, agentkit.InputDocument) // page side
res, err := embedder.Embed(ctx, []string{query},    agentkit.InputQuery)    // ask side
// res.Vectors() [][]float32 (L2-normalized), res.Usage(), res.Cost()
```

### Facts the design must honor
- **Vectors are L2-normalized** (`normalizeFloat32Vectors` in agentkit `embed.go`)
  → **cosine similarity = plain dot product.** Store them as-is; no wiki-side
  normalization needed. *(Note: agent F claimed normalization is absent — that was
  the dead in-repo module; the external agentkit wiki uses does normalize.)*
- **`float32`** throughout.
- **Roles**: `InputDocument` for page bodies, `InputQuery` for the question —
  `text-embedding-3` is trained for this asymmetry; use it.
- **Dimension reduction is supported** (set `Dimensions` in `[Min,Max]`); OpenAI
  shortens server-side (Matryoshka). 1536→512 ≈ 99% retrieval quality.
- **Batching**: up to 2,048 inputs/request, auto-fanned, order preserved.
- **Usage/cost tracked** per call and cumulatively.

### Model & dimension choice
| Model | Native dims | Max input tok | Price (USD/M tok) |
|---|---|---|---|
| `text-embedding-3-small` | 1536 | 8192 | $0.02 |
| `text-embedding-3-large` | 3072 | 8192 | $0.13 |

**Recommendation: `text-embedding-3-small` reduced to 512 dims.** ~99% of native
quality at a third of the memory and ~6× lower price than `-large`. Make
**model + dims a configured call-site knob** (§9) and store `model@dims` with
every vector so a future swap is a re-embed, not a schema break. Embedding 100k
pages of ~1–2k tokens each ≈ 100–200M tokens ≈ **$2–4 one-time** on
`-small` — cost is a non-issue; recall quality drives the choice.

---

## 3. Vector storage & search — brute-force cosine in Go

**Recommendation: store vectors as float32 BLOBs in SQLite; brute-force cosine
(dot product) in Go.** No extension, no ANN index.

### The numbers that decide it (100k ceiling)
| dims | RAM (100k × 4B) | brute-force latency / query |
|---|---|---|
| 3072 (3-large native) | 1.23 GB | ~3–15 ms |
| 1536 (3-small native) | 614 MB | ~3–8 ms |
| **512 (recommended)** | **205 MB** | **~1–5 ms** |
| 256 | 102 MB | ~1–3 ms |

Brute-force flat scan stays adequate to **~1M vectors**; at 100k we are **~10×
under**, at 512 dims ~30× under. Per-query cosine time is dwarfed by the OpenAI
query-embedding round-trip (tens–hundreds of ms) anyway. ANN only earns its
complexity past ~500k–1M vectors *or* high QPS — neither applies.

### `sqlite-vec` — ruled out (for the right reason)
`asg017/sqlite-vec` is a C extension; its Go binding needs **CGO**, but wiki's DB
driver is **`modernc.org/sqlite` (pure-Go, CGO-disabled)** and cannot load it.
(A WASM path exists but buys nothing at this scale.) Brute-force in Go sidesteps
the whole question and keeps the static-binary build clean. A pure-Go HNSW
(`coder/hnsw`) exists but is unnecessary index-maintenance complexity at 100k.

### Storage shape
A **side table**, not a column on `pages` (pages are 1:1 with subjects;
`pages.id == subject_id`):
```sql
CREATE TABLE page_embeddings (
    subject_id   TEXT PRIMARY KEY,   -- == page id
    model        TEXT NOT NULL,      -- e.g. "text-embedding-3-small"
    dims         INTEGER NOT NULL,   -- e.g. 512
    vec          BLOB NOT NULL,      -- little-endian float32, L2-normalized
    content_hash TEXT NOT NULL,      -- sha256(title\nbody) at embed time
    updated_at   TEXT NOT NULL
);
```
Side table keeps the hot page-read path narrow, makes "missing vector" a trivial
`NOT EXISTS`, and lets a model/dims swap be a re-embed rather than a schema break.
Vectors and the FTS index live in the **same SQLite file**, so appkit's
`VACUUM INTO` backup/restore snapshots them atomically — **no special handling**.

### Lifecycle — embed *after* commit, never in the tx
The ingest integrate tx (`service.go`, ~lines 297–356) is deliberately
local-only and fast; the expensive compile already runs **before** `BeginTx`.
Putting an OpenAI call inside the tx would hold SQLite's single writer
(`SetMaxOpenConns(1)`) open across a network round-trip and break the
"reads never block on ingest" promise. So:

- **FTS5 sync stays inside the tx** (pure-local SQLite write — see §4).
- **Embedding happens after commit**, via a **catch-up worker**: select pages
  whose `page_embeddings` row is missing, whose `content_hash` ≠ current
  `sha256(title\nbody)`, or whose `model@dims` ≠ configured — and (re-)embed them
  out of band. `hashText`/sha256 already exists in `data_model.go`. (Prior
  research assumed a `pages.version` column for staleness; **there is none** —
  use `content_hash`.)
- Same after-commit treatment for the **merge** page rewrite.

### Backfill (on first ship)
- **FTS5**: backfill deterministically in the migration itself —
  `INSERT INTO pages_fts(pages_fts) VALUES('rebuild');` (pure SQL, no network).
- **Vectors**: no migration (no network in migrations). Every existing page
  starts with a missing/mismatched `content_hash`, so the steady-state catch-up
  worker **drains the backlog on first boot** with no new verb. (If an operator
  wants an explicit trigger, the existing `rerun` machinery is the closest fit,
  but lazy catch-up suffices and never blocks `ask` on a cold vector.)

---

## 4. The keyword lane — re-add FTS5

FTS5 `pages_fts` existed (phase-02 data model) and was **deliberately dropped**
(`20260622001058_drop_pages_fts.sql`) once `ask` became exact-name-only and
nothing consumed keyword search — eliminating tested-but-unreachable code and the
in-tx FTS-sync trap. This upgrade **re-introduces it**, paying that cost back on
purpose.

- Recreate as an **external-content** FTS5 table over `pages(title, body)`
  (`content='pages'`), as before.
- **Sync explicitly in the same tx as each page write** (the phase-02 comment
  intended exactly this): on UPDATE, issue the FTS5 `'delete'` with the **OLD**
  title/body read *before* the row update, then re-insert. `wiki.bak`'s
  `ftsPhrase()` / external-content sync is the reference to copy verbatim.
- **Sanitize the query**: wrap user terms as quoted FTS5 phrase literals
  (`"` → `""`), OR them together — both escapes operators/injection and widens
  lexical recall across aliases/synonyms.

BM25 earns its place in the hybrid: it catches exact/rare terms, identifiers, and
out-of-vocabulary tokens that embeddings smear, and it is nearly free.

---

## 5. Fusion — Reciprocal Rank Fusion

**Recommended pipeline:**
```
query ─┬─ FTS5/BM25  → top 60 (list A)
       └─ cosine     → top 60 (list B)
              └─ RRF fuse (k=60) over the union
                     └─ relevance floor (§7)
                            └─ final K = 8 pages → ask-synthesis
```

- **RRF over weighted score fusion.** `score(p) = Σ_lanes 1/(k + rank_lane(p))`,
  **k = 60** (the Cormack-2009 sweet spot; the default in OpenSearch, Elastic,
  Azure AI Search, Weaviate, Mongo Atlas). BM25 is unbounded and cosine is
  [-1,1] — they share no axis, so rank-based fusion needs no normalization and no
  labeled tuning data, and empirically beats linear fusion. Keep a `WIKI_RRF_K`
  knob.
- **Parameters**: ~**60 candidates per lane**, final **K ≈ 8** pages (range 5–12;
  8 × 12k cap ≈ 96k chars, comfortable for a Claude synthesis context). Make
  candidate-count and K config knobs.
- **Reranking: skip.** No dedicated rerank model (only Anthropic + OpenAI
  embeddings); a separate LLM-reranker call is cost/latency for marginal gain
  once RRF has produced a clean top-8 from same-corpus dual retrieval. If ever
  needed, the cheap path is to **fold it into synthesis** — pass the fused top
  ~12 and let the single synthesis call select/cite what it uses (zero extra
  calls). Add a true two-stage reranker only if (future) eval shows fusion
  surfacing junk.
- **Registry-first option** (from prior research, still apt): pin an exact
  normalized-name match at rank 1 when the question names a subject verbatim,
  then let the hybrid fill the rest — preserves today's precise behavior as a
  special case of the new path.

---

## 6. Query-side strategy

**Recommendation: keep one LLM query-analysis call (you already pay for it), but
change what it emits, and send *different* text to each lane.**

- **Repurpose the `ask-subject` call into query analysis.** Have it emit a small
  JSON: `{ sub_queries: [...], keywords: [...], aliases: [...] }`. The highest-ROI
  thing it does is **decomposition**: the corpus is **one page per subject**, so a
  "compare X and Y" question must fan out to X's page and Y's page separately — a
  single blended query embedding sits between them and matches neither. Cap
  sub-queries at 3–5 (redundancy outweighs recall past ~5).
- **Two different query strings — the key construction detail:**
  - **Dense (embedding) lane** ← the **full natural-language** question / each
    sub-query verbatim (`InputQuery` role). `text-embedding-3`'s query encoder
    expects the sentence, not keywords.
  - **Keyword (FTS5/BM25) lane** ← the **extracted keywords + entities + aliases**,
    sanitized and OR-ed — *not* the whole sentence (stopwords dilute BM25 and
    raise FTS5 syntax/injection risk).
- **HyDE — skip.** It backfires on exactly this setting (lossy summary pages +
  named entities): it invents plausible-but-wrong entity names/dates and is
  discouraged for fact-bound retrieval. Decomposition + alias expansion already
  buys the semantic-alignment win HyDE targets, without the hallucination tax and
  extra generation.
- **Multi-query paraphrase fan-out — skip.** ~1.77× runtime and N× search for
  recall on *broad* queries; decomposition already produces the *useful* kind of
  query multiplicity (distinct subjects).

**Default**: one analysis call → `{sub_queries[≤4], keywords, aliases}`; per
sub-query, embed the raw text (dense) + OR'd sanitized keywords/aliases (BM25);
RRF-fuse all lanes; dedupe by `PageID`. **Cheaper variant** (if multi-subject
questions are rare): no decomposition — embed the raw question + FTS5 on extracted
keywords. **Richer variant** (only if a future eval shows a gap): conditional
HyDE fired only when top fused similarity is below a confidence threshold.

---

## 7. Honest-empty & citations under fuzzy retrieval

Fuzzy search **always** returns *some* top-K, so the old "0 names resolved → empty"
gate disappears. Restore honesty with a **two-layer gate**:

1. **Deterministic relevance floor before synthesis.** Apply a **cosine floor**
   (start ~**0.30–0.35** absolute; tune against a handful of known in/out-of-corpus
   probes) and require the fused top hit to clear a minimum. If nothing survives,
   return `honestEmpty()` **without calling the synthesis LLM** — preserving the
   no-LLM-on-empty property the keyword path had. Make the floor a config knob.
   *(Floor on the raw lane signals, not the RRF score, which is uncalibrated.)*
2. **Keep `found=false`** as the second layer: the synthesis prompt already says
   "if the pages do not answer, return found=false," and `validateCitations`
   enforces that a found answer cites real gathered pages.

**Citations stay well-defined.** A citation resolves to a **page/subject**
regardless of *how* the page was retrieved (named vs. fuzzy). `validateCitations`
maps `{subject,title}` → `{path,title}` unchanged; fuzzy retrieval only changes
which pages enter the candidate set, not the citation contract.

---

## 8. The retrieval seam (carried forward, corrected)

Prior research already designed this seam for exactly this moment; keep it, with
the `Version`-staleness note corrected (no `version` column — see §3).

```go
type Hit struct {
    SubjectID string  // subject the page belongs to
    PageID    string  // stable fusion/dedup key AND citation ref (== subject_id)
    Score     float64 // lane-local: BM25 / cosine / fused RRF
    Snippet   string  // matched excerpt for citation + synthesis context
    Title     string
}
type Retriever interface {
    Search(ctx context.Context, query string, k int) ([]Hit, error)
}
```
Compose behind one interface: `keywordRetriever` (FTS5 `MATCH` + `bm25()`),
`vectorRetriever` (embed query → brute-force cosine), `hybridRetriever` (fan out
+ RRF). `ask` depends only on `Retriever`. **Lock now:** `PageID` is the stable
fusion+citation key every lane populates; `Search` returns a flat `[]Hit` (fusion
is `hybridRetriever`'s private detail). Add a `SearchLimits{Default,Cap}.Resolve()`
clamp.

---

## 9. Call-site config & `llm_calls` recording for embeddings

**Record embedding calls in `llm_calls`, and make embedding a configured call
site** — both fit the existing pattern and keep the "every LLM call recorded"
cost-accounting promise honest.

- **Two new call sites** (genuinely distinct): **page-embed** (ingest/merge time)
  and **query-embed** (ask time). Add to the `CallSites` struct + `resolveCallSite`
  in `config.go` following the existing `EXTRACT_*/COMPILE_*/ASK_*` env layering;
  the knobs are **model + dims** (not reasoning/temperature).
- **New `llm_calls` stages** `embed-page` and `embed-query`, added via a **new
  table-rebuild migration** (SQLite can't `ALTER` a CHECK; the existing
  `20260624200122_widen_llm_calls_stage.sql` is the rebuild template). `CallRecord`
  already carries `provider/model/usage`; `params` carries `{dims}`.
- **One integration caveat**: `llm.Client` today wraps only the *chat* provider;
  `agentkit.Embedder` is a different interface, so the embed path needs a small
  parallel record path (or thin adapter) onto the existing `Recorder` /
  `LLMCallStore.Record` seam — both reusable as-is.

---

## 10. The anti-collapse invariant (carried forward)

Unchanged by this work, but restate so design doesn't accidentally violate it
while touching the ingest/page path:

> **Claims are extracted only from raw source text, never from generated pages.**
> The moment claims are re-derived from pages, the recursive model-collapse loop
> is reborn.

This upgrade touches only **retrieval** (the read path) and **page embedding** (a
read-only derivative of an already-compiled page) — neither re-extracts claims —
so the invariant is preserved by construction. Worth a one-line design note that
embedding a page is a read-only projection, not a re-ingest.

---

## 11. Open questions for design to settle

1. **Cosine floor value** — start 0.30–0.35, but it needs calibrating against
   real in/out-of-corpus questions; design should name the default and make it a
   knob, and note how it'll be tuned (a few manual probes now; eval later).
2. **Final K and per-lane candidate counts** — defaults K≈8, 60/lane; confirm
   against the 12k page cap and synthesis context budget.
3. **Decomposition on by default, or the cheaper single-query variant?** Depends
   on how common multi-subject ("compare X and Y") questions are for this owner.
   Recommend on-by-default with a cap of 4 sub-queries.
4. **Embedding model & dims** — recommend `text-embedding-3-small @ 512`;
   confirm and lock as the page-embed call-site default. `model@dims` stored per
   vector so this can change later.
5. **Catch-up worker shape** — a new `Spec.Workers` poll loop vs. folding into the
   existing ingest worker; and whether to expose an explicit re-embed/backfill
   trigger or rely on lazy catch-up.
6. **Registry-first rank-1** — keep the exact-name-pinned-at-rank-1 behavior, or
   let the hybrid stand alone? (Recommend keep — cheap precision, preserves
   today's behavior as a special case.)
7. **Query-analysis output schema & prompt** — exact JSON
   (`sub_queries/keywords/aliases`) and how it threads into the two lanes.
8. **Vector load strategy** — load all vectors into a RAM slice at startup vs.
   stream from the BLOB column per query. At 100k×512 (~205 MB) an in-RAM cache is
   fine and fastest; design should pick and state cache-invalidation on re-embed.
</content>
</invoke>
