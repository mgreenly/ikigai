# Subject De-duplication ("Aliases") — Corrected-Scale Research

Status: research (feeds a design doc). Scope: manual, owner-approved subject de-duplication for the **wiki** service. Supersedes the scale framing, cost analysis, and timing recommendation of the prior draft of this file (SUSPECT — written under a false "personal wiki" scale premise); reuses only its scale-independent mechanics, re-verified against code.

---

## 1. Context & the corrected scale model

### Why "personal wiki" was wrong

The repo and several design docs call this a "personal wiki" and wave away cost ("fine for a personal wiki", `project/design/D07.md:19`). **That framing is empirically false and it poisoned the prior research pass.** The measured evidence:

- The live wiki holds **256 subjects produced from ~4 source documents** (paged via `mcp__ikigenba_wiki__subjects`: 100+100+56, then empty cursor) — roughly **64 subjects/document**.
- That 256 is **after** `normalize()` (`internal/wiki/data_model.go:104-114`: NFKC-fold → lowercase → single-space → diacritic-strip) already collapsed case/diacritic/whitespace variants. So 64/doc is the *irreducible, dup-prone residual* — exactly the input dedup operates on, not the raw mention count.
- Duplicates are **dense, not rare**: ~30+ obvious true-duplicate pairs are visible in those 256 subjects — acronym (GDW / Game Designers' Workshop; NIPI / New Infinities Productions, Inc.; TSR / Tactical Studies Rules), word-overlap (Rob Kuntz / Robert Kuntz; Hekaforge / Hekaforge Productions), near-edit (Lejendary Adventure / Lejendary Adventures). Dup density is on the order of **10%+ of subjects**.

A "several thousand document" lifetime therefore lands at **tens of thousands of subjects, plausibly 100k+**. **Every recommendation below is designed to hold at 100k+ subjects.** Any claim that leans on "the table is tiny" or "O(N²) is fine here" is rejected unless justified with real numbers.

### The projection-free principle

New-subject yield per document **decays** over the corpus's life (Heaps' law: V(n) ≈ K·nᵇ, b<1 — each new doc re-mentions already-known subjects more as the corpus saturates). The early ~64/doc is the *high-water mark*, not the steady state. Because b and K are topic-dependent and unknowable a priori, **no point projection of final N is defensible**. We therefore design **projection-free**:

1. Cost is paid **per actual subject creation**, never against a forecast.
2. Per-creation cost is bounded by the blocking **neighborhood** (acronym bucket, word-index postings, length-bucketed edit distance), **independent of total N** (modulo the hot-token leak, bounded in §3).
3. Detection runs at subject-creation time in the worker, so lifetime discovery cost ≈ **O(N) total**, not O(N²).
4. **The only number the system ever reads is `SELECT count(*) FROM subjects`** — used solely to *size the one-time backfill*. We do not store or design against any forecast.

### Cost table

The two cost components have entirely different shapes. **Compares are CPU-cheap when indexed; the LLM judge is the binding cost** (one network round-trip per surviving candidate pair).

| Model | N=10k | N=50k | N=200k | Verdict |
|---|---|---|---|---|
| **(a) Naïve all-pairs** compares = N(N−1)/2 | ~5.0e7 | ~1.25e9 | ~2.0e10 | Compares tolerable as raw CPU, but if any all-pairs variant feeds the **judge** even at 0.1% survival → ~50k / 1.25M / 20M LLM calls per pass = days-to-weeks, hundreds-to-thousands of dollars. **REJECT for recurring use.** |
| **(b) Indexed blocking, full re-scan** (the backfill shape) compares ≈ N × capped-neighborhood | ~5e5 | ~2.5e6 | ~1e7 | Sub-second to a few seconds CPU. Judge calls only on survivors. **This is the ONE-TIME backfill** — batched + parallel, off the steady-state path. |
| **(c) Per-new-subject at creation** (steady state) = O(neighborhood), flat in N | µs CPU; 0–3 judge calls | same | same | **The model the feature uses.** Lifetime ≈ O(N). |
| **(d) Per-doc batch of K newcomers** = K × (c), K decays via Heaps | small | small | small | Compares synchronous at mint; judge as post-commit follow-on. |

**No recurring O(N) or O(N²) sweep exists.** The backfill (model b) is the only potentially expensive, batched, parallelizable job, and it runs exactly once.

### Fixed decisions (design within these)

- **Durable alias.** A merge has FORWARD effect: after merging loser→winner, a future ingest mentioning the loser name resolves to the winner via a minimal `aliases` table feeding name resolution. Not retroactive-only.
- **No auto-merge.** Detection *proposes*; every merge needs explicit owner approval via MCP. (This directly rules out any server-side auto-approve-above-confidence threshold.)
- **Detection = indexed lexical blocking + an LLM-judge.** Vectors are not built (`D03:3,70` deferred them) and are not warranted here — see §3.

---

## 2. Detection architecture: at-creation, in the worker, post-commit

### The completeness property (proven against code)

`subjects.norm_name` is `TEXT NOT NULL UNIQUE` (`migrations/20260620185852_phase_02_data_model.sql:19`) and set **once at INSERT** in `plannedSubject` (`service.go:382-389`). `SubjectStore.Save` is a bare `INSERT INTO subjects (id, name, norm_name, type)` (`data_model.go:550-556`), not an upsert; grep confirms **there is no `UPDATE subjects` path anywhere** — the only subjects DML is that one INSERT. A subject's blocking keys derive purely from `norm_name`, so a subject's *matchability never changes after creation*.

**Therefore: for any true duplicate pair, the later-minted member is compared against the earlier (already-existing) member at its mint time, and the pair is caught then. No periodic sweep is needed to DISCOVER candidates** (subject to the lexical-recall caveat below — completeness covers *blocking*, not *judge decidability* and not *pairs lexical blocking never surfaces*). One-directional "newcomer vs everything older" querying is sufficient — exactly half the work of symmetric matching.

### Why not a sweep, why not the front door

- **Not a periodic sweep.** The prior doc proposed a background sweep + `dedup_jobs` queue + epoch bookkeeping. The completeness property makes the sweep's *discovery* role redundant, and a sweep recomputes O(N) (or O(N²) if it feeds the judge) every run, inheriting N into recurring cost. **Drop the sweep, the queue, and the epoch marker entirely.**
- **Not the front door.** Front-door ingest is deliberately LLM-free fire-and-return (`D04:5`); the subject is not even minted/committed until the worker's `integrate` finishes (`service.go:298`). There is literally nothing to compare against at the front door. The prior doc conflated "front-door ingest time" with "subject-creation time" and wrongly rejected both.

### The two-stage hook (lexical at mint, judge as follow-on)

Both stages live on the single worker goroutine (`worker.go:19-48`, one-at-a-time `ProcessNext`/`Wait`), both off the request path:

- **Stage 1 — lexical blocking, in-memory, no LLM.** When `plannedSubject` decides a subject is new (`service.go:382`), the newcomer is accumulated in `plan.newSubjects`. Generate candidate pairs by probing the in-memory block index (§3). The candidate pool for each newcomer is the **union of (a) the committed blocking neighborhood and (b) the other newcomers in this same job** (the K-at-once case below).
- **Stage 2 — LLM judge, strictly post-commit follow-on.** After `integrate`'s tx commits (`service.go:298`), take the job's new-subject id set (carried out of `integrate` — see "carrier" note below), run blocking, and issue one `llm.JSON` judge call per surviving pair. Persist each verdict into a `merge_candidates` table (proposed, never auto-merged). Because this runs after the commit and after pages are already available, a slow/crashing judge degrades to "candidate not yet proposed" — never to "page unavailable" or "integrate failed".

**The judge must never run inside the integrate tx** — an LLM call inside the write tx would serialize all writes behind a multi-second network call, exactly what D04/D14 forbid. (`integrate` already runs Extract/Compile *before* `s.write.BeginTx` at `service.go:234,239`; "compile/LLM outside the tx" is the established idiom, not a new requirement.)

**Carrier note:** `claims.SubjectIDsByJob(job.ID)` returns *all* subjects a job touched (new + affected), not just new ones. The new-subject id set therefore is **not** recoverable from the DB post-commit; it lives only in `plan.newSubjects` inside `integrate`. The design must explicitly thread `plan.newSubjects` out of `integrate` to the post-commit follow-on (e.g. as a return value or a struct field on the in-flight job record), not re-derive it.

### The K-new-subjects-per-doc case

If one document mints both `IBM` and `International Business Machines` as new subjects in the same job, and the candidate pool is only the *committed* set, neither sees the other and the pair is **missed forever** (nothing re-mints them). Stage 1 MUST union `plan.newSubjects` into each newcomer's candidate pool. Exact-norm duplicates within one job already collapse via the `known` map (`service.go:371`), so they are never self-paired.

### The "evidence arrives later" gap — TWO distinct cases, only one closed by re-judge

Completeness guarantees a pair is *flagged at blocking time*; it does **not** guarantee the judge can *decide*, and it does **not** guarantee lexical blocking *surfaces every true pair*. These are two different holes:

- **Case (a): BLOCKED but UNDECIDED (closed).** A brand-new subject minted from a one-line doc has thin/zero claims, so a true pair (thin `JFK` vs rich `John F. Kennedy`) *is* lexically flagged (via the acronym map) but the judge returns low-confidence/undecided until the thin side gains claims. This is closed **without a sweep** by piggybacking on the existing recompile. `integrate` already recompiles every subject in the job's `affected` set (`service.go:315,346-347`, D07) whenever its claims change. That recompile *is* the "evidence changed" signal. So Stage 2 has two triggers, both bounded by *affected-subjects × neighborhood*:
  - **DETECT-NEW:** newly-minted subjects this job.
  - **RE-JUDGE-ENRICHED:** existing affected subjects holding an OPEN/undecided/low-confidence candidate whose per-side **content fingerprint** moved (claim-set hash). Re-qualify exactly when the fingerprint changes.

- **Case (b): NEVER BLOCKED (NOT closed — accepted recall limit).** If a true pair shares no rare token, no matching initials, and a large edit distance (the canonical nickname case `JFK`↔`John F. Kennedy` where no acronym is derivable from the long form's leading letters, or `Bill`↔`William`), lexical blocking never surfaces it, so it is never an OPEN candidate and RE-JUDGE-ENRICHED can never fire for it. **This pair is lost with no sweep to recover it.** This is an *accepted recall limitation of lexical-only blocking*, not a defect to patch with a sweep (a sweep would reintroduce recurring O(N²)/judge cost). Two future remedies fit the architecture, both deferred: a small curated nickname/abbreviation dictionary feeding the blocking keys (see §3), and an **extract-time in-document co-reference hint** that pre-seeds a candidate when the source document itself glosses the variant (see §10 Q12) — both produce a *candidate*, never a blind collapse. The design doc must state case (b) plainly and not overclaim completeness.

The only legitimate non-creation, non-enrichment trigger is **after a MERGE** (see §6): a merge changes future name resolution and can make the winner newly comparable to subjects it was never paired with, so re-judge the winner's neighborhood as post-merge follow-on.

---

## 3. Indexed blocking at scale

A single in-memory `blockIndex` (new package `internal/dedup`), owned by the worker, built once on boot from `SELECT id, name, norm_name, type FROM subjects` (one O(N) read) and mutated incrementally **on the single goroutine, post-commit** (mint adds postings; merge removes loser postings; delete removes postings). No locking is needed because all mutation and all queries are on the one worker goroutine. The table is the source of truth; on any drift, fail loud and rebuild on next boot.

**Boot-build cost (honest figure):** the `SELECT` itself is fast (the `subjects(name,id)` index from `20260622075642` supports the scan), but building `byInitials` + token postings + length buckets in Go over 200k rows is realistically **sub-second to low-single-digit seconds**, not "tens of ms". This is an **O(N) cost paid ONCE at startup** (like any cache warm-up) and introduces no recurring N-dependence; size it as such rather than asserting an optimistic figure.

All keys derive from `normalize()` output (`data_model.go:104-114`), so the index is a deterministic pure function of the subject set.

### The indexes (per-creation neighborhood bound)

Each maps a key → slice of `{id, normName, type}` refs; per-creation cost is the sum of four probes, each O(neighborhood) **after the hot-token guard below**, not O(N):

1. **Token inverted index** `map[string][]ref` — keyed on each norm_name token after dropping a small stopword/legal-suffix set (the, of, and, inc, corp, ltd, llc, co, company). Query unions postings for the newcomer's **rare** tokens (df below threshold), scores by **idf-weighted Jaccard**, keeps ≥ ~0.4. Catches "John F. Kennedy"↔"John Kennedy", "Acme Corp"↔"Acme Corporation".
2. **Acronym/initialism map** — `byInitials map[string][]ref` keyed by first-letters of each token ("international business machines"→"ibm"); a single-token spaceless name is itself a candidate acronym. Two O(1)-ish lookups catch the long form when "ibm" is minted and vice-versa. **This is where lexical wins and embeddings lose — keep it mandatory.**
3. **Length × first-char edit-distance buckets** `map[bucketKey][]ref` — scan lengths len−2..len+2, run banded Levenshtein (early-exit at >2). Bucketing keeps the pool to near-equal-length names; without it this rule is the O(N) trap.
4. **Substring / containment** — covered by rule 1's Jaccard for whole-word containment. A char-trigram map is the heaviest in memory (~30-50 MB at 200k) — **defer it**; add only if a gold-set test shows real misses.

Merge all rule outputs into one set keyed by the sorted (id_lo, id_hi) pair, dedup, apply caps, then emit to the judge.

### Hot-token caps — the one place N re-enters per-event cost

A frequent token ("john", "inc", "system") or hot acronym ("TSR") grows a posting list of O(fraction of N): at 200k, "john" might be 2,000–6,000 entries. **Two distinct costs leak here, and both must be capped at LOOKUP time, not scoring time:**

- **Judge-call flood** (the expensive leak): joining on a hot token floods the judge with thousands of pairs per newcomer.
- **Union/scoring CPU** (the cheaper but real leak): even before the per-creation candidate cap trims to ~20, *materializing and scoring* a 6,000-entry posting list is O(fraction of N) CPU per mint. Microseconds-to-low-milliseconds in absolute terms, but it IS N-dependent and must not be hidden behind the candidate cap.

**Mandatory mitigation (apply at posting-LOOKUP, so the hot postings are never materialized):**
- **Document-frequency ceiling:** a token with df > max(50, 0.005·N) (≈1,000 at 200k) becomes a *query stopword* — the index **skips its posting list entirely at lookup time** (never reads/unions/scores it). Candidates must be supported by ≥1 *rare* shared token, so "John Smith"↔"John Doe" never pairs on "john" alone, and the 6,000-entry list is never walked. This is what keeps both leaks bounded.
- **Hard per-creation candidate cap** (~20): if more survive (from rare-token unions), keep the highest idf-weighted-Jaccard / lowest-edit-distance and drop the rest. A missed dup is recoverable (cheap error); a flood of junk judge calls is the trap.

Tolerable judged pairs per creation: **~5–8 steady-state, 20 ceiling.** Thousands (unguarded hot token) is unacceptable. With df-ceiling-at-lookup, neither the judge count nor the union CPU scales with N.

### Memory & lifecycle

At 200k subjects (~20-char names, ~2-3 tokens): token index ~30-60 MB, acronym map a few MB, edit-distance buckets small; **total ~50-90 MB** — negligible next to the SQLite page cache, comfortably in-process.

The steady-state worker and the one-time backfill (§5) each build their **own** in-memory index in their **own** process; they are independent. The backfill (a separate process) does not mutate the worker's index, so the worker does not see backfill-era candidate writes in its index until next boot — acceptable, because the backfill operates on the pre-feature corpus exactly once and writes only to `merge_candidates`, which both processes read from the DB.

### Vectors / ANN — reconsidered and rejected

**Do not build ANN/vector blocking for this.** ANN's sublinear search only pays off when the linear alternative is expensive, but the lexical indexes are already O(neighborhood) and sub-millisecond. ANN would add an embedding call-site, a full-corpus `page_vectors` backfill (`D03:70`, deliberately deferred), and an index to maintain — while (a) **structurally missing the acronym case** (IBM↔International Business Machines have near-zero lexical overlap and 2-3-word embeddings are noisy) and (b) **not reducing the binding judge cost at all** (every candidate it surfaces still needs one judge call). Vectors are a recall tool for a precision-bound, judge-bound problem. Reconsider only on a *measured* gold-set miss of pairs that share no tokens, no initials, and large edit distance (the case (b) nickname class) — and even then a small curated nickname/abbreviation dictionary feeding the blocking keys is cheaper and more precise.

---

## 4. The LLM judge

### The judge is the binding cost

Compares are CPU-microseconds; each surviving pair is one network LLM round-trip (~0.5–2 s, a fraction of a cent, recorded in `llm_calls` per D13). Steady-state load is tiny and self-limiting (Heaps decay + thinning neighborhoods as obvious dups get merged). **The only large load is the one-time backfill** (§5).

### BLOCKER prerequisite (must be the feature's first phase)

`llm_calls.stage` has `CHECK (stage IN ('extract','compile','ask'))` (`20260622071935_llm_calls.sql:8`, confirmed). The production recorder is wired (`cmd/wiki/main.go:55`), so a `stage='dedup_judge'` INSERT **fails the CHECK loudly** on every judge call. Ship a timestamped migration (`bin/new-migration wiki widen_llm_calls_stage`) widening the CHECK to include `'dedup_judge'` before any judge runs.

### Steady-state vs backfill execution modes

| | Steady-state (worker, post-commit) | Backfill (off-worker, one-time) |
|---|---|---|
| Granularity | **One pair per call** (pin-testable; Go owns the yes/no gate) | **Batched, K=10–25 pairs/call** (amortizes per-call overhead ~K×) |
| Model | `ModelID` = Sonnet 4.6 (`wiki.go:24`), Temp 0, `DisableReasoning()`, `MaxParseRetries 2` — mirrors `compile.DefaultCallSite` | **Opus 4.8 + reasoning high** allowed (mirrors `eval/judge.go:25-26`: `anthropic.ModelOpus48` + `agentkit.Level("high")`) — a false merge here is the most destructive error |
| Concurrency | single worker goroutine | bounded pool (e.g. G=4–16); only provider calls run concurrently — all `merge_candidates` writes funnel through **one** SQLite writer |

Precedent for both shapes is in-repo: `internal/eval/judge.go` + `eval.go:273-323` already runs a batched, index-validated LLM-judge with `validateJudgeVerdict`.

### Precision-first rubric (the error is asymmetric)

A false `same:true` is doubly destructive: it collapses two real subjects **and** writes a forward-acting alias that mis-routes all future ingests of the loser name. A missed merge merely leaves two pages (status quo). So:
- `same:true` **only** for identical real-world referents under different names/spellings.
- `same:false` for namesakes, part-vs-whole, merely-related, concept-vs-instance, **and whenever evidence is not clearly sufficient**.
- A **confidence floor** (drop below `medium`, config knob) keeps low-signal verdicts out of the owner's queue and the re-judge surface.

Evidence per subject: `name`, `type`, the compiled page **lead** (~600 chars), and up to ~5 salient claim bodies — **not** raw source docs, **not** both full 12k pages. Note `ClaimStore.Save` inserts exactly id/subject_id/job_id/body (`data_model.go:672-676`); the richer extracted `kind`/`occurred_at` (`extract.go:13-20`) are **never persisted**, so there is no per-claim kind/occurred_at evidence to feed the judge.

**The judge's evidence is impoverished relative to what we briefly held.** The single strongest disambiguation signal for a pair like `GDW`↔`Game Designers' Workshop` is the *source document* that glossed them together ("GDW (Game Designers' Workshop) published Traveller") — an explicit in-document co-reference. Extract holds that document; by judge time it is gone, digested into separate page leads and claims. This is the kernel of truth in the rejected "front-door canonicalizer" idea: any canonicalizer that could actually resolve `GDW` correctly would need *inference over the document context*, at which point it is no longer a deterministic form function but **entity linking** — i.e. the judge, relocated to extract. The disposition (capture in-document co-reference as alias *hints* feeding this judge, vs. leaving the judge on digested evidence) is **Open Q12**.

Give the judge prompt the same offline gold-set + eval treatment the extract prompt got (commits 2751e6f / 4d09a2b) **before** it becomes default.

### Candidate-judge prompt draft (single-pair, steady state)

```
System: You decide whether two wiki subjects refer to the SAME real-world entity.
The cost of a wrong YES is high: it permanently merges two pages and re-routes all
future mentions of the loser name. When in doubt, answer NO.

Subject A — name: "{a.name}"  type: {a.type}
  page lead: {a.lead}
  claims: {a.claims[:5]}

Subject B — name: "{b.name}"  type: {b.type}
  page lead: {b.lead}
  claims: {b.claims[:5]}

Answer YES only if A and B are the SAME entity under different names/spellings
(e.g. an acronym and its expansion, a spelling variant, a full vs short name).
Answer NO for: different people who share a name; a part vs the whole; merely
related or associated entities; a concept vs an instance; or if the evidence is
not clearly sufficient.

Output JSON: {"same": bool, "confidence": "low"|"medium"|"high", "reason": "<=1 sentence"}
```

`validateResponse` asserts `confidence ∈ {low,medium,high}` and `same==true ⇒ non-empty reason` (mirrors the non-empty checks in `compile.go:92-103` / `extract.go:139`). The batch variant returns `{"verdicts":[{"pair_index":n,"same":...,"confidence":...,"reason":...}]}` with strict index-coverage validation (the `eval.go:323` pattern).

**Persist every decided pair** (approved / dismissed / below-floor), keyed on the unordered norm-name pair + per-side content fingerprint, so it is **never re-judged** until evidence moves — this is what keeps lifetime cost ≈ O(N).

---

## 5. The one-time backfill migration

The at-creation model only catches *future* subjects. The pre-existing corpus must be swept **once** to find candidates among subjects created before the feature shipped.

### Home: a standalone CLI binary, NOT an appkit verb, NOT automatic

The appkit dispatcher is a **closed verb set** — `serve|version|manifest|migrate|schema`, default-case error on anything else (`appkit/appkit.go:215-224`, in the *sibling* `appkit` module wired via a committed `replace`). So the backfill cannot be a new appkit verb or `opsctl backfill`. Ship it as **`cmd/wiki-dedup-backfill/main.go`** in the wiki module, mirroring the `cmd/eval-extract/main.go` precedent (off-serve `llm.Client`, own flags, `ANTHROPIC_API_KEY`). It ships in the same static artifact, so it's already on the box after deploy. (That appkit is a separate module reinforces this: the backfill genuinely cannot bolt onto the dispatcher and must live in `cmd/` inside wiki.)

- **NOT automatic on first deploy:** `opsctl deploy` runs migrate + atomic swap + restart in a downtime window; injecting a multi-hour/day LLM sweep would hang the deploy and burn unbounded API cost unattended — violating the suite's short-scheduled-downtime bet. It is an **explicit operator trigger**, run after the feature is live, watched by a human.
- **NOT on the steady-state worker:** funneling the whole corpus through the single ingest worker freezes live ingestion for hours-to-days and gives no judge parallelism. Run it as a **separate process** — it contends only briefly for the SQLite write lock per INSERT, never freezing ingestion.

### Operation shape

1. Open the deployed DB (write handle + read pool, `db.OpenRead` as `main.go:50`); build the same `llm.Client`. Reuse the exact `internal/dedup` Stage-1/Stage-2 engine the steady-state path uses — the backfill is just a different *driver*.
2. Load the subjects snapshot once; build the in-memory block indexes (with hot-token caps applied at lookup).
3. Walk subjects in **ULID order** from a persisted high-water cursor (`backfill_state(cursor_subject_id, started_at, judged_count, updated_at)`). For each, emit its blocked neighborhood as sorted (id_lo, id_hi) pairs, de-duped against existing `merge_candidates` / dismissals / merges.
4. Fan out the **batched** judge across G goroutines (reads are concurrent); every verdict above the floor is sent to **one writer goroutine** doing `INSERT OR IGNORE INTO merge_candidates(...)`. **Advance the cursor only after a subject's whole neighborhood is judged+written** — this is the primary crash-safety guard; the unordered-pair UNIQUE + INSERT OR IGNORE is the idempotency backstop.
5. Observe: print `count(*)` → estimated pairs/cost/ETA and require confirmation; enforce a `--budget` / `--max-judge-calls` ceiling that stops cleanly (cursor persists); log progress; rely on `llm_calls` rows with `stage='dedup_judge'` as the authoritative cost ledger.

### Sizing

The **only** number needed is `SELECT count(*) FROM subjects` at run time. With indexed blocking + caps yielding ~0.5–2 candidate pairs/subject: ~5k–20k judge calls at 10k subjects, ~25k–100k at 50k, ~100k–400k at 200k — multi-hour to multi-day **serially**, which is exactly why batching (K=10–25 → ~K× fewer calls) + parallelism (G=8 → hours not days) are mandatory. Do **not** forecast future growth; size to the corpus as it stands.

### Prerequisites

The widen-`llm_calls`-CHECK migration (§4) and the `merge_candidates` + `backfill_state` tables (with a UNIQUE on the sorted pair) must exist before the backfill runs.

---

## 6. The merge operation: data ops & schema

Merge is **one all-or-nothing write tx on the single-writer** (`s.write.BeginTx`, `service.go:239`), run as a **work item on the single worker goroutine** (so it cannot interleave with `integrate` — see the race below). It reuses the **existing D07 `Compile`; there is no new merge prompt.**

### Merge-queue mechanism (the load-bearing race fix — must be specified, not assumed)

The race mitigation rests entirely on merges executing on the *same single goroutine* as `integrate`, but the worker loop today drains **only ingest jobs** via the generic `Service.ProcessNext`/`Wait` interface (`worker.go:9-48`). There is no second work-item type. The design MUST specify the concrete enqueue/dispatch mechanism. Recommended (lowest-friction, matches existing machinery): **reuse the `jobs` table with a `kind` discriminator** (`ingest` vs `merge`), have `ProcessNext` branch on `kind` to either `integrate` or `mergeSubjects`, and have the `merge` MCP verb (§7) INSERT a `kind='merge'` job carrying `pair_id`/winner-override. This keeps one queue, one writer, one `RequeueWorking` boot-recovery path (`worker.go:25-28`), and one status-guard idiom. A separate parallel queue is the alternative but duplicates all of that. Pick one in the design; do not leave it as "a work item on the worker".

### Three phases (mirroring integrate)

**Phase A — resolve (no tx).** Resolve winner & loser **by subject ID** carried on the merge work item (from `pair_id`) via `SubjectStore.Get` (indexed PK, `data_model.go:570`). **Do NOT route the common path through `GetByPath`** — it is an O(N) type-partition scan that computes `slug()` in Go for every row (`data_model.go:584-606`, no slug index); at 100k entities every merge would scan ~100k rows. Reserve `GetByPath` only for an explicit `winner` type/slug override, wrapping `ErrAmbiguousPath` into a clean `toolError`. Validate in Go: both exist, `winner.ID != loser.ID`; if the chosen winner is itself an alias loser, follow the alias chain to the terminal winner.

**Phase B — recompile (no tx, LLM).** Read loser's + winner's claims, union in memory, call `compiler.Compile(ctx, winnerSubject, combinedClaims)` — `Compile` is a **3-arg method** `Compile(ctx, s wiki.Subject, claims []wiki.Claim)` (`compile.go:46`); it takes identity + the complete claim set only and runs its own internal tighten/retry loop (the `tighten` hint is an internal `renderPrompt` parameter at `compile.go:56,71,105`, never a caller argument and never a prior body). Claim IDs are stable across merge (merge only repoints `claims.subject_id`), so any `[claim.ID]` citation the model chooses to emit (`compile.go:128` injects them into the prompt input) remains valid — note citation *rendering* is an LLM/prompt behavior, not a code-enforced invariant (`validateResponse` only checks non-empty title/body, `compile.go:92-103`). `Compile` structurally caps the body at `utf8.RuneCountInString ≤ PageCharCap=12000` (truncating at `compile.go:84`), and the `pages.body CHECK(length(body) <= 12000)` (`migration:42`) counts the **same unit** — SQLite `length()` on TEXT counts characters/code points, not bytes. **The compile cap therefore already satisfies the CHECK; there is no byte-vs-rune mismatch and no multibyte-rollback hazard, so no extra safeguard is needed.**

**Phase C — one tx on the write handle.** Construct **tx-bound stores** `NewSubjectStore(tx)/NewClaimStore(tx)/NewPageStore(tx)/NewAliasStore(tx)` (never the read-pool `s.*` at `service.go:71-73`; mirrors the tx-bound construction at `service.go:253-255`). Re-check the work-item status guard (like `service.go:246-251`). Then, in **strict order**:

1. `claims.RepointSubject(loser → winner)` *(net-new — no `ClaimStore.RepointSubject` exists today)*
2. `pages.Upsert(winner page)` *(existing)*
3. `pages.DeleteBySubject(loser)` *(existing, `data_model.go:809`)*
4. `subjects.Delete(loser)` *(net-new — no `SubjectStore.Delete` exists today) — MUST precede the alias insert (frees the shared norm_name)*
5. `aliases.RepointSubject(loser → winner)` *(net-new; path-compress chained aliases)*
6. `aliases.Insert(norm_name=loser.NormName, subject_id=winner.ID, name=loser.Name, created_by, created_at)` *(net-new — whole `AliasStore` is new)*
7. Insert tombstone/audit row (loser id/name/type, winner id, moved claim ids, ts)
8. Update merge work-item → done; `tx.Commit()`. Any error → full rollback.

The **delete-loser-before-alias-insert order is load-bearing**: `subjects.norm_name` UNIQUE and `aliases.norm_name` UNIQUE share the same value; inserting the alias first is a UNIQUE violation. (Net-new store methods to size in the plan: `SubjectStore.Delete`, `ClaimStore.RepointSubject`, and the entire `AliasStore` — `Insert`/`RepointSubject`/`GetByNormName`.)

### The aliases schema (one timestamped migration: `bin/new-migration wiki create_aliases`)

No FOREIGN KEY; UNIQUE/CHECK only; TEXT ULID-friendly ids; closed-set fields validated in Go:

```sql
CREATE TABLE aliases (
  norm_name  TEXT NOT NULL UNIQUE,   -- normalize(loser name); the resolution key
  subject_id TEXT NOT NULL,          -- winner subjects.id, by convention (no FK)
  name       TEXT NOT NULL,          -- loser display name at merge time
  created_by TEXT NOT NULL,
  created_at TEXT NOT NULL           -- RFC3339Nano, matching formatTime (data_model.go:709)
);
CREATE INDEX aliases_subject ON aliases (subject_id);  -- path-compress step 5
```

### The resolveByName hot-path hook — O(1)/O(log n), no LLM, no scan

Add a shared `resolveByName(ctx, name)` helper. Because `ask` lives in a **separate package** (`internal/ask`) and holds its own `*wiki.SubjectStore` (`ask/ask.go:44`) calling `GetByNormName` directly (`ask/ask.go:157`) while bypassing `plannedSubject` entirely, the helper must be a **wiki-package function or a `SubjectStore` method** that both callers consume — otherwise `ask` silently loses the durable forward-alias effect. It is consulted **before mint** in `plannedSubject` (between the `GetByNormName` miss at `service.go:379` and mint at `:382`) **and** by `ask`'s `gatherPages` (`ask/ask.go:142,157`):

1. `GetByNormName(name)` — indexed UNIQUE point lookup, O(log n). On hit, return it.
2. On `sql.ErrNoRows`, query `aliases WHERE norm_name = normalize(name)` — a second indexed UNIQUE point lookup, **O(log n) at any N** (no LLM, no scan).
3. On alias hit, load the winner via `Get(subject_id)`, follow the alias chain to the terminal winner (depth-bounded; fail-loud on cycle/dangling target — no FK guards it), populate the in-job `known` map (`service.go:371`).

**Resolution order is subjects-first-then-aliases** (a re-minted live subject shadows an old alias; reclaiming a loser name as a standalone subject requires deleting the alias row first). This is the **only** change on the ingest/ask hot path, and it is provably O(log n) — the prior doc's "hot path" concern is fully answered.

---

## 7. MCP UX: the tool surface & approval flow

Add **four verbs** (13→17), each cloning the established D16 idiom: a function field on `Handler` (`mcp.go:22-37`), a `WithXxxService[T]` Option (`mcp.go:148-204`), a `handleToolCall` case (`mcp.go:321-352`), a `handleXxxCall`, and a conditional `tools()` append (`mcp.go:672-708`). **All subject references cross as type/slug paths, never ULIDs; the pair is keyed by a server-minted opaque `pair_id` (mirroring `job_id`).** Errors use the two-channel convention strictly: unknown `pair_id` → `notFound("merge_candidate", pair_id)` whole-result envelope (`mcp.go:1263`); malformed input → `toolError` (`mcp.go:1325`). Owner identity from `appkit.IdentityFrom(ctx)` on write verbs.

1. **`merge_candidates`** (browse, paginated, READ). `inputSchema = listSchema({confidence: array<enum low|medium|high>})` (auto-adds limit/cursor). `confidence` is a **match-any string array over a closed enum**, modeled on `jobStatusArraySchema`/`validJobStatuses` (`mcp.go:800-808,970-1009`) — not a numeric threshold (the codebase has no ordinal-threshold pattern). The handler maps to `paging.Params` and calls `CandidateStore.ListOpen` doing a **pure keyset SQL read** of `merge_candidates WHERE status='open'` `ORDER BY confidence DESC, created_at, pair_id LIMIT limit+1` — never compute/judge in the read, so a thousands-deep backlog paginates in O(limit) (≤200 rows/call per `page.go:11-12`). Each item carries enough to triage without a second round-trip:
   `{ pair_id, confidence, rationale, evidence_stale, suggested_winner: "type/slug", suggested_loser: "type/slug", subjects: { <path>: {path, type, name, claim_count, has_page}, ... } }`.
   Deep inspection uses the **existing** `page`/`claims` verbs on the two paths. Returns `pagedResult("candidates", items, next_cursor)`. **Stale-reference rule:** skip (or `status='superseded'`) any open candidate whose subject was deleted by an unrelated merge — see the chained-merge case below.
2. **`merge`** (approve, WRITE, **fire-and-return**). `objectSchema({pair_id, winner?}, [pair_id])`. The actual merge runs on the single worker (LLM recompile off the request path, like ingest) via the `kind='merge'` job (§6), so it returns a **pending handle** `{pair_id, status:"queued"|"already_merged"|"dismissed"}` — **never** `status:"merged"` (completion is async; observed by the pair dropping off `merge_candidates` or via `merges`). A `winner` override must equal one of the pair's two paths; resolve via `GetByPath`, wrapping `ErrAmbiguousPath` → `toolError` (net-new code — no existing handler wraps it). Idempotent re-approve.
3. **`merge_dismiss`** (reject / remembered-no, WRITE, idempotent). `objectSchema({pair_id, reason?}, [pair_id])`. Records `created_by` + reason on a dismissal row **keyed by the norm-name pair + content fingerprint**, so detection re-proposes only when evidence moves. Returns `{pair_id, dismissed:true}`.
4. **`merges`** (audit read-back, paginated, READ). Keyset-paginates tombstone rows newest-first: `{ merged_at, winner: "type/slug", loser_name, loser_type, alias_norm_name, moved_claim_count, created_by }`. Closes the irreversible-write → visible-audit loop (un-merge deferred).

**Chained / transitive candidates under concurrent merges.** If A↔B and A↔C are both OPEN and the owner merges A→B (A deleted), the OPEN A↔C candidate now references a deleted subject. Rule: at **browse** time skip/supersede candidates whose subject no longer exists, and at **approve** time resolve each side through `resolveByName`/the alias chain to its terminal subject before merging (so an approve of A↔C re-targets B↔C, or no-ops if already merged). Without this, the queue surfaces dangling candidates that fail at approve.

**Handling a large post-backfill backlog (the scale crux):** confidence-sorting puts precision-safe high-confidence pairs first so the agent processes the top and can stop; bulk approval is **N idempotent `merge` calls in a loop**, NOT a batch mega-verb (D16 explicitly rejected the `job <action>` mega-verb). **No auto-approve-above-confidence tool** — it violates the fixed no-auto-merge decision; the safe equivalent is agent-side loop-approval of the high-confidence page, human in the loop per call. Document the cursor-stability contract: order `(confidence DESC, created_at, pair_id)`; a concurrent backfill inserting higher-confidence pairs means an in-flight cursor walks a best-effort snapshot, so a fresh first-page fetch surfaces new top-of-queue pairs.

**Bad-merge / bad-alias escape hatch (open gap).** v1 defers both un-merge AND alias-delete, and `merge_dismiss` only touches OPEN candidates, never committed aliases. **A mistaken high-confidence merge is therefore unrecoverable through the v1 tool surface** — its forward-routing alias keeps mis-resolving future ingests with no operator escape. The human must either accept this for v1 or pull a minimal `alias_delete` verb (delete the `aliases` row by `norm_name`; does NOT un-merge claims/pages) into scope. Flag explicitly; do not imply recoverability via the tombstone alone.

---

## 8. Edge cases & risks

| Failure / risk | Mitigation | v1 / deferred |
|---|---|---|
| **Hot-token bucket flood** → single mint spawns thousands of judge calls AND scans an O(fraction-of-N) posting list | DF ceiling applied **at lookup** (skip the posting list entirely above max(50, 0.005·N)), require ≥1 rare supporting token, hard per-creation cap ~20 | **v1, mandatory** |
| **Intra-doc pair missed** (IBM + Int'l Business Machines both new in one job) | Union `plan.newSubjects` into each newcomer's candidate pool | **v1** |
| **New-subject id set not recoverable post-commit** (`SubjectIDsByJob` returns new+affected) | Thread `plan.newSubjects` out of `integrate` to the post-commit follow-on (explicit carrier) | **v1** |
| **Evidence-arrives-later, case (a): BLOCKED but undecided** (thin newcomer) | RE-JUDGE-ENRICHED on the `affected` set when content fingerprint moves (no sweep) | **v1** |
| **Evidence-arrives-later, case (b): NEVER blocked** (lexically-thin true pair, e.g. JFK↔John F. Kennedy nickname class) | **Accepted recall limitation of lexical-only blocking.** No sweep. Future remedy = curated nickname/abbrev dictionary feeding blocking keys | **deferred / accept** |
| **Merge ⇄ integrate race** (planIntegration holds no tx; `service.go:314-367`) could resurrect a deleted loser via Upsert | Run merge as a `kind='merge'` work item on the **single** worker goroutine (concrete queue mechanism in §6); compile outside the tx | **v1, load-bearing** |
| **`llm_calls.stage` CHECK** rejects `dedup_judge` (`20260622071935:8`) → every judge call fails loudly | Widen-CHECK migration as the feature's first phase | **v1, blocker** |
| **`GetByPath` O(N) scan** on merge resolution at 100k+ (`data_model.go:584-606`) | Resolve by subject ID (Get/PK); `GetByPath` only for winner override | **v1** |
| **Writing through read-pool stores** silently bypasses the merge tx | Construct `NewXxxStore(tx)` inside the tx (as `service.go:253-255`) | **v1** |
| **Dangling alias.subject_id** after chained merge (no FK) | `aliases.RepointSubject` in-tx + resolveByName validate-live/fail-loud + depth-bounded chain follow | **v1** |
| **Chained/transitive OPEN candidate** references a subject deleted by an unrelated merge | Browse skips/supersedes dead-subject candidates; approve resolves each side through the alias chain to its terminal subject first | **v1** |
| **Boot crash-recovery for un-judged newcomers** | Needs a per-subject `judged_through` watermark — NOT a naive anti-join (see Open Q6) | **v1, but redesigned — see Open Q6** |
| **Backfill not idempotent** → double-inserts / re-judges on resume | UNIQUE on sorted pair + INSERT OR IGNORE; cursor advances only after a subject's neighborhood is fully written | **v1** |
| **Wrong high-confidence merge** is destructive + forward-propagating, and **unrecoverable in v1** | Precision-first rubric + confidence floor + overridable winner + owner approval; tombstone records moved claim ids. Un-merge AND alias-delete both deferred → flag irreversibility to human (§7) | **v1 detect / recovery deferred** |
| **Dismissed-then-enriched** pair buried forever | Content-fingerprint-keyed dismissals re-qualify on evidence change | **v1** |
| **Un-merge needs moved-claim-id list, but that list is unbounded** for a claim-rich hot loser (D07 unbounded accumulation) → large single audit row | Bound it, or split moved-claim ids into a separate `merge_moved_claims` audit table rather than a blob column | **deferred (with un-merge); flag the sizing now** |
| ~~**Rune-vs-byte 12k cap mismatch**~~ | **STRUCK — non-issue.** SQLite `length()` on TEXT counts code points; `Compile` caps at 12000 runes (`compile.go:84`); the CHECK counts the same unit. Compile cannot trip the CHECK. No safeguard needed. | **n/a** |
| **occurred_at / kind "conflict" in merge** | Non-issue: `ClaimStore.Save` persists only id/subject_id/job_id/body (`data_model.go:672-676`); kind/occurred_at extracted but never stored; type is first-writer-wins | **n/a** |
| **D07 per-subject claim accumulation** (scope-adjacent): hot subjects accumulate claims forever; full recompile reads ALL claims every touch, truncates at 12k lossily; dedup *concentrates* claims onto winners → 12k truncation more likely on merged hot pages | Document the interaction; do not try to fix in this feature | **deferred / flag only** |
| **D12 `PageWithLinks` is O(N) per page read** (`internal/wiki/links.go:103-130`: `listAllSubjects` full scan + per-subject `GetBySubject`) — unusable at 50k+ **independent of dedup**; merged loser links go dead with no recompute trigger | **Flag LOUDLY as a separate must-fix.** Dedup must NOT add per-read alias-following to this already-hot path | **deferred / out of scope, raise to human** |

---

## 9. Recommended path forward

The four questions the overall research must answer, as numbered decisions:

1. **Detection** — Detect at **subject-creation time, inside the single worker, post-commit**, in two stages: cheap indexed lexical **blocking at mint** (Stage 1, in-memory `internal/dedup` block index) and the **LLM judge as post-commit follow-on** (Stage 2, fed by the `plan.newSubjects` carrier). Triggers, all *affected-subjects × neighborhood* bounded: **DETECT-NEW** (newly minted), **RE-JUDGE-ENRICHED** (existing affected subject whose content fingerprint moved, closes evidence-later case (a)), **POST-MERGE** (winner's neighborhood). **No periodic sweep, no front-door detection, no `dedup_jobs` queue/epoch.** Completeness (for *blocking*) is guaranteed by `norm_name` immutability (INSERT-only, no UPDATE path); the lexically-never-blocked case (b) is an **accepted recall limit**. Blocking is **mandatory indexed** (idf-weighted token inverted-index, acronym map, length-bucketed edit distance), with **DF stopword ceiling applied at lookup + per-creation candidate cap** to bound hot tokens in BOTH judge-count and union-CPU. **Vectors/ANN rejected.**

2. **The judge** — One `llm.JSON` call per pair in steady state (Sonnet 4.6, temp 0, reasoning off, recorded in `llm_calls`); **batched + parallel for the backfill** (Opus 4.8 high-reasoning permitted there). **Precision-first rubric** (asymmetric false-merge cost) + **confidence floor**. Persist every decided pair keyed on norm-name-pair + content fingerprint so nothing is re-judged until evidence moves. Gold-set-eval the prompt before it becomes default. **First phase ships the `llm_calls.stage` CHECK widening migration** (hard blocker).

3. **The merge** — One all-or-nothing tx on the single worker, entered via a concrete **`kind='merge'` job on the existing `jobs` queue** (the race fix, specified, not assumed); resolve by **subject ID** (not `GetByPath`); recompile the winner with the **existing 3-arg D07 `Compile` (no new merge prompt)** outside the tx; strict in-tx order (repoint claims → upsert winner page → delete loser page → delete loser subject → repoint aliases → insert alias → tombstone). Net-new store methods: `SubjectStore.Delete`, `ClaimStore.RepointSubject`, full `AliasStore`. New `aliases` table (`norm_name UNIQUE`, no FK), one timestamped migration. **`resolveByName` hot-path hook** (subjects-first-then-aliases, O(log n), no LLM) as a wiki-package helper shared by both `plannedSubject` and `ask`'s `gatherPages` delivers the durable forward effect.

4. **MCP UX** — Four verbs (`merge_candidates`, `merge`, `merge_dismiss`, `merges`) cloning the D16 idiom; opaque `pair_id`; type/slug paths in/out; two-channel errors; `merge` fire-and-return with a queued handle; confidence as a match-any enum array; chained-candidate resolution at browse+approve; large backlog handled by confidence-sort + pagination + agent loop-approval (**no batch verb, no auto-approve**). Flag the **no-recovery-from-bad-merge** gap explicitly.

5. **The backfill** — One-time, explicit operator-triggered, resumable/idempotent standalone CLI binary (`cmd/wiki-dedup-backfill`), off the steady-state worker, batched + parallel, sized solely from `SELECT count(*) FROM subjects`, with `--budget` ceiling and `llm_calls`-based cost observability. **Not** an appkit verb (closed set, separate module), **not** automatic on deploy, **not** on the ingest worker.

---

## 10. Open questions (need a human steer before the design doc)

1. **Content fingerprint definition** that gates re-judge + dismissal re-qualification: claim_count delta (cheap, misses same-count edits) vs. **hash of sorted claim ids** (catches repoints/edits). *Recommend the hash.*
2. **DF/rarity threshold and per-bucket cap values**, and the acceptable recall floor for common-token pairs — must be derived from a gold set, not guessed. Pick conservative defaults; make them config knobs.
3. **Steady-state judge model**: confirm Sonnet 4.6 is acceptable given the asymmetric false-merge cost (with Opus reserved for backfill), or should the steady-state judge also be Opus?
4. **Confidence floor level** (below `medium` vs below `high`) and whether it is a config override like the extract prompt (2751e6f / 4d09a2b).
5. **Backfill batch size K and parallelism G** — empirical tuning; does batching degrade verdict quality enough to matter given owner approval is the gate?
6. **Crash-recovery for un-judged newcomers — the anti-join does NOT work as a naive query.** `merge_candidates` records only *surviving pairs*, so a subject correctly judged to zero candidates is indistinguishable from one never judged; a naive "subjects with no merge_candidates row" anti-join would re-judge every zero-candidate subject (most of the corpus) on every boot — the forbidden recurring O(N)+judge cost. Recovery needs a **per-subject `judged_through` watermark** (e.g. a `subject_dedup_state(subject_id, judged_at)` row written when Stage-2 completes for a newcomer, regardless of whether it yielded candidates) so boot recovery is O(new-since-last-watermark), not O(N). Confirm this watermark schema (it also cleanly unifies with the backfill cursor). This is the redesign of the §8 "boot crash-recovery" row.
7. **resolveByName**: alias-chain depth bound and cycle behavior (fail-loud vs break); resolution order when a name is *both* a live subject and an alias target (recommended subjects-first).
8. **Tombstone / moved-claims shape**: record moved claim ids (to keep a future un-merge possible) vs. counts only — the id list is **unbounded** for a claim-rich loser under D07 accumulation, so a single audit row is a real sizing risk. Recommend a separate `merge_moved_claims` table over a blob column if ids are kept.
9. **Bad-merge recovery escape hatch**: accept that a mistaken merge's forward-routing alias is unrecoverable in v1 (un-merge + alias-delete both deferred), or pull a minimal `alias_delete` MCP verb into v1 scope?
10. **Scope boundary for D12 `PageWithLinks`**: is its O(N)-per-read rework (the largest scale risk in the blast radius, `links.go:103-130`) in scope for this feature's planning at all, or a strictly separate work item? *Recommend separate, but raise it explicitly — it threatens usability at 50k+ regardless of dedup quality.*
11. **`aliases.created_at` type**: confirm RFC3339Nano TEXT (matching `formatTime`, `data_model.go:709`) over INTEGER epoch for schema consistency.
12. **Capture in-document co-reference as alias hints at extract time?** A deterministic front-door *canonicalizer* (rewrite a name to canonical form before lookup/mint) is **rejected**: only true-equivalence orthographic transforms are safe to auto-collapse, and that is exactly what `normalize()` (`data_model.go:104-114`) already does — every *semantic* rewrite (legal-suffix strip, acronym, nickname) is ambiguous and context-dependent, so a blind front-door collapse is an ungated, content-free, **silent + irreversible + undetectable** auto-merge (strictly worse than the §6 merge path on every axis the human-gate decision cares about) and it erodes the `norm_name`-immutability the §2 completeness property rests on. The defensible residue: extract already runs an LLM over the full document in the worker (`service.go:234`, off the request path — not a D04 front-door violation), and the document often states the identity explicitly ("GDW (Game Designers' Workshop)"). **Proposal:** extend extract to emit, per subject, alternate surface forms co-referenced *in that document*; those hints (a) seed a high-confidence candidate pair (a free assist for case (b)) and (b) become evidence the post-commit judge reads — a **candidate, never a collapse**, so the owner-approval gate holds. **Do not relocate the whole judge to extract:** blocking is still needed to bound the candidate set at 100k+, and completeness for the K-newcomers-in-one-doc case (§2) and RE-JUDGE-ENRICHED (case a) both still require the post-commit pairing — extract-time linking cannot see a not-yet-minted or future-enriched subject, so it is an *enhancement on top of* the block→judge→gate spine, not a replacement. **Costs to weigh before adopting:** it touches the extract prompt that was just gold-set-tuned (commits 2751e6f / 4d09a2b), inheriting an eval cycle, and the hints need storage (a column or side table the post-commit judge reads). *Recommend: capture as hints, keep the spine; treat as a v1.x enhancement unless the gold set shows case-(b) misses that justify pulling it into v1.*
