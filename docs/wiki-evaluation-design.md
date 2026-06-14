# wiki evaluation — design

> **What this fixes.** `docs/wiki-redesign-research.md` settled *what* the wiki
> evaluation must measure (the ten inference sites), the *shape* of every
> evaluation (`f(prompt, model, effort, input) → output`, scored against a
> dataset, swept per generation), and the four reusable scorer kinds. It then
> left **six questions open** for the design (its "Open questions for the design
> doc"). This document resolves those six and pins the two artifacts the plan's
> P12 `Verify` requires — the **dataset record format** and the **four scorer
> kinds** — so Part II (P13–P16) executes a settled design, the way
> `wiki-redesign-design.md` settled Part I. It is the design half of the
> design→plan pair (`docs/README.md`); the plan half is the Part II phases
> (P13–P16) already in `docs/wiki-redesign-plan.md`.
>
> **What this is *not*** (carried verbatim from the research doc, because it
> governs every decision below): the harness is **not a test harness in the CI
> sense, not a build gate, not a deploy gate**. Every score is an input to a human
> decision, never an automated pass/fail. Nothing here may fire `go test ./...`
> red, gate a `/finish` phase, or block a deploy. The integration tier
> (`wiki-redesign-plan.md`, *Integration testing*) is the only thing that runs
> real models inside the build, and it is itself advisory.
>
> **Status.** Locked (2026-06-14). Resolves the research doc's six open questions;
> pins the dataset format and scorer kinds. The remaining concrete file layout,
> loader code, and per-site generators are P13–P16's build work, constrained by
> what is fixed here.

## The two pinned artifacts (the P12 `Verify` gate)

Before the six decisions, the two shapes everything else references — fixed here
so P13's loader and P14's scorers are written against one contract, not
re-derived.

### Dataset record format

A test set's dataset is a list of **cases**, each one record with exactly these
fields (the research doc's `(case_id, site, generation, failure_tag, input,
gold)`, made concrete):

| field | type | meaning |
|---|---|---|
| `case_id` | string | Stable unique id within the dataset (e.g. `match-0007`). Survives across generations so a case can be tracked / refreshed. |
| `site` | string | The inference site this case exercises — one of the ten registry names (`extract`, `match`, `compile`, `merge`, `dup_judge`, `canonical_name`, `ask`, `candidates`, `search`, `sweep`). Ties the case to a scorer kind. |
| `generation` | int | Which numbered generation of this site's test set the case belongs to (1-based). The outer sweep dimension. |
| `failure_tag` | string | The dangerous-direction behaviour the case stresses (e.g. `false_merge`, `over_extract`, `fabrication`, `identical_name_diff_thing`, `zero_overlap_synonym`). Free-form but drawn from a per-site enumerated vocabulary; lets a scorer slice the dangerous axis by stressor. **Not** a difficulty label (research: harder = later generation, never a per-case stamp). |
| `input` | object | The exact input the **real** call-site function consumes — site-shaped (a document for extract; an incoming subject + shortlist for match; a manifest for merge; a question + synthetic-wiki id for ask; …). Stored verbatim so the harness feeds prod the byte-identical input. |
| `gold` | object | The reference the scorer aligns against — site-shaped (the gold `subjects[]` for extract; `same(id)`/`no_match` + expected `dup_pairs` for match; the rubric expectations + must-survive cite ids for merge; the gold answer + supporting pages + abstention flag for ask; the gold relevant-page set for retrieval). |

Serialized as JSON (one file per `(dataset)` artifact; see *Test-set storage*
below). The `input`/`gold` objects are intentionally `site`-polymorphic — the
loader returns them as `json.RawMessage` and each scorer unmarshals into its own
typed shape, so adding a site never reshapes the record.

### The four scorer kinds

Every one of the ten sites maps to exactly one of four scorer kinds (research
doc piece 2; this is the P14 build surface). Each kind reports the
**dangerous-direction error as a named separate axis**, never lumped into one
accuracy number — the research doc's asymmetry principle is a structural
requirement on the scorer interface, not a per-scorer choice.

1. **Set-alignment** — `extract`, `compile`. Align predicted↔gold subjects;
   report **subject precision and recall separately** (over-extraction is the
   dangerous axis, its own counter), type accuracy on matched subjects, claim
   recall (fuzzy / LLM-judged), self-containedness. `compile` adds compression
   ratio, per-claim citation precision/recall, and `occurred_at` accuracy.
2. **Asymmetric confusion** — `match`, `dup_judge`. Binary (match) / ternary
   (dup judge) confusion with **false-merge** named separately (and false-split
   for match; false-dismiss + a `can't-tell-yet`-when-evidence-present laziness
   metric for dup judge). The `dup_pairs` side-channel scored as its own recall
   number (research obligation 3 — the side-channel is a *distinct* output, never
   folded into the verdict).
3. **Recall@k + RRF** — `candidates`, `search`, `sweep`. recall@k for candidates
   (recall is king — a miss mints a permanent dup), a ranking metric for search,
   pair-discovery recall for sweep; the lexical-only-vs-hybrid side-by-side with
   embed model/dims, per-lane thresholds, and RRF `k` as sweep axes.
4. **Mechanical-checks + rubric-judge-panel** — `merge`, `ask`. The deterministic
   invariants Part I already exposes (merge: the §6.1 citation-preservation gate,
   write-set conformance, claim-cite presence — reused for free; ask: citation
   faithfulness as a mechanical "does the cited page exist + contain the span"
   check) **plus** a rubric LLM-judge panel for the subjective criteria (merge:
   lead-identity, woven-not-ledgered, contradictions-corralled, no-loss,
   no-hallucination; ask: answer correctness, abstention correctness on the gap
   set — **fabrication rate is the headline** — contradiction-surfacing, with
   retrieval failure decomposed from synthesis failure).

`canonical_name` (site 6, low stakes) is scored as a thin **agreement-with-
convention** check — a degenerate asymmetric-confusion case — and rides the same
kind-2 plumbing rather than warranting a fifth kind.

---

## The six resolved questions

### 1 — Golden authorship vs real-data harvest

**Decision: synthetic-first authorship is the spine for every site; real data is
an *additive anchor*, never the seed.** The mix is set per site by what real data
the system actually produces (enablement obligation 4), but the *rule* is
uniform: real data **validates and augments** a synthetic generation — it never
seeds the generator, because a generator seeded from production output inherits
production's blind spots (the exact false-merges and missed candidates we are
trying to surface would never appear as gold).

Per-site mix:

| site | synthetic | real-data anchor |
|---|---|---|
| extract | primary (authored docs w/ gold subject+claim sets) | ingested documents on `int` + their captured extract output (obligation 4) become an additional *unlabeled* corpus for spot-checking recall; a human confirms a sample of golds. |
| match / dup judge / candidates / sweep | primary (one shared synthetic identity corpus) | the `dup_flags` + dup side-channel rows captured in prod (obligation 4) supply real positive pairs to add to later generations. |
| compile | primary (authored event piles) | real `outbox`/digest output, if any, validates compression behaviour; not a generator seed. |
| merge | primary (manifests built directly, skipping extract) | none needed — merge's inputs are internal. |
| ask | primary (a fixed synthetic wiki + question set) | the **`asks` table is a first-class golden source** (obligation 4): real questions + their answers/citations become validation cases and feed the *answerable* slice of later generations. Still validates, never seeds — gold answers are re-confirmed by a human/judge, not trusted from prod. |
| canonical_name | primary (thin preference set) | none. |

**Why the asymmetry "validate, don't seed":** the redesign's whole polarity is
catching the dangerous direction (false-merge, fabrication, over-extraction).
Those are precisely the cases prod gets *wrong*, so they are absent from prod's
"gold." A synthetic generator authors the trap deliberately; real data then
confirms the synthetic distribution resembles reality and contributes additional
*blunt* positives for the regression floor.

### 2 — Judge-model independence

**Decision: the judge model is a single, fixed, held-out model — never a model
under test in the same run.** The two sites that need an LLM-judge (merge's
rubric panel, ask's answer-correctness + the fuzzy/LLM-judged claim recall in
set-alignment) use a judge pinned by config and **excluded from that run's sweep
axis**, so a model can never grade its own output (self-preference is the named
failure).

- **Independence is enforced structurally, not by honour:** the harness asserts
  at run start that the configured judge model id is **not** in the run's
  model-sweep set for any judged site, and refuses the run (a config error, not a
  silent skip) if it is. This makes "the judge isn't under test" a checked
  invariant rather than operator discipline.
- **The pin (provisional, P16 may revise):** the model registry currently carries
  three chat models — `claude-haiku-4-5`, `claude-sonnet-4-6`, `gpt-5.5`
  (`agentkit/model/registry.go`). The production call sites under test pin
  `claude-sonnet-4-6` and (extract may sweep) `gpt-5.5`. **The judge is
  `gpt-5.5` when the swept role uses Anthropic models, and `claude-sonnet-4-6`
  when the swept role includes `gpt-5.5`** — i.e. the judge is always drawn from a
  *different provider family than the role under evaluation*, the strongest
  cross-family guard against self-preference available in the current registry.
  Where a single run sweeps both families for one role, the judge is the
  registry's strongest model held out of *that* role's axis; if no held-out
  strong model exists for a role, the harness reports the constraint rather than
  judging with an in-sweep model. (When a fourth, deliberately-judge-only model
  is added to the registry it supersedes this and the cross-family rule is
  dropped — recorded as the cleaner end state.)
- **Panel size:** subjective rubric criteria (merge's woven-not-ledgered,
  lead-identity, contradictions-corralled; ask's answer correctness) are scored
  by a **panel of 3 judge samples**, aggregated by majority for binary criteria
  and median for graded ones. Deterministic mechanical checks (citation
  preservation, write-set conformance, claim-cite presence) are **single-shot, no
  panel** — they are not judgments. Panel size is itself a config knob the report
  can revisit if 3 proves noisy; 3 is the smallest odd panel that breaks ties.

### 3 — Test-set storage & identity

**Decision: a test set is a named, versioned, on-disk bundle of two independently
versioned artifacts — a `dataset` and a `prompt` — and a run pins the exact
bundle it used by content hash, so a result stays attributable after the set is
superseded.**

Layout (committed under the harness's tree — location fixed by question 4):

```
testsets/
  <site>/
    datasets/
      gen-1.json          # the dataset records (the record format above)
      gen-2.json          # a later, harder generation — stands beside gen-1
    prompts/
      v1.txt              # a candidate prompt artifact
      v2.txt
    bundles/
      gen-1.json          # {dataset: "datasets/gen-1.json", prompt: "prompts/v1.txt"}
      gen-1-promptv2.json # same data, candidate prompt — a distinct bundle
```

- **A bundle names one dataset + one prompt.** Minting a new test set is swapping
  either: a harder dataset against the same prompt (the saturation escape hatch)
  or a candidate prompt against the same dataset (prompt iteration). Both are
  first-class, both produce a new bundle, neither edits an old one.
- **Saturated generations are kept, never deleted** (research doc) — `gen-1.json`
  stays as a regression floor when `gen-2.json` does the discriminating.
- **Run-to-bundle pinning is by content hash.** A run record stores the
  bundle's name **and** the SHA-256 of (dataset file bytes + prompt file bytes).
  So even if a bundle file is later edited in place, an old run's results remain
  unambiguously attributable to the exact `(data, prompt)` it scored. The hash is
  also the cache key's prompt/data component (question 4).
- **The production prompt is just a prompt artifact** — `prompts/vN.txt` may *be*
  the pinned production default for that site (a symlink/copy from
  `wiki/internal/config`'s default), so "evaluate production" and "evaluate a
  candidate" are the same operation against different bundles (research doc's
  reconciliation with the production-code-path principle). The harness reads the
  prompt from the bundle and injects it into the real call site via config — it
  never forks the call-site code.

### 4 — Where the harness lives & cost control

**Decision: the harness is a standalone Go command under `bin/`, built from the
suite repo, not an `opsctl` verb and not a `go test` target.**

- **Not `opsctl`:** `opsctl` is the on-box operator CLI for box operations
  (`CLAUDE.md`); the harness is an off-box developer/analysis tool that makes paid
  provider calls and produces reports — wrong tenant.
- **Not a `go test` target:** `go test ./...` is the trustworthy, free, offline,
  deterministic **phase gate** — the harness is paid, networked, and
  nondeterministic by nature, and the research doc forbids it from gating
  anything. Putting it behind `go test` would either pollute the gate or require a
  build tag that hides it; a plain command is honest about what it is.
- **It lives at `bin/wiki-eval`** (a small Go `main` in the suite, wired into
  `go.work` for local dev), importing Part I's **call-site registry** (P2) to
  reach the real call-site functions. It takes a site, a bundle name, a model ×
  effort sweep spec, and a judge pin; it writes the results table.

**Output caching is load-bearing, not an optimization** (the sweep is a cartesian
product of paid calls; a re-score with a changed scorer must cost zero provider
calls):

- **Cache key:** `(test-set-bundle-hash, case_id, prompt-hash, model, effort)` —
  exactly the research doc's `(test-set, case, prompt, model, effort)`, with
  test-set and prompt resolved to their content hashes (question 3) so a bundle
  edit correctly misses the cache. The prompt hash is folded in even though it is
  part of the bundle, so a prompt-only swap re-runs only what changed.
- **Cache value:** the **raw call-site output** plus its captured cost + latency —
  *not* the score. Scoring runs over the cached raw output, so changing a scorer
  (P14 iteration) re-scores for free. This is the property the `Verify` of P13/P16
  asserts ("reproduces a second run entirely from cache, zero paid calls").
- **Cache store:** a content-addressed directory (`~/.cache/wiki-eval/` or
  repo-local `.wiki-eval-cache/`, gitignored), one file per key. It is a pure
  memoization layer — deleting it only costs money, never correctness.

### 5 — Saturation detection

**Decision: a generation is declared *saturated* when the top configs cluster at
the ceiling with no separation — operationalized as a two-part rule, reported by
the harness, decided by a human.**

A generation is **saturated** when **both** hold over the run's config set:

1. **Ceiling:** the best config's headline score for that site is `≥ 0.95` of the
   achievable maximum (per-site headline: subject F1 for extract/compile;
   1 − false-merge-rate for match/dup; recall@k for retrieval; the rubric mean
   for merge; 1 − fabrication-rate for ask).
2. **No separation:** the spread between the best and the *k*-th config
   (`k = min(3, n_configs)`) is `≤ 0.02` on that headline **and** their
   dangerous-direction error rates are statistically indistinguishable (all zero,
   or within one case of each other on the dangerous axis).

When both fire, the harness prints a **`SATURATED — mint gen-N+1`** advisory in
the report; it does **not** auto-generate anything (research doc: a triggered
decision, not a vibe, but still a human's call). The thresholds (`0.95`, `0.02`,
`k`) are config knobs so a site with intrinsically noisier judging can loosen
them. Saturation is always evaluated **within one generation** — a config acing
gen-1 while gen-2 still separates is the designed steady state, not saturation of
the site.

### 6 — Decision presentation

**Decision: a run renders one row per config and never a single lumped rank — the
dangerous-direction error, cost, and latency sit *beside* the headline score, so
the tradeoff the decision turns on is always visible.**

The report is a table, **always captioned with the generation it scored**
(`gen-N`), one row per `(model, effort)` config, with columns:

| column group | columns |
|---|---|
| config | model, effort, role (for per-role sweeps), prompt-version |
| headline | the site's primary score |
| **dangerous axis** (always separate) | the named dangerous-direction rate — false-merge-rate, over-extraction count, fabrication-rate, false-dismiss-rate — per the site's scorer kind |
| cost | total USD for the config's cases + per-case mean (from P0c's per-call `cost_usd` accounting) |
| latency | mean + p95 ms per call (from P0c's `duration_ms`) |
| coverage | n cases scored / n cached |

Presentation rules that bind P16's renderer:

- **No single composite rank.** The table never collapses these columns into one
  number; ranking the rows is the human's job, and the table exists so they rank
  on the axis they care about (a 95%-with-one-false-merge config must be visibly
  worse than a 90%-with-none config — research doc's worked example).
- **Sort default is the headline, but the dangerous axis is a tiebreak the
  renderer surfaces** — rows within `≤ 0.02` headline are visually grouped so a
  human compares their dangerous/cost/latency directly.
- **The retrieval side-by-side is its own table** (lexical-only vs hybrid per call
  site: recall lift vs cost) — the deliverable that licenses or declines the
  vector lane at each site (research §8–10).
- **The chosen config is recorded back as Part I's config default** (the
  feedback loop): the report names, per site, the `(prompt, model, effort)` a
  human picked, and P16d ships them. The report documents that this — a human
  reading the matrix — *is* how defaults get set; nothing auto-promotes.

---

## What P13–P16 inherit from this lock

- **P13 (the rig)** builds the loader against the record format above, the runner
  against Part I's call-site registry, the `config × metric` table reported per
  generation, and the content-hash-keyed output cache from question 4. It proves
  the rig on **Match** (the shared identity corpus's headline consumer).
- **P14 (the scorers)** builds exactly the four kinds above, each reporting its
  dangerous axis separately, reusing Part I's deterministic invariant checks
  (§6.1 gate, write-set conformance, claim-cite presence) for the merge mechanical
  half.
- **P15 (test sets)** builds one generator per site emitting bundles in the
  storage layout of question 3, sharing the identity corpus across
  match/dup/candidates/sweep and the synthetic wiki across ask/search,
  LLM-authoring goldens then running the adversarial verification pass, anchoring
  against real data per question 1's validate-don't-seed rule.
- **P16 (sweep + report)** runs the full `generation → {prompt,data} × model ×
  effort` matrix (incl. OpenAI via P0a), renders the report per question 6,
  applies the saturation rule of question 5, and closes the feedback loop into
  Part I's config defaults (shipped by P16d).
