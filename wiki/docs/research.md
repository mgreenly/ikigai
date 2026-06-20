# wiki — Research

**Status: informational, non-contractual.** This doc feeds the author of
`wiki/docs/design.md` and nothing downstream consumes it (the autonomous build
reads only product, design, plan). It records options, prior art, constraints,
and recommendations gathered before design. It is a single coherent statement of
current research — edited in place as the goal evolves, never appended to.

Scope: **phase 1** of the wiki (the thin proving slice defined in
`wiki/docs/product.md`): `ingest(text)` async → extract subjects+claims → compile
per-subject pages → `ask` (grounded, cited, honest-empty) over keyword search,
plus `status`, inspect tools, `health`, empty `reflection`.

---

## 0. The load-bearing constraints (read first)

These shape every later section.

1. **Use the EXTERNAL agentkit; the in-repo one is dead.** wiki must depend on
   `github.com/ikigenba/agentkit` (source `~/projects/agentkit`), the
   multi-provider conversation/orchestration SDK. The **in-repo** `agentkit/`
   (module `agentkit`) is a *different*, now-**dead** library — do not import it
   or use it as a reference. (It still has stale importers like prompts/sites;
   that is legacy, not a pattern to follow.)
2. **The external agentkit has no embeddings/vector API *yet*, but will gain one
   before wiki needs it.** Verified: today there is no `embed/` dir or embedding
   funcs in `~/projects/agentkit`. **Decision (owner): embeddings will be added to
   `github.com/ikigenba/agentkit` before wiki's vector lane needs them** — so the
   vector follow-on consumes agentkit's embeddings API rather than a
   bring-your-own client. Phase 1 (keyword-only) does not touch embeddings at all;
   the seam (§4) just has to leave room for that future API. (The in-repo
   `agentkit/` module is **dead** — do not import it or use it as a reference.)
3. **The external agentkit has NO structured-output / JSON-schema-constrained
   generation and NO forced `tool_choice`** (verified). Structured JSON for
   extract/compile is the service's responsibility (prompt + parse + validate +
   retry).
4. **Prod-build dependency wrinkle (must be resolved in design).** Every existing
   dependency is either an in-repo sibling (`replace => ../x`) or a published
   module on the proxy. An absolute-path `replace github.com/ikigenba/agentkit
   => /home/mgreenly/projects/agentkit` works for local dev but **will break the
   off-box `bin/ship` build** (it builds HEAD in a throwaway worktree where that
   path does not exist, with `GOWORK=off`). Design must pick one: publish/tag the
   external agentkit on the module proxy, vendor it, or move it into the
   mono-repo. This is the single biggest non-domain risk to shipping.
5. **The previous build (`wiki.bak/`) already solved most of the hard problems.**
   Its prompts, FTS5 setup, citation chain, async-worker pattern, and retrieval
   seam are directly portable. The bloat to leave behind is the
   event/cron/digest/aliasing/dup-flag/lint/eval machinery. Mining `wiki.bak` is
   the cheapest path to a correct thin slice.

---

## 1. The agentkit SDK — how wiki drives the LLM

Module `github.com/ikigenba/agentkit`, Go 1.26, pre-stable. Single public package
`agentkit`; each provider is a thin sub-package (`anthropic`, `openai`, `google`,
`zai`). The whole model: build a `*Conversation` struct, call
`conv.Send(ctx, text) *Stream`, range `stream.Events()`. The tool loop runs to
completion internally; events are whole completed **messages**, never token
deltas.

### Core surface
```go
type Conversation struct {
    Provider Provider; Model string; System string
    Gen GenSettings; Retry RetryPolicy
    Tools []Tool; MCPServers []MCPServer
    History []Message; MaxToolIterations int // 0 => 1000
    Log io.Writer
}
func (c *Conversation) Send(ctx, userText) *Stream
func (c *Conversation) Close() error
func (c *Conversation) TotalUsage() Usage; func (c *Conversation) TotalCost() Cost
```
`Stream`: `Events() iter.Seq[Event]` (Go 1.23 range-over-func, **iterate exactly
once, must drain or break**), `Err()`, `Usage()`, `Cost()`, `Warnings()`. Event
union: `MessageDone{Message}`, `ToolUse{ID,Name,Input}`, `ToolResult{...}`. The
last `MessageDone` (or `conv.History[len-1]`) is the final answer. On a
successful turn `History` is replaced+extended and usage accumulates; on
failure/early-break the turn rolls back. `Send` validates upfront and returns an
error-stream for nil provider / empty model / unknown model / empty input / a
prior stream still live / after `Close()`.

### In-process tools (this is how `ask` works)
In-process Go-function tools are first-class — `ask`'s internal `search` tool
should be one of these, **not** a remote MCP server (no network hop).
```go
func NewTool[In any](name, desc string, fn func(ctx, in In) (string, error)) Tool
func RawTool(name, desc string, schema json.RawMessage, fn func(ctx, json.RawMessage) (string, error)) Tool
```
`NewTool` auto-derives the JSON schema from `In` via `invopop/jsonschema` (use
`json:` tags + `jsonschema:"required"`). Tool output is **text-only** (return a
JSON string if you want structure). An ordinary tool error is fed back **in-band**
(`ToolResult{IsError:true}`) so the model can recover; only MCP transport faults
abort the turn. Tool names must be unique.

### Providers, models, reasoning, retries, cost
- **Explicit key injection** — `provider.New(apiKey, opts...)`. agentkit never
  reads env or any secret store; wiki reads `ANTHROPIC_API_KEY` itself (via appkit
  config) and passes it in. Options: `WithBaseURL`, `WithHTTPClient`.
- **Closed curated model sets** (unknown id → `ErrInvalidConfig`). Anthropic:
  `ModelOpus48` (`claude-opus-4-8`), `ModelSonnet46` (`claude-sonnet-4-6`),
  `ModelHaiku45`. Also openai (gpt-5.x), google (gemini 2.5/3.x), zai (glm).
  **Best-tested: anthropic** (tool schemas pass through verbatim) and openai;
  google is lossy on tool schemas (drops `$ref`/`oneOf`, emits a `Warning`).
- **Reasoning** via `Gen.Reasoning`: `Level("low".."max")`, `Budget(n)`,
  `DisableReasoning()`. An unsupported value is non-fatal (falls back + `Warning`).
- **Retries**: `Conversation.Retry` (zero value = 4 attempts, exp backoff+jitter,
  honors `Retry-After`, transient categories only). The `ctx` is the whole-turn
  deadline.
- **Cost/usage**: per-turn `stream.Usage()/Cost()`, cumulative
  `conv.TotalUsage()/TotalCost()`; baked-in per-model pricing; Anthropic prompt
  caching applied automatically.

### Server-embedding gotchas (shape the design)
- `Conversation` is **not concurrent-safe and single-flight** → **construct a
  fresh `Conversation` per request**; share the stateless `*Provider` across
  requests.
- Streams are lazy, one-shot, must be drained; check `.Err()` after.
- Pass the request `ctx` (with deadline) into `Send`.
- Tool handlers run on agentkit's loop goroutine and **block the turn** — keep
  `search` fast and ctx-aware.
- No persistence — `History` is in-memory; serialize yourself if ever needed.

### Recommendations
- **`ask`** → a per-request `Conversation` with one in-process `search` tool
  (`NewTool`), Anthropic Sonnet 4.6, a strict grounded/honest-empty `System`
  prompt. Consume events, take the final message.
- **extract** (text→subjects+claims) and **compile** (claims→page) → a
  **tool-less** `Conversation` with `Temperature: 0` and low/disabled reasoning;
  `Send` once; take the final text; `json.Unmarshal` + validate. Because there's
  no JSON mode, build one small **"json-mode helper"** in wiki (prompt template +
  fenced-code stripping + unmarshal + retry-on-parse-failure) and reuse it for
  both. (Stronger-schema alternative: a single `NewTool[Out]("emit", …)` whose
  handler captures the typed input — but the model can't be *forced* to call it,
  so prompt-and-parse is lighter for batch ingest.)

---

## 2. The appkit chassis — scaffolding the service

A service = `appkit.Main(appkit.Spec{…})`. appkit owns the fixed verbs
(`serve`/`version`/`manifest`/`migrate`/`schema`/`backup`/`restore`),
config-from-env, SQLite open + forward-only migration runner with downgrade
guard, the loopback HTTP server (PRM doc, identity gate, ungated `/health`,
optional `/feed`, **Workers** lifecycle), and manifest emit. It knows nothing of
LLMs/jobs/tools — those are the service's `Handlers`/`Workers`/`Config` hooks.
Cleanest references: **`crm/`** (MCP producer) and **`notify/`** (consumer with a
background Worker). Chassis is `appkit/`.

### The `Spec` fields wiki uses
`App:"wiki"`, `Mount:"/srv/wiki/"`, `Port` (e.g. 3007), `MCP:true`,
`Migrations: db.FS`, `Handlers func(*Router) error`, `Workers
[]func(ctx)error`, optional `Health func(ctx)(map,error)`, optional `Config
func(getenv)(any,error)`, `ManifestExtras []ManifestKV`. Leave `Feed:""`,
`Consumes:nil`, `Events:{}`, `Publishes:nil`, `Subscriptions:nil` — wiki is on no
event plane in phase 1.

### Key mechanisms
- **Closure-capture wiring**: `Handlers` runs *after* appkit opens+migrates the
  DB and *before* Workers start. Build the domain `Service` in `Handlers` over
  `rt.DB()` (the shared single-writer `*sql.DB`), capture it in a `var`, and read
  it in the `Workers` closure. notify/crm do exactly this.
- **MCP mount + identity**: `rt.Handle("POST /mcp",
  rt.RequireIdentity(mcp.NewHandler(...)))`. `RequireIdentity` reads
  `X-Owner-Email` (401 + PRM challenge if absent) and stashes
  `Identity{OwnerEmail,ClientID}` on the ctx (`appkit.IdentityFrom(ctx)`).
  Services do zero token logic — nginx is the trust boundary.
- **MCP transport**: a minimal JSON-RPC 2.0 over HTTP POST in
  `internal/mcp/mcp.go` (copy crm's near-verbatim): `initialize`,
  `notifications/initialized`, `tools/list`, `tools/call`. Tools are declared as
  **hand-coded maps** (`desc/obj/typ/descTyp/enumTyp` helpers), dispatched by a
  `switch`. Result helpers `toolResultText/Err/JSON`.
- **Migrations**: `internal/db/migrations/*.sql` embedded via `//go:embed` as
  `db.FS`; `001_schema_migrations.sql` is frozen/verbatim across services; new
  ones via `bin/new-migration wiki <name>` (UTC-timestamped). Runner applies
  unapplied versions each in a tx, enforces a downgrade guard, auto-runs on
  serve. **Do not open the DB yourself at serve time** — appkit hands it via
  `rt.DB()`.
- **DB driver**: `modernc.org/sqlite` (pure-Go, no cgo), WAL + foreign_keys ON +
  busy_timeout, **`SetMaxOpenConns(1)`** (single-writer). The worker and MCP
  handlers serialize on that one connection → keep job-claim transactions short.
- **health / reflection**: `/health` is chassis-owned and ungated; the `health`
  MCP tool renders `appkit.Envelope(version,service,details)` + identity; the
  `reflection` tool returns `{publishes: events.Index(), subscribes:
  renderSubscriptions(...)}`. With empty Events/Subscriptions this **returns
  `{publishes:[],subscribes:[]}` for free** — exactly the phase-1 empty
  reflection. Still declare the tool (copy crm/notify).
- **Background worker**: use `Spec.Workers` — **do not hand-spawn a goroutine in
  serve**. appkit runs each `func(ctx)error` on the serve context; SIGTERM
  cancels ctx; a worker that *returns* is treated as a structural fault and
  brings the server down (so retry transient faults *inside* the loop, never
  return). notify's two consumer loops are the template; wiki's ingest worker is a
  poll-loop over the `jobs` table that exits only on ctx.Done().
- **Config**: appkit auto-reads `WIKI_*` universal knobs (`WIKI_PORT`,
  `WIKI_DB_PATH`, `WIKI_LOG_LEVEL`, …). Non-secret service config (model id, caps,
  concurrency) → `ManifestExtras` (round-trips into `etc/manifest.env`). Secret
  (`ANTHROPIC_API_KEY`) → read at the composition root via `getenv` (or
  `os.Getenv` in `Handlers`), fail loudly if absent; locally injected by a
  `wiki/.envrc` doing `source_up` + `export
  ANTHROPIC_API_KEY="$(cat ~/.secrets/ANTHROPIC_API_KEY)"`.

### New-service recipe (condensed)
1. Copy the shape of `crm/` (Makefile, etc/, `internal/db/db.go`,
   `internal/mcp/{mcp,tools}.go`), search-replace `crm`→`wiki`.
2. `wiki/go.mod`: module `wiki`; require+replace `appkit` (`=> ../appkit`) and
   `eventplane` (needed transitively); require the **external**
   `github.com/ikigenba/agentkit` (resolve the prod-build question in §0.4).
3. `go.work`: swap `use ./wiki.bak` → `use ./wiki`.
4. `bin/new-migration wiki <schema>` and `bin/new-migration wiki jobs`; keep
   frozen `001_schema_migrations.sql`.
5. `cmd/wiki/main.go`: `appkit.Main(Spec{App:"wiki", Mount:"/srv/wiki/",
   Port:3007, MCP:true, Migrations:db.FS, Handlers:…, Workers:[ingest loop]})`.
6. Domain in `internal/wiki/`; ingest worker in `internal/worker/`; tools
   `ingest`/`status`/`ask`/inspect + copied `health`/`reflection`.
7. `etc/manifest.env` = `wiki manifest`; `VERSION` = `0.1.0`.

---

## 3. What to port from `wiki.bak` (prior art in-tree)

The prior build is over-engineered but its **core LLM shapes and SQLite plumbing
are excellent and directly reusable.** Terminology map: old "inbox" = queued
arrival (raw text); old "run" = execution/status; old "merge" = your
claims→page compile; old "extract" = your subjects+claims extraction.

| Item | Source | Disposition |
|---|---|---|
| Extract prompt + schema + parser | `internal/config/extract_prompt.go`, `internal/integrate/extract*.go` | **Port**, drop aliases |
| Compile ("merge") prompt + ApplyMerge | `internal/config/merge_prompt.go`, `internal/integrate/merge.go` | **Port**, drop stale-notes; **add 12k cap yourself** |
| Schema (subjects/pages/jobs/asks) | `internal/db/migrations/…consolidated_schema.sql` | **Adapt**, drop aliases/dup_flags/stale_notes/vectors |
| FTS5 DDL + query + sync | `internal/page/{store,read,write}.go` | **Port as-is** (incl. `ftsPhrase`, external-content sync) |
| Async worker / status | `internal/worker/worker.go`, `internal/ingest/ingest.go` | **Adapt**, drop cron + conflict loop (~60%) |
| Citation chain | extract→merge→`internal/read/ask.go` | **Port** the inline-`[id]` + Answer contract |
| Retrieval seam | `internal/read/read.go` | **Port** `Retriever` + `SearchLimits` |
| Ask agent prompt/tools | `internal/config/ask_prompt.go`, `internal/read/ask.go` | **Adapt** — keep search/read_page/read_source |

Highest-value, lowest-risk: the **prompts** and the **FTS5 setup**.

### Notable specifics
- **Extract prompt** (`DefaultExtractPrompt`): six sections; closed-set type
  `entity|event|concept` + freeform `kind` subtype + `occurred_at` (events only).
  Two rules worth keeping verbatim: the **salience gate** ("extract a subject
  only when it is identifiable — the Wikipedia-article test — AND the document
  makes a concrete claim about it; never invent a subject to hold a stray fact")
  and the **claims discipline** ("each claim is a short SELF-CONTAINED prose
  statement, no pronouns; state only what the document ASSERTS — do not infer or
  synthesize"). `DocumentHeader` (source/title/tags/received-on) anchors relative
  time. No native JSON mode → it parse+validates with a `stripCodeFence` util.
  Drop the `aliases` field for phase 1.
- **Compile prompt** (`DefaultMergePrompt`): the **fold discipline** (weave new
  claims in; attach a new citation rather than repeat; on contradiction keep BOTH
  in a "Conflicting accounts" section, never silently overwrite) and the **lead
  discipline** (first paragraph states what the subject IS — its identity card).
  Crucially it folds from **the claims/manifest only, never the original
  document** ("the raw document invites re-extraction") — this is the
  anti-poisoning invariant in §5 made concrete. **Gap: the 12,000-char cap was
  never enforced in the old code** (no prompt mention, no write guard) — design
  must add both a cap instruction and a hard write-time guard.
- **Schema**: `subjects(id ULID, type, kind, canonical_name, occurred_at)`,
  `pages(subject PK, title, body markdown w/ inline [id] cites, version)`.
  Relationships documented in comments, **not FK-enforced** (suite's deliberate
  app-level-invariant style; use UNIQUE/CHECK where the DB must enforce). For
  phase-1 exact-name identity, add `UNIQUE(type, norm_name)` on subjects and drop
  the aliases table. Normalization spec to copy: "NFKC, casefold, trim, collapse
  ws, strip diacritics." Collapse old `inbox`+`runs` into one `jobs` table for
  `pending|working|done`, but keep the **separation of work-item (carries the
  permanent raw text) from execution-status**.
- **FTS5**: external-content virtual table `pages_fts(title, body,
  content='pages', content_rowid='rowid')`, **no triggers** — sync is explicit in
  the same tx as the page write. Two traps the old code gets right: `ftsPhrase()`
  wraps the user query as a quoted phrase literal (injection/operator safety),
  and an UPDATE must issue the FTS5 `'delete'` with the **OLD** title/body read
  *before* the row update, then re-insert. Copy both verbatim.
- **Async worker**: a dispatcher-free pool of identical goroutines; in-flight
  claims tracked in a **RAM-only** set under a mutex; **stamp only at commit,
  never at claim** (crash leaves the row `pending`, restart re-selects; boot sweep
  flips orphaned `running`→`crashed`); a contentless `Nudge()` (sync.Cond) wake
  after Accept is an optimization, not truth (every wake re-scans). Status derives
  from the arrival row + latest run. Drop the cron selection and the
  optimistic-commit conflict loop (~60% of the file).
- **Citation chain (port end-to-end)**: extract stamps each claim's cite with the
  arrival id → compile writes **inline `[inbox-id]` citations in the page body** →
  `ask` returns `{answer, citations:[{subject,title}], sources:[id], found}`. The
  `found:false`+empty-citations path is the honest-empty contract. The
  inline-`[id]`-in-body convention is the linchpin.
- **Retrieval seam (port)**: already a one-method interface (see §4).

**Leave behind:** `eval/`, `lint/`, `events/`, `consume/`, `producer/`,
`inbox/` event-doors, the `integrate/{match,resolve,digest,compile,manifest}.go`
aliasing/dup/digest machinery, and `page/vectors.go` (keep as the vector-lane
reference).

---

## 4. The retrieval seam (keyword now, hybrid later)

Phase-1 search is FTS5 keyword only, but must sit behind a seam so a vector/hybrid
lane drops in without touching the read path or `ask`. `wiki.bak` already shipped
this exact design; port it.

### Proposed seam
```go
type Hit struct {
    SubjectID string  // subject the page belongs to
    PageID    string  // stable fusion/dedup key AND citation ref
    Version   int     // page version retrieved (citation provenance)
    Score     float64 // lane-local: BM25 / cosine / fused RRF
    Snippet   string  // matched excerpt for citation + ask context
    Title     string
}
type Retriever interface {
    Search(ctx context.Context, query string, k int) ([]Hit, error)
}
```
Compose behind the same interface: `keywordRetriever` (FTS5 `MATCH` + `bm25()`,
the only one wired in phase 1); later `vectorRetriever` (embed query →
brute-force cosine over stored vectors); later `hybridRetriever` (fan out to
both, fuse with RRF). `ask` depends only on `Retriever`; the follow-on swaps the
composition root. **Lock two decisions now so they don't churn:** `PageID` is the
stable fusion+citation key every lane must populate, and `Search` returns a flat
`[]Hit` (fusion is `hybridRetriever`'s internal detail). Add a
`SearchLimits{Default,Cap}.Resolve()` clamp. `wiki.bak`'s `Service.Search` also
pins an exact normalized-name match at rank 1 then lets the retriever fill the
rest — that "registry-first" pattern fits phase-1 identity well.

### Vector follow-on shape (decide the seam now; build later)
- **Embeddings provider**: the vector lane consumes **agentkit's embeddings API**
  (being added to `github.com/ikigenba/agentkit` before wiki needs it — §0.2), not
  a bring-your-own client. wiki should depend on that API rather than re-implement
  a provider client. Likely default model: OpenAI `text-embedding-3-small`
  (1536 dims, ~ $0.02/1M, cheap/no GPU), with the actual provider/model selection
  surfaced through agentkit. **Store `model@dims` with every vector** so a provider
  swap is a re-embed migration, not a schema break.
- **Storage under pure-Go SQLite**: store vectors as a **BLOB column + brute-force
  cosine in Go** — for thousands of pages this is sub-ms to a few ms per query,
  needs no extension. Normalize at write time so cosine = dot product. (Note:
  `asg017/sqlite-vec` C bindings do **not** load under modernc; a pure-Go
  `modernc.org/sqlite/vec` port reportedly now exists but is unverified and buys
  nothing at this scale.) Revisit only past ~100k vectors.
- **Fusion**: **Reciprocal Rank Fusion**, `score = Σ 1/(k+rank)` over lists,
  `k=60`. Preferred over score normalization because BM25 (unbounded) and cosine
  ([-1,1]) share no axis; RRF fuses on rank, needs no calibration. Keep a weight
  knob (`WIKI_RRF_K` existed in `wiki.bak`).
- **Chunking**: pages are ≤12k chars and already lossy summaries — **embed the
  whole page as one vector** (keeps `page_vectors` 1:1 with `pages`, makes the
  re-embed worker trivial — re-embed when `pages.version` advances — and keeps
  `PageID` identical across lanes). Chunk only if long-page recall later proves
  weak.

---

## 5. The LLM-wiki risk to respect (and the invariant that defeats it)

The "Karpathy LLM-wiki risk" is real and **validates this architecture**. His
~Apr-2026 "LLM Knowledge Bases" note frames it as a compiler: immutable raw
sources are source code, the generated wiki is the compiled binary, compilation
flows **one way only**. The academic backing — Shumailov "Curse of Recursion"
(*Nature* 2024), Alemohammad "Self-Consuming Models Go MAD", Gerstgrasser
(accumulating real+synthetic breaks the curse) — shows **model collapse requires
a closed loop where gen-N output is fed back as gen-(N+1) truth**. It is not a
property of generation itself.

**Recompile-from-claims structurally immunizes wiki** against collapse as defined:
each page derives from the same fixed claim/raw substrate, never from its
predecessor page. This is exactly why the compile step must fold from
**claims only, never from prior pages or the raw document re-extracted**.

**The one hard invariant (carry into design as a stated constraint):**
> Claims are extracted **only** from raw source text, **never** from generated
> pages. The moment claims are re-derived from pages, the recursive loop is reborn.

### Residual traps (different in kind — all real)
- **Frozen extraction error**: claims are themselves LLM-extracted, so a bad claim
  is baked into the durable record and faithfully recompiled forever — silent and
  permanent. Mitigate with **provenance + confidence on every claim** (carry
  `{doc_id, offsets}`), since there is no human curator.
- **Boundary leakage**: if any downstream consumer treats wiki's *published pages*
  as authoritative source, the loop reappears outside the boundary. (Not a phase-1
  concern — wiki is on no event plane — but worth a design note.)
- **Extraction tail loss**: even one-directional extraction systematically drops
  rare/low-salience claims the extractor deems unimportant.

### Best practices worth borrowing (phase-1-appropriate subset)
- **Claim extraction** (Claimify / VeriScore / FActScore lineage): select →
  disambiguate (refuse to extract when meaning can't be resolved, don't guess) →
  decompose into self-contained verifiable claims. But avoid **over-atomization**
  ("Molecular Facts": too-stripped claims lose the context to identify the right
  entity) — keep just enough disambiguating context. `wiki.bak`'s salience gate +
  claims discipline already encode most of this.
- **Grounded/honest-empty answering — prompt alone is theater.** The realistic,
  no-fine-tuning levers for a Go/SQLite service: (1) a **retrieval-score gate** —
  if top-k is below a cutoff, deterministically short-circuit to "nothing in the
  wiki" *before* the model runs (the biggest honest-empty lever wiki controls);
  the keyword-only phase-1 analogue is "zero FTS hits → honest-empty without an
  LLM call". (2) Treat the grounded/cite/abstain **system prompt as a necessary
  first layer, not sufficient.** A post-hoc NLI entailment verifier (answer claims
  vs. cited spans) is the rigorous enforcement, but is likely **over-scope for the
  phase-1 thin slice** — note it as a known gap / fast-follow rather than build
  it now. (3) Citation = *support*, not bracket-presence; verify a cited id
  actually backs the claim if/when a verifier lands.
- **Lossy compile that stays faithful**: the recompile-from-claims path is **not**
  a broken-telephone chain (it reads primary facts, not a prior summary), so it
  accumulates no generational error — favor it. A **claim-anchored hybrid** is
  ideal long-term (incremental edits day-to-day; full recompile from claims on the
  12k-cap boundary, on claim deletion/correction, or after N edits) — but note
  `rebuild` (full recompile) is explicitly deferred; design must still **factor
  ingest's extract/compile stages to be independently re-invokable** so the thin
  recompile re-trigger can land later without reshaping.
- **Subject typing (`entity|event|concept`)**: a defensible coarse 3-bucket
  scheme (entity≈endurant, event≈perdurant, concept≈abstract); pulling out
  `event` is principled. Pitfalls: the **entity/concept boundary is genuinely
  fuzzy** and items are often both; **type can change**. Recommendations that fit
  phase-1's closed set: treat `type` as **metadata on a stable subject, never part
  of identity** (a type change is a cheap edit, not a migration) — phase-1
  identity is normalized-name exact match, so this is naturally satisfied. (The
  literature's "allow multiple types / allow `other`" advice is noted but conflicts
  with product.md's locked closed set + exactly-one-type rule — flag for design,
  don't override the product.)

---

## 6. Open questions for design to settle

1. **Prod build of the external agentkit** (§0.4) — publish/tag, vendor, or
   move in-repo. Blocking for ship; pick before plan.
2. **`ask`'s internal `search` tool vs. the public inspect/search tools** — are
   they the same surface or separate? (Handoff flagged this undecided.) The seam
   in §4 suggests both go through `Retriever`; design should state whether the
   agent's tool is literally the public `search` or a distinct internal one.
3. **Exact phase-1 schema** for subjects/claims/pages/jobs — re-spec from
   `wiki.bak`'s consolidated schema minus aliases/dup/stale/vectors, with
   `UNIQUE(type,norm_name)` and the collapsed `jobs` table.
4. **12k-char cap enforcement** — prompt instruction + hard write-time guard
   (the old code had neither). Decide truncate-vs-recompile-tighter on overflow.
5. **Honest-empty mechanism for keyword-only phase 1** — confirm "zero FTS hits →
   deterministic empty answer, no LLM call" as the rule.
6. **Reasoning/temperature settings** per call site (extract/compile vs. ask).
7. **Whether to keep `wiki.bak`'s `superseded`/version-guard citation
   accounting** (cheap correctness) or simplify it out for the thin slice.
</content>
</invoke>
