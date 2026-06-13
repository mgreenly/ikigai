# wiki evaluation — research

> **What this is.** Research, before design, on how we measure and evaluate the
> wiki's effectiveness — specifically the places the system relies on **inference
> to make a decision**. The motivating requirement: be able to test different
> models in different roles, different effort levels for the same work, and score
> decisions — by building a **synthetic test for essentially anything an LLM
> does**, then seeing how models perform. Tests come in **generations**: when one
> saturates we author a harder one and keep the old as a regression floor — we do
> not try to classify individual cases by difficulty. The worked example that
> named the need: a synthetic test for alias collisions.
>
> **What this is *not*.** These evaluations are **not a test harness, not CI, and
> not a release blocker**. Nothing here gates a deploy or fails a build. They are
> offline tools to *run and score implementations* — a prompt, a model, an effort
> level, a config — so we can **compare candidates and decide which to use**. A
> score is an input to a human decision, never an automated pass/fail.
>
> **Status.** This is the research feeding a future `wiki-evaluation-design.md`.
> It fixes *what* must be evaluated and the *shape* of each evaluation; it does
> not fix the harness's concrete schema, file layout, or build phases — that is
> the design doc's job. Input to this doc: `wiki-redesign-decisions.md` (the
> authoritative running log; every call site below is read from it).
>
> **Scope boundary.** We evaluate the places a *model* decides. The deliberately
> mechanical spots — `normalize()`, older-ULID-wins, the page-version gate,
> brute-force cosine — are excluded by construction: nothing infers, so there is
> nothing to score, only ordinary unit tests. The one grey area we *do* include
> is hybrid retrieval: no LLM runs in the lane, but the design repeatedly defers
> its thresholds and its lexical-vs-hybrid wiring "to the eval harness," so the
> harness owns it.

## The inference inventory

Every place the redesign relies on inference to make a decision. Five are LLM
calls in the write path, one is the read-path agent, one is a lint judge, one is
a low-stakes naming judgment, and three are mechanical-but-tuned retrieval sites
the design explicitly hands to the harness.

| # | Site | Shape | Output | The error that hurts |
|---|------|-------|--------|----------------------|
| 1 | **Extract** | structured, tool-less, full-context | `subjects[] {type, kind, name, aliases, claims, occurred_at}` | over-extraction; non-self-contained claims |
| 2 | **Match** (resolution) | structured, tool-less, shortlist in | `same(id)` \| `no_match` (+ dup side-channel) | **false-merge** (poisons a page) |
| 3 | **Compile** (digest) | structured, tool-less, event pile in | `subjects[]` + per-claim `cites` + `occurred_at` | citation mis-attribution; under-compression |
| 4 | **Merge** | agentic (read/write pages) | rewritten prose pages | dropped citation; ledger-not-prose; buried identity |
| 5 | **Dup judge** (lint) | structured, full pages in | `merge` \| `dismiss` \| `can't-tell-yet` | **false-merge** (irreversible surgery) |
| 6 | **Canonical-name pick** (lint) | tiny judgment | which name is canonical | low stakes (id chosen mechanically) |
| 7 | **Ask** | agentic RAG (lookup / search / read / read_source) | cited answer | fabrication from world knowledge |
| 8–10 | **Retrieval** (candidates / search / sweep) | hybrid BM25 + vector, no LLM in lane | ranked pages / flagged pairs | missed candidate → silent permanent dup |

The alias-collision example spans three of these that share one synthetic
identity corpus: **#2 Match** (the judgment), **#8 candidates** (the recall step
that feeds Match a shortlist), and **#10 sweep** (the proactive walker that finds
duplicates built from disjoint streams). Treating them as one corpus, three
consumers, is the first structural saving the harness buys.

## The uniform shape every evaluation takes

Abstractly, every call site is `f(fixed_prompt, model, effort, input) → output`,
and an evaluation is `score(output, gold)` aggregated over a dataset. So every
site's eval is assembled from the same four reusable parts:

1. **A dataset** of `(input, gold)` cases, each tagged with a *failure-mode
   label* (which behavior the case stresses), belonging to a numbered
   **generation** of the test set.
2. **A runner** that holds the production prompt fixed and sweeps the cartesian
   product `{model} × {effort} × {cases}`, calling the **real call-site code**
   with model and effort injected as config.
3. **A site-specific scorer.**
4. **A report**: one row per config (model × effort), cells = score **plus cost
   and latency**, always reported *against a named generation*. That table *is*
   the answer to "is opus-at-high worth ~4× sonnet-at-medium on this generation."

### Three principles that outrank any single metric

- **Evaluate the production code path, never a reimplementation.** Prompt, model,
  and effort are injected as config into the *real* call-site function so the
  schema, tool wiring, and assembly are byte-identical to prod. A forked copy
  scores the fork, not the system. This is a hard constraint on how the call
  sites are written: each takes its **prompt, model, and effort from config**,
  never a hard-coded constant — and "production" is then just a pinned
  `(prompt, model, effort)` triple per site, one configuration the harness can
  also evaluate.

- **Harder tests are later generations, not a difficulty classification.** We do
  not try to stamp a case "easy/medium/hard" — that label is unreliable and
  nothing scores against it. Instead a test set comes in **generations**: when a
  generation saturates (every candidate aces it, so it no longer separates
  them), we author the next generation with subtler cases. "Harder" is a property of a
  *later generation as a whole*, observed through saturation, not assigned to
  individual cases up front. A generator naturally produces a spread of case
  shapes from blunt to subtle; later generations weight the mix toward the subtle
  end, but we let the scores — not a label — tell us a generation got harder.

- **Scoring is asymmetric, because the design's polarity is.** Nearly every site
  has a deliberately lopsided error cost — "doubt is `no_match`," "when in doubt
  do not extract," "the wiki has nothing on this." Raw accuracy averages away
  exactly the thing those decisions tune. Every scorer therefore reports the
  **dangerous-direction error rate separately** (false-merge rate,
  over-extraction count, fabrication rate), never a single lumped accuracy.

### Test sets are versioned, swappable bundles (committed)

**A test set is a named, versioned bundle of `(dataset + prompt)` for one site,
and the harness runs against a chosen test set — never against a single
hard-wired one.** This is a committed property, not an afterthought, because of
how evals decay: a test set we author today will eventually **saturate** —
every candidate aces it — at which point it stops separating them and its only
remaining value is as a baseline. When that happens we author a *harder* test
set (new data, and a refreshed prompt if the harder data needs one) and stand it
up alongside the old. Consequences:

- **Datasets and prompts are each independently versioned artifacts**; a test set
  references one of each. A new test set is minted by swapping either — a harder
  dataset against the same prompt (the saturation escape hatch), or a candidate
  prompt against the same dataset (prompt iteration). Both are first-class.
- **Saturated generations are kept, never deleted.** An aced generation stays a
  useful baseline — it still tells you a candidate hasn't *regressed* on the
  basics even though it no longer separates the leaders — while the newer, subtler
  generation does the discriminating. Generations are the single escalation
  mechanism — there is no separate within-set difficulty ladder to maintain.
- **The sweep matrix gains an outer dimension.** Within a generation the harness
  sweeps `model × effort`; the generation (hence its prompt and data) is the
  outer selector. So the full comparison is
  `generation → {prompt, data} × model × effort`, and a result is always reported
  *against a named generation* — "config X scores Y **on gen 2**" — so saturation
  is visible rather than silently flattering a stale set.
- **Reconciles with the production-code-path principle:** because the prompt is
  an injected input, evaluating a candidate prompt and evaluating the pinned
  production prompt are the *same* operation against different config — no special
  path, and the prompt the harness scores is always run through the real call
  site.

### What the harness is also tuning

The matrix compares configs within a generation, but its real product is licensing the
config the redesign deliberately defers. Knobs the harness arbitrates:
`WIKI_MATCH_EXCERPT_CHARS` (default 600), the candidate FTS thresholds, the
per-lane sweep thresholds, RRF `k` (60), the embed model and dims
(`text-embedding-3-large@1024`), the ask turn/token budget — and, orthogonal to
all of them, **model-per-role and effort-per-role**.

## Per-site evaluation design

### 1 — Extract

- **Decides:** which subjects a document carries, and what it asserts about each.
- **Hard because:** the salience gate (identifiable *and* claim-bearing),
  within-document co-reference, and self-contained claims that must read
  correctly far from their source.
- **Synthetic data:** documents authored with a known gold subject+claim set,
  spanning case shapes from blunt to subtle — a single clearly-named entity with
  one explicit claim; several subjects with co-reference and an event whose
  relative date must resolve against the context header; a dense document with
  near-salient distractors ("the team", "the meeting"), nested co-reference
  (Apollo the program vs Apollo 11), self-contradiction (both claims must be
  emitted), and the dialog/transcript shape (attribute speakers, don't extract
  the back-and-forth). Later generations weight toward the subtle end.
- **Score:** align predicted↔gold subjects (by normalized name/alias overlap),
  then report **subject precision and recall separately** — over-extraction is
  the dangerous axis and gets its own counter; type accuracy on matched
  subjects; claim recall (fuzzy / LLM-judged); a self-containedness check (does a
  claim still carry a pronoun or "the company"?). Claim quality is measured here
  because it is the input quality merge inherits.

### 2 — Match (resolution judge)

- **Decides:** is this incoming subject the *same real-world thing* as one of the
  candidates on the shortlist.
- **Hard because:** identity ≠ similarity, and a false-merge poisons a page
  irrecoverably while a false-split is cheap and lint-repairable — the contract's
  "doubt is `no_match`" polarity is the whole calibration.
- **Synthetic data (the headline case):** build a synthetic registry of subjects
  with pages, then generate **positive (truly same)** cases spanning blunt to
  subtle — identical name + corroborating claims; different surface form, same
  identity (AWS / Amazon Web Services); zero-token-overlap synonym (AWS / "Amazon
  Cloud") — and **negative (truly different)** cases likewise — different name
  and type; similar name, different thing (Apollo program vs Apollo 11); and the
  subtle trap, **identical name, different real-world thing** (two Bob Smiths,
  different employer/city, the distinguishing fact buried in the candidate
  excerpt). Vary shortlist size and the gold candidate's rank; seed look-alike
  candidate pairs to exercise the dup side-channel. Later generations lean on the
  subtle cases.
- **Score:** binary confusion, but **false-merge rate and false-split rate
  reported separately** — a config that is 95% accurate with one false-merge on a
  subtle case can be strictly worse than one at 90% with none. Score the
  `dup_pairs` side-channel as its own recall number. Use this site to sweep
  `WIKI_MATCH_EXCERPT_CHARS`: rerun the subtle cases at 600 vs 1200 chars and watch
  for truncation-driven identity errors (the excerpt is the merge prompt's
  obligation to this call site — see Merge).

### 3 — Compile (digest)

- **Decides:** the few durable claims a pile of routine machine events still
  warrants next month — narrate outcomes, not deltas.
- **Hard because:** aggregation-is-knowledge (fourteen `stage_changed` events →
  one outcome claim), and per-claim citation attribution — the model can
  mis-attribute an inbox id, a risk the document path structurally cannot have.
- **Synthetic data:** event piles with a gold aggregate-claim set, from blunt to
  subtle — a clean sequence of N events → one outcome claim; interleaved events
  for several subjects in one pile (split by subject *and* aggregate); a noisy
  pile with reversals (opened → advanced → lost) where the correct claim is the
  net outcome, with enough events that the cited subset is non-trivial.
- **Score:** reuse extract's subject/claim aligner, plus a **compression ratio**
  (was the per-event noise suppressed?), **citation precision/recall per claim**
  (the mis-attribution risk becomes a number), and `occurred_at` accuracy.

### 4 — Merge

- **Decides:** how to fold a manifest's claims into prose pages — woven, not
  ledgered; identity in the lead; contradictions corralled; citations preserved.
- **Hard because:** the output is prose, so quality is partly subjective — yet
  several of its invariants are exact.
- **Split the eval in two:**
  - **Mechanical checks (deterministic, no judge):** citation preservation (the
    design already specifies this as a commit-time gate — score it as a per-case
    pass/fail count),
    write-set conformance (only the manifest's pages mutated), and every assigned
    claim-cite id present in the new body. These catch the load-bearing
    invariants for free.
  - **Rubric LLM-judge (panel):** grade each rewritten page on a fixed rubric —
    lead-identity-establishing, woven-not-ledgered, contradictions-corralled, no
    information loss vs the input claims, no hallucination beyond them — scored
    per criterion, multiple judge samples for the subjective ones.
- **Synthetic data:** construct manifests directly (skip extract) — a base page
  plus claims with known relationships (new info / corroborating-existing /
  contradicting-existing / identity) — from an empty page + two claims (new-page
  craft) to contradiction injection, a claim that belongs in the lead, and many
  existing citations that must all survive.
- **Closed-loop bonus check:** run **Match** against the merged page's 600-char
  lead to confirm identity is still recoverable — that is merge's explicit
  obligation to the match call site, made testable.

### 5 — Dup judge (lint)

- **Decides:** `merge` / `dismiss` / `can't-tell-yet` on a pair of existing
  subjects, given both full pages and complete alias lists.
- **Hard because:** subject merge is irreversible surgery, and `dismiss` is
  permanent (it blocks the pair from ever re-flagging) — so both error directions
  are costly, and over-using `can't-tell-yet` means the system never converges.
- **Synthetic data:** **shares Match's identity corpus** — the same underlying
  "are these the same?" truth, repackaged as full pages plus a third outcome.
  Add cases where the pages genuinely lack distinguishing evidence (gold =
  `can't-tell-yet`) vs cases where the evidence is present (must decide).
- **Score:** three-way confusion with **false-merge and false-dismiss as the two
  dangerous axes**, plus a "`can't-tell-yet` when evidence was present" laziness
  metric. Comparing this site against Match on the shared corpus quantifies how
  much full-page evidence + the ternary option buys over excerpt + binary — a
  finding in its own right.

### 6 — Canonical-name pick (lint subject merge)

- **Decides:** which of two names becomes `canonical_name` on the surviving
  subject (the id is chosen mechanically — older ULID wins — so this is the only
  judgment in the merge).
- **Low stakes**, included for completeness. A small preference golden set
  (e.g. "Robert Smith" vs "Bob Smith") with a stated convention; score as
  agreement-with-convention. Worth a thin dataset; lowest priority.

### 7 — Ask (RAG agent)

- **Decides:** the cited answer to a question, read-only and agentic, iterating
  over lookup / search / read / read_source within a turn budget.
- **Hard because:** it must refuse to answer from world knowledge — "the wiki has
  nothing on this" is the correct answer when the corpus is silent, the same
  cheap-honest-failure polarity as match and extract.
- **Synthetic data:** build a fixed synthetic wiki (pages with known facts, known
  **gaps**, and known **contradictions**), then a question set: **answerable**
  (from single-page lookup by name to multi-page / temporal-span / cross-subject
  synthesis), **unanswerable** (gold = "the wiki has nothing"), and
  **contradiction** (gold = surfaces both sides with both citations). Real
  questions from the `asks` table become free goldens once the system runs.
- **Score:** answer correctness (LLM-judge against the gold answer + gold
  supporting pages); **citation faithfulness** (each cited page actually supports
  its claim); **abstention correctness on the gap set** — the fabrication rate is
  the headline metric; contradiction-surfacing. Decompose **retrieval failure**
  (the needed page was never retrieved) from **synthesis failure** (retrieved but
  answered wrong), so a low ask score points at the right subsystem. Sweep the
  turn budget here: more turns buy recall at a cost, and the matrix shows the
  knee.

### 8–10 — Retrieval (candidates / search / sweep)

- **No LLM runs in the lane**, but the redesign makes "lexical-only vs hybrid,
  per call site, scored" a first-class harness deliverable, so it lives here.
- **Synthetic data:** seed a registry with known synonym and zero-token-overlap
  pairs (the candidates/sweep blind spot BM25 cannot see) and known cross-stream
  duplicate pairs (Bob-from-email vs Robert-from-CRM, never co-examined at
  integration time).
- **Score, per call site:** recall@k for **candidates** (recall is king — a miss
  silently mints a permanent duplicate subject; precision merely spends a cheap
  judge call); a ranking metric for the **search** verb's top-k; pair-discovery
  recall for the **sweep**. The deliverable is the **side-by-side**: recall lift
  from adding the vector lane vs its cost (shortlist bloat feeding paid judge
  calls), per site, with embed model/dims, per-lane thresholds, and RRF `k` as
  sweep axes. This is the measurement that licenses (or declines) the vector lane
  at each site — the design asserts the lane's *existence* but explicitly refuses
  to assert its *value* anywhere it is not measured.

## High-level plan to build the harness

Four pieces, in dependency order. (Phasing for execution is the design/plan
doc's job; this is the shape.)

1. **The rig (build once).** A dataset format
   `(case_id, site, generation, failure_tag, input, gold)`; a runner that injects
   `(prompt, model, effort)` into the **real** call-site functions and captures
   raw outputs with cost and latency; a results table `config × metric`, reported
   per generation. Scorers plug in per site.
2. **The scorer library.** Four kinds cover every site: set-alignment
   (extract, compile), asymmetric binary/ternary confusion (match, dup judge),
   recall@k + RRF (retrieval), and mechanical-checks + rubric-judge-panel
   (merge, ask).
3. **Per-site test sets.** One generator per site, producing a versioned
   `(dataset + prompt)` bundle — a generation — that the harness selects by name
   (so a later, harder generation can stand up beside the first). **Share corpora**:
   one identity corpus → match + dup + candidates + sweep; one synthetic wiki →
   ask + search. LLM-author the goldens, then run an **adversarial verification
   pass** over them (the redesign's own pattern); anchor against real data
   wherever it exists (ingested documents for extract, the `asks` table for ask).
4. **The sweep + report.** Run the matrix and produce the comparison table that
   answers the operating questions directly: which model in which role, at which
   effort, and whether the lift is worth the cost. That table is the product —
   and it is what then licenses the config defaults the redesign currently defers
   (excerpt chars, candidate/sweep thresholds, RRF `k`, embed model/dims, ask
   budget).

## Open questions for the design doc

- **Golden authorship vs real-data harvesting.** Each site lists a synthetic
  generator *and* a real-data anchor (ingested docs, the `asks` table). The
  design must decide the mix per site and whether real data seeds the generator
  or only validates it.
- **Judge-model independence.** Merge and ask lean on an LLM-as-judge. The design
  must fix the judge model (and whether it may be one of the models under test —
  it should not be, to avoid self-preference) and the panel size for subjective
  criteria.
- **Test-set storage and identity.** How a `(dataset + prompt)` bundle is named,
  versioned, and stored on disk, and how the harness pins which one a run used so
  results stay attributable after a set is superseded. Output caching (below)
  keys partly on this — a cache entry is `(test-set, case, prompt, model,
  effort)`.
- **Where the harness lives and what it costs to run.** Whether it is an `opsctl`
  / `bin` tool, a Go test target, or a standalone command; whether it runs
  against live provider APIs every time or caches model outputs by
  `(test-set, case, prompt, model, effort)` so re-scoring is free. The sweep is a cartesian
  product over paid calls — output caching is likely load-bearing, not an
  optimization.
- **Saturation detection — when to mint the next generation.** A generation's
  value is gone once every config aces it. The design needs a rule for declaring
  saturation (e.g. top configs all above some score with no separation) so
  "build the next generation" is a triggered decision, not a vibe.
- **How a comparison becomes a decision.** Scores are inputs to a human choice,
  not an automated gate. The design should say how a run's matrix is presented so
  a person can actually pick — e.g. surface the dangerous-direction error
  (false-merge, fabrication) and the cost/latency alongside the headline score,
  rather than a single ranked number that hides the tradeoff the decision turns
  on.
