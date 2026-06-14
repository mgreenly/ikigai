# Plan — wiki redesign: a prose-pages knowledge service

> **Locked build-time decision (P1) — digest claimable unit: Framing 2.** The
> claimable unit for the digest pass is a single **`(cron-row, entry)` pair**, not
> the whole cron row. This keeps the selection critical section uniform (one
> claim = one job to run), makes the cron-row stamp a pure completion-time join
> with **no leave-pending-if-locked-out** special case, and removes that special
> case from the spine — which matters most because P4 is built before any digest
> exists (design §11, plan P1).

The phased build plan for the wiki rewrite. The **how and the decisions** live
in `docs/wiki-redesign-design.md` (the source of truth — read it first, in full);
the full rationale behind any decision is in `docs/wiki-redesign-decisions.md`.
This file turns that design into a sequence of **subagent-sized phases**.

> Status: **draft** (2026-06-13), to be refined. The existing `wiki/` code is
> superseded — this is a clean rewrite, nothing is preserved.
>
> **The plan has two parts.** **Part I (P0a–P11)** builds the wiki service —
> starting at **P0a–P0c**, a shared-library expansion of the agentkit SDK: an
> **OpenAI** chat backend (P0a, ported from `/mnt/projects/ikigai-cli`), an **embeddings**
> lane (P0b), and **per-call cost logging** plus Anthropic effort parity (P0c) —
> all prerequisites the rest of the build assumes. **Part II (P12–P16)** builds the offline **evaluation
> harness** described in `docs/wiki-redesign-research.md` — the tool that
> sweeps `model × effort × prompt` per inference site and scores the results.
> Part II depends on Part I:
> the harness scores the *production* call sites, so it cannot exist until they
> do. Throughout Part I we honor the **enablement obligations** (see below) that
> make those sites harness-callable; Part II then builds the rig, scorers,
> generators, and report on top. The harness is a measurement tool — it never
> gates a deploy or fails a build (research doc, "What this is *not*").

> **Local credentials.** `wiki/.envrc` already exports both
> `ANTHROPIC_API_KEY` and `OPENAI_API_KEY` (resolved from `~/.secrets/` via
> direnv). If a phase needs to run live inference or embeddings locally, the keys
> are there — `direnv allow` in `wiki/` and they are in the environment; no need
> to source or set them by hand.

## Sizing principle

Each phase is one coherent unit a single subagent can complete from a cold start
within one context budget: it compiles, its tests pass, and it is independently
committable. Phases execute **strictly in the listed order**; each assumes the
committed output of all prior phases.

The dominant risk this plan is structured to defuse: the design's correctness
lives in the **seams between components** (the airtight end-of-run transaction,
the resolve→merge→commit pipeline shared by document and digest passes, the
cross-prompt invariants, the optimistic-commit/conflict loops). A naive
horizontal cut (extract, then resolve, then merge, …) would leave early phases
unverifiable because their load-bearing behavior only materializes once the
seam-partners exist. So we build **the spine first with stubbed integrators**
(P4–P5), then add real behavior as vertical slices *onto a working whole*
(P6a onward). Every phase from P4 on can be exercised end-to-end. So the swap
from stub to real integrator is **mechanical rather than a reshaping**, P4 pins
the two contracts every integrator hands off through — the `Manifest` type and
the `Integrator` interface — *before* any real integrator exists; P6a–P8 then
fill behavior into those fixed shapes instead of re-deriving the seam.

## Schema canonicity (a standing rule)

The complete, final SQLite schema lives in **one place — design §12
("Consolidated schema")** — and it is authoritative. P1 *transcribes* §12 into
migrations; no phase re-derives the DDL from prose. Two rules bind every phase:

- **No phase silently changes the schema shape.** A column, constraint, or index
  a phase needs must already be in §12. If it isn't, the phase **adds it to §12
  first** (and says so in its commit), then writes the migration — so §12 and the
  database never diverge.
- **Committed migrations are immutable** (`CLAUDE.md`, `bin/check-migrations`).
  Any later phase that finds the live schema lacking something §12 names adds a
  **new** corrective migration via `bin/new-migration` — it never edits an earlier
  one. A schema gap is therefore always recoverable forward, never a dead end.

This exists because P1's schema is the foundation of all of Part I, its artifacts
are immutable, and a P1 self-check written from P1's own reading cannot catch a
column P1 itself omitted — §12 is the external spec that both the migration and
the schema test are checked against.

## Manifest canonicity (a standing rule)

The `Manifest` is the load-bearing **swap-boundary contract** P4 freezes *before*
any real integrator exists — the spine and its end-of-run transaction consume it,
never integrator-specific data, which is the whole "swap is mechanical" bet of the
Sizing principle and the plan's self-declared dominant risk ("the design's
correctness lives in the **seams**"). It is the seam's equivalent of the schema,
and it gets the same discipline as *Schema canonicity* above — for the same reason
and against the same failure.

The danger is that, unlike the schema, the design **never consolidates the
manifest**: it is described only as scattered prose (design §3 the version slot,
§4.4 the annotations, §4.5 the commit, §5 the generalized claim, §6.1 the
`superseded` source), with no §12-style authoritative section. So the complete
cross-phase contract lives in **one place: the enumeration below** — and it is
authoritative. P4 *transcribes* it into the `Manifest` Go type; **no phase
re-derives the contract from the design's prose** (the very anti-pattern §12
forbids for the DDL). The `Manifest` is the in-memory work order, **never
persisted** (the run id is its durable identity), **empty-capable** (a stub emits
a minimal one). Its fields, each tagged with the consumer that reads it:

- **`subjects[]`, addressable by `subject_id`** — extracted/compiled subjects,
  **individually addressable** so a conflict can re-enter the existing stage
  functions for *one* subject without reshaping the type (one page per subject —
  §4.1, `pages.subject` — so every per-page slot below hangs off its `subjects[]`
  entry, never a manifest-global scalar). Each entry carries (a) the **extracted
  fields** — `type, kind, name, aliases, claims[]` (extract/compile emit them;
  merge reads `type`/`kind` for routing and `name`/`aliases` for the
  identity-establishing lead — §4.2, §5) — **plus** (b) the **resolution
  annotations**: the resolved **subject_id** and **target page** (first read by
  P6b2, the first producer; §4.4), the per-subject base `version` slot, the
  per-subject `occurred_at`, and the per-page `superseded` and `stale_notes`
  carriers below (§4.4).
- **generalized `{text, cites[]}` claim shape** per subject — on each `subjects[]`
  entry's `claims[]`; the document pass fills `cites` with the one inbox row id;
  compile fills per-claim cites (read by P7a's merge and P8's compile; §4.3, §5).
- **per-subject merged page content (`page_title`, `page_body`)** — the rewritten
  prose page merge produces for this subject's target page (§4.4: "rewritten prose
  pages"). Merge's read+write-page work is **captured** into these slots rather than
  written directly, so the end-of-run transaction owns the only write and there are
  **zero mid-run partial writes** (§4.5). Populated by merge in P7a, read by P7a's
  end-of-run transaction (the page upsert + the `pages_fts` sync). One page per
  subject (§4.1), so the content lives on the `subjects[]` entry, never a
  manifest-global scalar.
- **write-set pages** = exactly the **target pages named by `subjects[]`** (each
  addressable by subject_id), **not a separate parallel structure**: P6b2 populates
  the write set as exactly the subjects' target pages, and P7a's merge write set is
  that set verbatim — "the manifest's pages, exactly" (populated by P6b2, read by
  P7a's merge; §4.4).
- **per-subject `occurred_at`** — first-writer-wins, **events only**, a property of
  each event subject (so it lives on the relevant `subjects[]` entry, not as one
  manifest scalar — §4.1 `subjects.occurred_at`) (read by P7a's commit; §4.1).
- **per-page `superseded`** — the dropped-citation declarations the §6.1 gate
  checks, carried **per page on the `subjects[]` entry** so P7b's re-run can
  re-validate the gate against the *re-run's* fresh `superseded` (populated by
  merge, read by P7a; on a P7b conflict re-merge it is **re-derived** from the new
  merge output, never the stale original; §6.1).
- **per-subject base `version` slot** — the `pages.version` the page was read at,
  one per `subjects[]` entry (design §3: "the manifest records the version merge
  read"; one page per subject ⇒ the base version is per subject, never a
  manifest-global scalar); **populated at merge-read time in P7a**, **read by P7b's**
  per-page optimistic-commit `WHERE subject=? AND version=?` guard.
- **per-subject re-merge handle** — the second thing P7b's conflict loop needs: not
  a diagnosis step but the structural requirement that a **single subject's slice**
  (its `claims[]`, target page, base `version`) be re-merged/re-resolved in
  isolation. The conflicting page/subject is identified by the failing
  `WHERE subject=? AND version=?` (P7b) or the alias `UNIQUE` violation (P7b2) — no
  diagnosis (§3) — and the subject-addressability above is what lets the retry
  re-enter the existing stage functions for that one subject (read by P7b/P7b2; §3).
- **`stale_notes[]`** — the stale-note set merge produces while folding (subject,
  note, `cites`; the `id`/`run_id`/`status` are filled at write time per §12 #4):
  the carrier through which merge signals which `stale_notes` the end-of-run
  transaction must write **in its existing commit**, so P7a's transaction owns the
  write rather than reaching into merge's side effects (populated by merge, read by
  P7a's end-of-run transaction; §6 / §12 #4).
- **`dup_pairs[]`** — candidate duplicate subject pairs to flag, each in canonical
  order (smaller ULID first): the pairs resolve's many-ids arm surfaces (P6b) and
  match's side channel reports (P6b2), **assembled into the manifest by P6b2** (the
  manifest producer) and **read by P7a's end-of-run transaction**, which inserts
  them into `dup_flags` via `FlagDup` (§3, §4.3, §4.5; eval obligation 3).

Two rules bind every phase, mirroring *Schema canonicity*:

- **No phase silently reshapes the `Manifest`.** A field a phase needs must already
  be in the enumeration above. If it isn't, the phase **adds it here first** (and
  says so in its commit), then updates the type and every producer — so the
  contract and its consumers never diverge.
- **The type is not immutable — and that is the danger, not a reprieve.** A schema
  gap is recoverable forward via a *new* migration; a `Manifest` field discovered
  missing at P7b has **no additive escape hatch** — it forces editing the committed
  type *and* every producer already written against it (P6b2, P7a), across phases
  that strictly-sequentially built on the frozen shape. That is precisely the
  "downstream concurrency phase silently reshaping a spine type" failure P4 exists
  to prevent. So the "add here first" discipline is the *only* defense: the field
  set must be complete at P4 and checked against this enumeration, never discovered
  at runtime.

This exists for the same reason *Schema canonicity* does: a P4 self-check written
from P4's own reading cannot catch a field P4 itself omitted — this enumeration is
the **external spec** that both the `Manifest` type and P4's completeness test are
checked against, exactly what §12 is for the schema. So that check must be
**enumeration-derived, not hand-authored**: P4's completeness test is a single
**table-driven** test whose cases *are this enumeration* — one row per field above,
paired with its declared consumer — asserting, by reflection over the Go type, that
**every** named field is present, and failing the phase if any is absent. This is the
in-memory mirror of what `bin/check-migrations` + the §12 schema test do for the DDL:
a field-presence assertion against the authoritative list. It is **distinct from** the
stub's round-trip test (which proves a *populated* `Manifest` survives the end-of-run
transaction but, by construction, can **never** catch a field absent from both the
type and the stub — the stub does not populate what the type lacks). Without the
table-driven check an omitted field rides a green round-trip all the way to P7b — the
"no additive escape hatch" failure above, surfacing at its least-recoverable point.

## Phase-budget re-check (a standing rule)

The Sizing principle above is the one invariant the whole `/finish` model rests
on (`docs/README.md`: "that bound is the whole reason phases exist"). The three
cross-cutting systems that follow — eval enablement, the integration tier, and
prompt-default validation — each deliberately **grow existing call-site phases
rather than add new ones** ("adds no new phases," stated at each). That keeps the
chain short, but it silently spends the very budget the Sizing principle protects:
every standing slice piled onto P6a–P11 makes those phases bigger, and a phase
that quietly overruns one cold-start context is the single failure the whole
phased model exists to prevent — a subagent that runs out mid-phase commits a
*partial* integrator that can still pass its mocked unit gate, and every strictly
sequential phase after it builds on the corruption.

So the rule: **any change that grows a phase must re-assert that the phase still
fits a single subagent cold start, and split it via sub-letters if it doesn't** —
the §12-style "fix the spec first" discipline, applied to the context budget
instead of the schema. Splitting never disturbs downstream numbering:
"sub-letters keep every downstream edge intact" (Dependency chain, below). The
phases nearest the budget — to re-audit whenever a standing requirement touches
them — are the densest call-site units, the ones carrying the system's hardest
prompts (extract, match, merge) bundled with the resolve/commit machinery:
**P6b2**, **P7a**, and **P7a2**. **Three phases were carved off under exactly this
rule:** **P7a2** off P7a (the merge prompt + its whole validation surface overran
the airtight transaction it shipped with — see P7a2); **P6b2** off P6b (the match
prompt + its validation surface overran the mechanical resolution arms — P6b is now
the zero-LLM resolve/candidates half, P6b2 the match+manifest half); and **P7b2**
off P7b (the duplicate-mint conflict arm carved from the optimistic commit +
lost-update loop + §6.1 gate it was bundled with). These pre-emptive splits move a
runtime budget gamble into a design-time decision rather than trusting an
overrunning cold-start subagent to notice and split itself.

## Phase completion is a checklist, not a green gate (a standing rule)

The phase-budget re-check above is a *design-time* defense — split a phase before
the march if it won't fit. This rule is its *boundary-time* backstop, for the phase
that overruns anyway. The failure it guards is the one the re-check rule names: a
subagent that runs out mid-phase **commits a partial integrator that still passes
its mocked unit gate**, and every sequential phase after it builds on the
corruption. The hole is that `/finish`'s done-criterion — "it compiles and its
mocked tests pass" (`docs/README.md`) — is satisfied by a *partial* phase: a missing
conflict arm, an unwired gate, or an un-populated manifest field does not fail
`go test ./...` when every LLM is mocked and the absent piece simply has no test
yet.

So the rule: a dense integrator phase is **done only when every named deliverable in
its body is present and individually tested**, not merely when the suite is green.
Each such phase carries a **`Deliverable gate`** line in its `Verify` block
enumerating its load-bearing pieces; the coordinator confirms each is present and
covered *before closing the phase*. This converts "a partial integrator rode a green
mocked gate into the next phase" from a silent, propagating corruption into a **loud
failure at the boundary that introduced it** — the same move the prompt gates made
for placeholder prompts (*Prompt-default validation*), applied to partial
*implementations*. It is pinned to a phase's `Verify` — the gate `/finish` actually
enforces — for the same reason the checkpoints are: a rule addressed only to "the
orchestrator" never fires (see *Integration testing*).

This adds **no new phase** and no new mechanism the orchestrator doesn't already
run; it only makes "done" mean *complete*, not merely *green*. The phases that carry
the line are the **immutable foundation (P1)**, the **frozen swap-boundary contract
(P4)**, and the spine integrators where a partial commit is both likely and
load-bearing: **P1, P4, P6b, P6b2, P7a, P7a2, P7b, P7b2, P8**. **P4 belongs for the
same structural reason P1 does:** it freezes the `Manifest` — the seam's §12 — and a
`Manifest` field omitted at P4 has, unlike a schema gap, **no additive escape hatch**
(*Manifest canonicity*): it is not recoverable forward but forces editing the
committed type and every producer across the strictly-sequential phases built on the
frozen shape. A green round-trip test cannot catch it (the stub never populates a
field the type lacks), so only a boundary checklist forces the coordinator to confirm
the **enumeration-derived completeness test covers every field** in the *Manifest
canonicity* enumeration before P4 closes — exactly as P1's checklist confirms every
§12 table. **P1 belongs on this list for the strongest reason of all:** its
artifacts are *immutable* (the §12 schema, transcribed into committed migrations),
so a partial commit there — a cold-start subagent that runs out after transcribing
seven of nine tables, or that omits the `outbox` byte-identity test — is recoverable
only by *forward corrective migrations across an already-marching build*, the
costliest recovery in the plan, at its least-recoverable phase. And it is exactly
the failure this rule guards: a partial migration set rides a green
`go test ./wiki/internal/db/...` because the un-transcribed tables simply have no
test yet (the schema test the same subagent wrote covers only what it reached). The
§12-cross-check in P1's `Verify` states the *intent*, but only a boundary checklist
forces the coordinator to confirm **every** §12 table, both library-owned
migrations, and the locked fork decision are present before P1 closes — independent
of whether the green suite happens to cover them all.

## Dependency chain

```
Part I — the wiki service
P0 (preflight: ANTHROPIC_API_KEY + OPENAI_API_KEY present — HALT-not-SKIP, the entry mirror of P11k)
 ▼
P0a ──▶ P0b ──▶ P0c ──▶ P1 ──▶ P2 ──▶ P3 ──▶ P4 ──▶ P5 ──▶ P6a ──▶ P6b ──▶ P6b2 ──┐
oai     embed   cost+   schema scaffold ingest spine failure extract resolve  match │
chat    lib     a-eff   (stubs) policy  doors                       +cands  +manifest│
                                                                                     │
    ┌────────────────────────────────────────────────────────────────────────────────┘
    ▼
   P7a ──▶ P7a2 ──▶ P7b ──▶ P7b2 ──▶ P8 ──▶ P9a ──▶ P9b ──▶ P9c ──▶ P10 ──▶ P11 ──▶ P11k ──▶ P11d ──┐
   merge   merge    commit  dup-     digest dups    sweep   stale   read    embed   keyed   first    │
   core    gate     +lost-  mint                                                    gate    deploy   │
                    update                                                                           │
   (P11k: keys-REQUIRED, HALT-not-SKIP — the Part-I exit gate; P11d: first prod deploy to int) ▼
Part II — the evaluation harness     P12 ──▶ P13 ──▶ P14 ──▶ P15 ──▶ P16 ──▶ P16d
                                     design  rig    scorers  test    sweep   re-
                                     lock                    sets    +report deploy
```

> **Phase-sizing note.** The old monolithic **P6** (registry primitives +
> `normalize` + extract + resolve + match + the manifest's first real producer —
> two of the system's hardest prompts plus the candidates design in one phase),
> **P7** (merge + the airtight commit + the two conflict loops + the §6.1 gate +
> the stub swap), and **P9** (three lint jobs that "differ wildly in shape and
> cost," design §6) each overran the one-subagent-cold-start budget the whole
> `/finish` model rests on (`docs/README.md`). They are split: **P6 → P6a**
> (registry primitives + `normalize` + extract) **/ P6b** (resolve + candidates,
> zero-LLM) **/ P6b2** (match + the manifest), cut along the `subjects[]` data
> contract design §4.2 pins and then along the match-call boundary; **P7 → P7a**
> (merge core + the plain happy-path commit + the stub swap) **/ P7a2** (the real
> merge prompt + its offline gate, the second integration checkpoint, and the
> merge eval hook — carved off because the merge prompt is the system's hardest
> and arrives bundled with the airtight transaction) **/ P7b** (optimistic commit
> + the lost-update loop + the §6.1 gate) **/ P7b2** (the duplicate-mint conflict
> arm); **P9 → P9a** (`lint-dups` + the shared lint plumbing) **/ P9b**
> (`lint-sweep`, zero-LLM) **/ P9c** (`lint-stale`). The chain is otherwise
> unchanged — sub-letters keep every downstream edge intact.

**P0 precedes everything** — a deterministic, offline preflight that HALTs the
march before P0a if either key is absent (see *Keys present before the march*),
so the live-model checkpoints fire at their designed points instead of skipping.
Numeric order then satisfies every edge. P0a–P0c (the shared-library work) underpin
every LLM call site (P2's wrapper) and the embedding lane (P11), and let Part II
sweep OpenAI models. P0b (embeddings) is P11's only hard dependency and P0a
(OpenAI chat) is what Part II's OpenAI sweep needs, so both could be resequenced
next to those consumers; they are front-loaded here to keep P1–P16 numbering
stable. P1 (schema) and P2 (scaffold) underpin everything else. P4's spine needs
P3's inbox rows to select. P6b consumes P6a's extracted `subjects[]`; P6b2 takes
P6b's resolution outcome to match and assemble the manifest. P7a closes
the document-pass loop P6a–P6b2 open (its merge runs against a placeholder default
prompt under mocks); P7a2 then lands merge's real config-default prompt and the
validation surface that proves it (offline gate, the second integration
checkpoint, the eval hook); P7b hardens its commit (optimistic concurrency + the
lost-update loop + the §6.1 gate) and P7b2 adds the duplicate-mint arm. P8 reuses
P6b–P7b2's resolve→merge→commit.
P9a lands the shared lint plumbing P9b/P9c reuse; P9c consumes the `stale_notes`
P7a's merge writes. P10's read side needs pages to exist (P7+). P11 (the embedding lane) is designed now but
**sequenced last** — FTS5-first is build ordering only (design §9.3). **P11k** then
gates the Part-I→Part-II boundary: the keyed validation phase that discharges the
Part-I exit obligation (the three integration checkpoints each green at least once)
before any harness exists. **P11d** then takes the validated service to production
— the first deploy to `int` — so real ingests and `asks` start accruing as Part
II's real-data goldens (enablement obligation 4) while the harness is built.
**Part II starts only after P11k**: every inference site
must exist, be harness-callable, **and** have been proven live — not mock-only —
before the harness that scores them can be built (research doc's
production-code-path principle). P12 resolves the eval design before any harness
code is written, the way P1 resolves the digest fork.

## Suggested package layout

A fresh `wiki/` aligned to component boundaries (names are guidance, not
mandate): `cmd/wiki` (verb dispatcher); `internal/config` (env → config,
per-call-site prompt/model/effort seam); `internal/inbox` (`Accept`, payload
store, `ReadPayload`); `internal/worker` (pool + selection); `internal/run`
(runs lifecycle, failure policy); `internal/integrate` (extract / resolve /
merge / compile, the manifest); `internal/page` (pages + registry: subjects,
aliases); `internal/index` (hybrid retriever, FTS5 + vector lane);
`internal/lint`; `internal/read` (ask / search / timeline); `internal/mcp`;
`internal/consume` (eventplane consumer doors); `internal/llm` (the config-driven
call wrapper shared by every call site).

## Eval-engine enablement (a cross-cutting requirement)

`docs/wiki-redesign-research.md` builds an offline harness that sweeps
`{model} × {effort} × {prompt}` per inference site and scores the results. Its
foundational principle — **evaluate the production code path, never a
reimplementation** — is a constraint on *this* build. The harness (Part II)
can only ever score what Part I makes scorable, so these obligations are what
let Part II import and exercise the *real* call sites rather than a fork. Five
obligations fall on Part I; each is woven into the phase that owns it and
re-checked in that phase's **Eval hook**:

1. **Every inference site is a clean, externally-callable function**
   `f(cfg{prompt, model, effort}, input) → output`, with `(prompt, model,
   effort)` injected from config — never a constant, never read from env at the
   site, never buried un-callable inside the worker loop. "Production" is then
   just a pinned triple per site; the harness calls the *same* function with a
   different triple. The ten sites (research §"inference inventory"): extract,
   match, compile, merge, dup judge, canonical-name pick, ask, and the three
   retrieval lanes (candidates / search / sweep). This is design §10 made
   mechanical — the `internal/llm` seam (P2) plus a callable entry point per site.
2. **Every deferred knob is a config value, not a constant** — the harness tunes
   them: `WIKI_MATCH_EXCERPT_CHARS` (P6b2), candidate FTS thresholds (P6b), per-lane
   sweep thresholds (P9b), RRF `k` and `WIKI_EMBED_MODEL`/`WIKI_EMBED_DIMS` (P11),
   the ask turn/token/wall-clock budget (P10).
3. **Outputs preserve the dangerous-direction signal** so an asymmetric scorer
   can read it: match returns its binary verdict **and** the `dup_pairs`
   side-channel as distinct outputs (P6b2); compile emits per-claim `cites` and
   `occurred_at` (P8); the dup judge's ternary `merge | dismiss | can't-tell-yet`
   is preserved verbatim (P9a). Don't lump these into a single pass/fail.
4. **Free goldens are captured, not discarded** — the design's by-products are
   the harness's real-data anchors: `asks.question` (and answer/citations) stored
   (P10); the dup side-channel and `dup_flags` (P6b2/P9a); ingested documents + their
   extract output reachable for extract goldens (P3/P6a). Nothing in these phases
   may drop data the research doc names as a golden source.
5. **Mechanical invariants stay deterministically checkable** — citation
   preservation (§6.1, P7b), write-set conformance + claim-cite presence (P7a2) are
   exposed as the same pass/fail the harness scores merge on; `normalize`,
   older-ULID-wins, the version gate, brute-force cosine stay pure and unit-tested
   (research excludes them from inference scoring by construction).

These obligations add **no new phases** — they shape the phases below (each
addition subject to the phase-budget re-check rule above). The one
new artifact they justify is small and lands in P2: a documented **call-site
registry** (the canonical list of the ten sites with their injected-config
triple), so "is every site harness-callable" is a checklist, not an audit.

## Integration testing (a standing tier)

A second test tier runs the **real models against the real call sites** —
separate from the per-phase unit gates, which mock every LLM (from P6a onward)
and must stay deterministic, free, and offline so `go test ./...` remains the
trustworthy phase gate. This tier answers a different, narrower question: **can
the currently pinned production config actually run?** It exercises each call
site's **live `(prompt, model, effort)` triple** (design §10 / the P2 call-site
registry) end-to-end and asserts the output is *structurally valid* — never
whether it is *good*. Quality is Part II's graded sweep; this is binary liveness
and wiring.

What only a real call catches, and a mock never can: a prompt edit that makes the
model emit unparseable JSON, a wrong/renamed/deprecated model id, an effort level
the model rejects, real-API response-shape drift, or the abstention path /
citation gate failing to fire at all. What it deliberately does **not** catch — a
false-merge, a fabricated answer, weak extraction — is by construction a quality
judgment and belongs to Part II.

Rules that keep it honest:

- **Structural assertions only.** Each check asserts the mechanical invariants
  obligation 5 already exposes — page row + matching `[inbox-id]` citation,
  citation-preservation gate intact, a binary/ternary verdict that resolves, a
  vector of the configured dims — on *real* output. It never asserts content, so a
  nondeterministic model still yields a deterministic pass/fail.
- **Blunt fixtures only.** Cases where even a weak model is near-certain (identical
  name + corroborating claim → `same`; an obviously different type → `no_match`).
  Subtle / adversarial cases are Part II's, not this tier's — that is what keeps
  flake low.
- **Separate, gated, advisory.** Build-tag / env-gated (real calls need
  `ANTHROPIC_API_KEY` / `OPENAI_API_KEY` and the network), so it is **always in
  the tree but never in the unit gate**. Run on demand or on a cron; it is
  **advisory, never a deploy gate** — the same non-gating stance as Part II, for
  the same nondeterminism-and-cost reasons. A red run is a "go investigate"
  signal; mitigate residual flake with blunt fixtures, structural assertions, and
  a single retry.
- **Pinned to the live config.** Because it runs whatever triple config currently
  pins, the moment Part II's report (P16) swaps a default model or prompt this
  tier silently re-validates that the new pick at least *runs* — so a config
  change can't quietly ship a prompt the model chokes on.
- **Administered at named checkpoints, not merely present.** A tier that is "in
  the tree but never in the unit gate" is built and then never read: the unit
  gate mocks every LLM (from P6a onward) and a phase is *done* when it compiles
  and those mocked tests pass, so an integrator with a placeholder prompt (prompts
  are deferred config defaults — "Open items") can reach "all green" while its
  live `(prompt, model, effort)` triple has never once produced parseable,
  schema-valid output. To close that gap, three phases each **own a checkpoint as
  a line item in their `Verify` block** (not free-floating orchestrator prose)
  that **runs the accumulated tier, with keys present**: **P6a** (the first LLM
  site — extract — lands; catches an unparseable-JSON / wrong-model-id /
  mis-shaped-schema failure at the *first* site that has one, ~5 phases before P11
  and while the fix is one prompt), **P7a2** (the first full document-pass slice —
  extract + match + merge live), and **P11** (the full pipeline). Pinning the
  checkpoint to a phase's `Verify` — the gate `/finish` actually enforces when it
  closes a phase — is deliberate: addressed only to "the orchestrator," the run
  would never fire, because `/finish`'s orchestrator works *phases* and "does not
  do the hands-on work itself" (`docs/README.md`). A **red checkpoint pauses the
  march for investigation** — an explicit stop-and-look, **not** a hard CI/deploy
  gate (the advisory, non-gating stance above is unchanged for the same
  nondeterminism-and-cost reasons). And because the `/finish` environment may lack
  `ANTHROPIC_API_KEY` / `OPENAI_API_KEY` or the network, an unrunnable checkpoint
  **emits a visible `INTEGRATION CHECKPOINT SKIPPED — no keys` line rather than
  passing as if it ran** — so a skipped checkpoint is a recorded fact the
  coordinator sees, never an unnoticed absence (the "built then never read" hole
  this whole tier exists to avoid). The checkpoints are *when the already-built
  signal is read*, nothing more.

Like the enablement obligations, this adds **no new phases** — it grows one slice
at a time inside the call-site phases that already exist (within the phase-budget
re-check rule: when a slice overruns a phase, it splits via sub-letters, as P7a2
did). Each of **P6a–P11**
contributes its slice as its call sites land (see each phase's *Integration test*
line), so the full end-to-end real-model liveness check exists the moment P11
closes rather than being bolted on at the end — and the three checkpoints above
(after P6a, P7a2, P11) are where the `/finish` march actually *runs* that signal,
so a hollow integrator can't ride a green unit gate all the way to P11 unnoticed.

## Prompt-default validation (a standing gate)

The integration tier above is the only mechanism that runs a real model against a
real prompt — and it is **advisory and skip-on-no-keys** by construction (its
checkpoints emit `INTEGRATION CHECKPOINT SKIPPED — no keys` and the phase still
passes). That leaves a gap the tier was meant to close but cannot when `/finish`
runs without keys: a call-site phase reaches "all green" on a **placeholder
prompt** (prompts are deferred config defaults — "Open items"), because from P6a
on the unit gate mocks every LLM. Two failure modes hide in that gap, and they
are **not** the same:

- **(a) the prompt is structurally absent** — empty, a stub, or missing the
  sections the design pins for that site. This is **deterministic and detectable
  offline, with no key.**
- **(b) a real prompt makes a real model emit unparseable / mis-shaped output.**
  This is **nondeterministic and needs a live call** — correctly the advisory
  integration tier's job.

The plan hard-gates (a) and leaves (b) advisory, via two mechanisms:

1. **A deterministic, offline prompt-default gate, phase-owned in each call-site
   phase's `Verify`.** With no key or network it asserts, as an ordinary unit
   test: (a-i) the site's config-default prompt is **non-placeholder** — present,
   above a length floor, and carrying the structural sections the design pins for
   that site (extract's six §4.2, match's five §4.3, merge's six §4.4, compile's
   six-with-four-deltas §5, ask's six §9.2, and the lint judge / fold / stale-repair
   prompts §6); and (a-ii) the site **parses and schema-validates a committed,
   hand-authored, schema-faithful response fixture** (always present — no key, no
   network), so the parser + schema are exercised offline too. (A hand-authored
   fixture is exactly as good for the *structural* check, the only thing an offline
   test can assert; the **recorded** real-model fixture — which can only come from a
   live keyed call — refreshes this stub in **P11k**, where parser/schema drift is
   then also checked against real output.) Both halves are mockable and offline,
   so they run in the **unit gate `/finish` actually enforces** — converting "a
   placeholder rode a green unit gate to P11" from an un-gated advisory skip into a
   **deterministic phase failure**. It is pinned in each phase's `Verify` for the
   same reason the checkpoints are: a rule addressed only to "the orchestrator"
   never fires (see *Integration testing*). It deliberately does **not** assert
   (b) — whether the live model chokes stays the integration tier's advisory call.

2. **The three integration checkpoints become a Part-I exit obligation, owned by an
   explicit phase.** A `SKIPPED` checkpoint is no longer a silently-acceptable
   terminal state: each of **P6a / P7a2 / P11**'s checkpoint must have **at least one
   recorded non-skipped green run** before **Part II (P12)** begins. But this run is
   a **keyed, live-model action**, and the `/finish` orchestrator works *phases* and
   "does not do the hands-on work itself" (`docs/README.md`) — so a free-floating
   "run it on a keyed box on demand" precondition has no one in the march to fire it
   (the same trap the checkpoints' `Verify`-pinning avoids, see *Integration
   testing*). It is therefore discharged by a dedicated phase, **P11k** (the keyed
   Part-I validation gate, below), run on a keyed box at the P11→P12 boundary, which
   is **keys-REQUIRED and HALT-not-SKIP**: absent keys/network it **stops the march
   and surfaces to the human** rather than marking Part I done. This keeps the
   per-phase tier advisory (no nondeterministic hard gate — the stance of
   *Integration testing* is unchanged) while forbidding "every real-model check went
   dark" from counting as a **successfully executed Part I**. Pinned at the P11→P12
   boundary (P11k, P11's checkpoint, and the Part II preamble) so it is enforced,
   not free-floating.

Mechanism 1 grows one `Verify` line at a time inside the call-site phases that
already exist (P6a, P6b2, P7a2, P8, P9a, P9c, P10) and adds **no new phase**
(but see the phase-budget re-check rule — these added `Verify` slices are exactly
what split P7a2 off P7a); mechanism 2 adds exactly **one** phase — **P11k**, the keyed Part-I validation gate
at the P11→P12 boundary — the single keyed, live-model step the otherwise-phase-only
`/finish` march needs an explicit owner for.

## Keys present before the march (a standing precondition)

The integration checkpoints (P6a / P7a2 / P11) and the keyed exit gate (P11k)
deliver their early-warning value only if the live `(prompt, model, effort)`
triples can actually run; absent keys they emit `INTEGRATION CHECKPOINT SKIPPED —
no keys` and defer the whole live-model signal to one late P11k big-bang. So the
precondition: **before the `/finish` march starts, both `ANTHROPIC_API_KEY` and
`OPENAI_API_KEY` are wired into the local environment** via the `.envrc` pattern
sourced from `~/.secrets/` — `wiki/.envrc` adds the `OPENAI_API_KEY` export beside
its existing `ANTHROPIC_API_KEY` one, then `direnv allow`. With both present the
checkpoints fire at their designed points instead of skipping, so a broken prompt
/ wrong model id / rejected effort surfaces ~5 phases before P11 "while the fix is
one prompt." One `OPENAI_API_KEY` covers OpenAI chat **and** embeddings (one key,
both endpoints — design §9.3); there is no separate embeddings key. These are the
**local/dev** credentials that run the march and the keyed P11k gate; the **box**
credentials are seeded separately by `wiki/bin/secrets` in the deploy phase (P11d).

This precondition is **enforced by a phase, not left to operator memory** — the
free-floating-prose trap mechanism 2 already named ("a free-floating … precondition
has no one in the march to fire it"), here applied to the *entry* the way P11k
applied it to the *exit*. **P0** (below) is the first thing the `/finish` march runs:
a deterministic, offline presence check that **HALTs before P0a if either key is
absent**. So "keys are wired" is verified by the march itself at phase 0 — a keyless
run fails immediately at zero cost rather than silently skipping every checkpoint and
only surfacing at the P11k big-bang ~24 phases later.

---

## [x] P0 — Preflight: both keys present (the entry HALT gate)

*The single deterministic, offline gate that makes the standing precondition above
**enforced by the march itself**. It is the **entry mirror of P11k**: where P11k is
the keys-REQUIRED / HALT-not-SKIP **exit** gate at the P11→P12 boundary, P0 is the
keys-REQUIRED / HALT-not-SKIP **entry** gate before P0a. Run by the `/finish`
orchestrator as the very first step — like reading the plan, this is an orchestrator
preflight, not delegated worker code. It ships no product code and makes **no
network / live call**: presence only; live-triple validity stays the checkpoints'
(P6a / P7a2 / P11) and P11k's job.*

- **Assert both keys are present** in the march environment — `ANTHROPIC_API_KEY`
  **and** `OPENAI_API_KEY` set and non-empty (one `OPENAI_API_KEY` covers OpenAI
  chat **and** embeddings — design §9.3 / the precondition above). A bare presence
  test (`test -n "$ANTHROPIC_API_KEY" && test -n "$OPENAI_API_KEY"`), never a
  provider call.
- **HALT-not-SKIP on absence.** If either key is absent the gate **does not pass**
  — it **halts the march and surfaces to the human** with a visible
  `PRE-MARCH KEY GATE BLOCKED — no keys` line, and **P0a is not dispatched**. This
  is deliberately the *opposite* of the per-phase checkpoints' advisory
  skip-on-no-keys stance: those are mid-march signals, but the one precondition they
  all depend on is checked **once, hard, up front** — so a keyless run fails at
  phase 0 (cost: nothing) instead of skipping every checkpoint and only halting at
  P11k ~24 phases later (cost: the whole march's early-warning value).
- **Presence, not validity.** P0 proves the keys are *wired*, which is all an
  offline deterministic gate can prove; whether a live model accepts the pinned
  `(prompt, model, effort)` triple is the advisory checkpoints' and P11k's job. P0
  exists so those checkpoints **fire at their designed points instead of skipping** —
  it guarantees their one shared precondition before any phase spends work.

**Touches:** nothing — an orchestrator-run environment gate; no files, no code.
**Verify:** with both keys present the gate passes and the march proceeds to P0a;
with either key absent the march halts **before P0a** with the visible
`PRE-MARCH KEY GATE BLOCKED — no keys` line and surfaces to the human; the check
makes no network call (presence only). **This gate, like P11k, is never "done" while
skipped.**

---

## [x] P0a — agentkit OpenAI chat backend (Responses API)

*Shared-library expansion; the deepest chat prerequisite. A near-mechanical
**port** of the proven backend in the sibling repo `/mnt/projects/ikigai-cli`
(`app-root/internal/provider/openai/openai.go`, governed by its
`reqs/providers.md`) into this repo's agentkit — not a green-field
implementation. P2's call-site wrapper resolves an OpenAI model through it, and
Part II can only sweep OpenAI once it exists. Blast radius across every
agent-backed service (prompts, dropbox, wiki) — hence its own phase with a
build-everything gate.*

The groundwork is already laid: `agentkit/provider` defines the provider-neutral
`Client` interface (R-S04B-QD3D), and `agentkit/model` already knows
`ProviderOpenAI`, the prefix→provider mapping, and a `gpt-5.5` registry entry
with effort vocabulary and pricing (`registry.go:168`). The two repos'
`provider.Client` interfaces are **identical except this repo added
`Request.MaxTokens`**, so the port is import-path + one-field work, not a
reshape.

- **`agentkit/provider/openai/`** — port the ikigai-cli backend, preserving its
  behavior: POST `/v1/responses` with SSE streaming (R-WWTI-LSSO; **Chat
  Completions is not used** — the Responses API is the only surface exposing
  `reasoning.encrypted_content`). Normalize the stream into the existing wire
  events; **OpenAI encrypted reasoning round-trips via `ThinkingBlock`**
  (`Text` = reasoning-item id, `Signature` = `encrypted_content`; `store:false`
  + `include:["reasoning.encrypted_content"]` — R-3D9Z-4ND7, satisfying
  R-ROBI-V64M for OpenAI). Native structured output via `text.format.json_schema`
  + `strict:true` with recursive strict-mode schema adaptation (R-3Z86-0IPP,
  R-3V3G-PYML); effort → `reasoning.effort` (R-22XS-LD6T); typed errors from
  `error.type` (R-574J-S9EP); usage from `response.completed` (R-ZEVA-05QR). The
  agent loop and wire codec **must not** import the subpackage (the interface is
  the only seam).
- **The port deltas (the only non-mechanical work):** honor this repo's
  `Request.MaxTokens` as `max_output_tokens` (the one interface field ikigai-cli
  predates); extend the composition-root `clientFactory`
  (`prompts/internal/runner/runner.go:43`) to dispatch `gpt-*` → `openai.New`;
  carry `SetBaseURL` for the offline test gate. Confirm the `gpt-5.5` registry
  entry (models, accepted effort levels, pricing — a model with unknown pricing
  cannot ship, R-ZZLK-I9CK).
- Key from `OPENAI_API_KEY`, presence-gated at the composition root (the
  `ANTHROPIC_API_KEY` pattern; absent key = OpenAI models unavailable, not a
  crash). Secret reaches the process via the `.envrc` pattern — never read.

**Touches:** `agentkit/provider/openai/` (new), `agentkit/model/registry.go`
(confirm OpenAI entries), the composition-root `clientFactory`,
`agentkit/CLAUDE.md`. **No wiki code yet.**
**Verify:** `go test ./agentkit/...`; an OpenAI `Stream` round-trips a tool call
and a reasoning signature against a **hand-authored SSE fixture over an
`httptest` server** (the `anthropic_test.go` pattern — `SetBaseURL`, no live
key, fully offline); structured-output, effort, and error-mapping cases mirror
the ikigai-cli tests; **all agent-backed services still build** (prompts,
dropbox, wiki) — the reason this seam is isolated.

## [x] P0b — agentkit embeddings client (`agentkit/embed`)

*Shared-library expansion; a **net-new** capability with no prior art
(`/mnt/projects/ikigai-cli` has no embeddings). Embeddings are **not** a generation call,
so they live in a dedicated sibling library, not the chat `Client` interface.
**First and only consumer is P11**; built here so the shared library is touched
once. Same build-everything gate.*

- **`agentkit/embed/`** — a small provider-neutral interface, e.g.
  `Embedder.Embed(ctx, model string, dims int, texts []string) → ([][]float32,
  error)`, with an OpenAI implementation for `text-embedding-3-large` @
  configurable dims. **Unary JSON POST** (not SSE); **one HTTP request per
  `Embed` call** over the whole `texts` slice — the caller (P11's catch-up
  worker) owns chunking to the provider's array/token limits, so "one API call"
  stays literal and the lib hides no fan-out. `dims` passed through verbatim (the
  provider rejects an illegal value); typed errors reuse `provider.Error`;
  `input_tokens` read from the response's `usage.prompt_tokens`.
- **Pricing:** add an embeddings-pricing entry for `text-embedding-3-large` (the
  published per-million **input**-token rate; output/cache = 0) so the embed
  call's `cost_usd` (P0c) is real, not zero (R-ZZLK-I9CK — every billed model
  declares its rate). The chat `modelSpec` is chat-shaped (efforts,
  maxOutputTokens), so this is a small dedicated embeddings-pricing path, not a
  chat-registry row.
- Key from `OPENAI_API_KEY`, presence-gated at the composition root (absent key →
  the embedder is simply not constructed; P11's vector lane falls back to
  lexical-only, design §9.3). The constructor rejects an empty key (the
  `anthropic.New` / `openai.New` pattern).

**Touches:** `agentkit/embed/` (new), the embeddings-pricing data,
`agentkit/CLAUDE.md`. **No wiki code yet.**
**Verify:** `go test ./agentkit/...`; the client returns correct-dimension
vectors and surfaces `usage.prompt_tokens` against a **canned embeddings
response over an `httptest` server** (offline, no live key); an absent key is a
clean construction refusal, not a panic; **all agent-backed services still
build.**

## [x] P0c — Per-call usage/cost logging + Anthropic effort parity

*Shared-library cross-cutting slice. Makes the AI library emit **one jsonl line
per API call** (chat and embed, every backend) into the app's structured log,
and closes the one remaining provider-uniformity gap (Anthropic effort).
Deliberately separate from P0a/P0b because it also modifies the **existing**
Anthropic backend. Depends on P0a + P0b (it wires logging into all three call
paths).*

- **One `slog` record per API call** (the requirement): every agentkit
  provider/embed call emits exactly one record at the point usage is known — per
  chat `Stream` completion (so a multi-turn agent run emits one line **per
  turn**, each turn being one HTTP request) and per `Embed` call. The sink is an
  injected `*slog.Logger` (stdlib — **no `agentkit`→`appkit` dependency**; the
  service passes its `appkit/logging` JSON logger in, so the line lands in the
  app's log as one JSON object — `appkit/logging/logging.go:42`); a **nil logger is a
  no-op**, exactly like `*trace.Tracer`. `trace.Tracer` stays the separate
  `--raw` debug channel; this accounting line is distinct and always-on.
- **Fields:** `provider`, `model`, `effort`, `input_tokens`, `output_tokens`,
  `cache_read_tokens`, `cost_usd` (from `model.ModelPricing(r).ComputeCost(...)`
  for chat, the embeddings rate for embed), `duration_ms`, and `stop_reason`
  (chat only). **Attribution without coupling:** agentkit knows no wiki
  call-sites — the caller pre-binds them, e.g. P2's `internal/llm` wrapper passes
  `logger.With(slog.String("call_site","extract"), slog.String("run_id", id))`
  and the backend only appends the accounting fields. This hands **Part II's
  cost/latency reporting** per-call, per-site data for free (research doc's
  report rows = score **+ cost + latency**).
- **Anthropic effort parity:** wire `req.Effort` → adaptive-thinking on the
  Messages API in `agentkit/provider/anthropic/` (today effort is silently
  dropped — `buildPayload` reads neither effort nor schema). Structured output
  already works uniformly via the agent loop's parse+validate+retry
  (R-WFWM-BKWX, `agentkit/agent/loop.go:197`), so **no structured-output work is
  needed** — only effort. Model + effort are **build-time config**, validated at
  startup per-model (R-ZX67-O1L1); there is no runtime selection — Part II
  varies the triple only by injecting an alternate config into the same call-site
  function.

**Touches:** `agentkit/provider/{anthropic,openai}/`, `agentkit/embed/`, a small
shared logging helper (record assembly), `agentkit/CLAUDE.md`.
**Verify:** `go test ./agentkit/...`; a captured `*slog.Logger` (buffer handler)
receives **exactly one JSON record per API call** for Anthropic chat, OpenAI
chat, and embed, each carrying a non-zero `cost_usd` matching the registry
computation and the caller's pre-bound `call_site`; a nil logger emits nothing;
an Anthropic request with effort sends adaptive-thinking on the wire; **all
agent-backed services still build.**

## [x] P1 — Decisions lock + consolidated schema

*Design §12 (the authoritative schema final), plus §2.2, §4.1, §4.5, §5, §6, §7,
§9.2, §9.3, §11. The prerequisite for everything; resolves the open schema/fork
items before any code depends on them.*

- **Lock the one build-time fork — the digest claimable unit** (design §11).
  *Framing 1*: claim the whole cron row, run its bound entries sequentially
  (simplest selection; needs a leave-pending-if-locked-out path). *Framing 2*:
  the claimable unit is a `(cron-row, entry)` pair (uniform selection, pure
  completion-time-join stamp, no leave-pending case; costs a cron-row→entry
  expansion in selection). **Recommendation: Framing 2** — it keeps the
  selection critical section uniform and removes a special case from the spine,
  which matters most given P4 is built before any digest exists. Record the
  decision at the top of this file once locked.
- **Decommission the legacy wiki — treat the service as empty** (design §1: a
  clean rewrite, nothing preserved). The existing `wiki/` is a full prior
  service that the rewrite must clear *before* P2 scaffolds the fresh one: a Go
  tree (`cmd/wiki/main.go` +
  `internal/{ask,consume,ingest,jobstore,lint,mcp,search,store,ids}`, referencing
  the dead `wiki_ingest` / `wiki_jobs` schema) and **three committed, immutable
  migrations** (`001_schema_migrations.sql`, `002_wiki.sql`,
  `003_feed_offset.sql`). The wiki holds **no data worth keeping — the `int` box
  DB is disposable** — so the teardown is unconditional:
  - **`git rm` the entire legacy Go tree** — every `wiki/internal/**` package and
    `wiki/cmd/wiki/main.go` — so P2's "fresh `wiki/`" starts from an **empty
    `internal/`** with no stale files to overwrite-or-collide (several legacy
    package names — `consume`, `lint`, `mcp`, `search` — recur in the new layout
    with different contents; a partial overwrite would leave a broken build,
    blowing the phase's context budget).
  - **Do not delete, edit, or rename the three legacy migrations** —
    `bin/check-migrations` forbids it (immutability is CI-enforced; the obvious
    "just delete `002_wiki.sql`" reflex trips a red build). Instead make the
    **first new migration a drop-legacy step** —
    `DROP TABLE IF EXISTS wiki_ingest; DROP TABLE IF EXISTS wiki_jobs;` (indexes
    drop automatically with their parent table) — so every DB, fresh box or
    existing, converges to the clean schema *after* the frozen old migrations
    replay forward. Create it first (via `bin/new-migration wiki drop_legacy`) so
    its timestamp sorts ahead of the consolidated DDL below;
    `001_schema_migrations.sql` is the shared appkit bookkeeping table and stays.
  - **Do NOT drop `feed_offset`** (design §12.1) — it is the eventplane consumer
    cursor store, owned by the library (`consumer.SchemaSQL`) and merely *applied*
    by the frozen `003_feed_offset.sql`, which stays in the set and keeps creating
    it. The new design's consumer doors (§2.1) still read/commit cursors there;
    dropping it breaks the consume side. (`003`'s body comment about the retired
    `raw/` store is stale, but the file is immutable and its DDL stays correct.)
  - **Add the producer `outbox` migration** (design §12.1) — the old wiki was
    consumer-only and carries no outbox table, but the rewrite is an eventplane
    **producer** (§8). Like `crm`/`ledger`'s `003_outbox.sql`, add a **new**
    migration (`bin/new-migration wiki outbox`) whose body is **byte-identical to
    `outbox.SchemaSQL`** (`CREATE TABLE outbox (...) ; CREATE INDEX
    idx_outbox_created_at ...`), and a `migrations_outbox_test.go` asserting that
    byte-identity (the established producer pattern). Without it the §8 outbox
    writes (`wiki.row_dead_lettered`, `wiki.ingest_refused`) have no table.
- **Transcribe design §12 (the schema final)** into ordered migrations via
  `bin/new-migration wiki <name>` (never hand-pick a number; never edit a
  committed migration), landing **after** the drop-legacy migration above. §12 is
  the single source — copy it table for table. Do **not** re-derive the DDL from
  the scattered riders in §2.2/§4.1/§6/§9.3, and do **not** add, drop, or rename a
  column/constraint/index relative to §12. If §12 looks wrong or incomplete, **fix
  §12 first** (and surface it), then transcribe — the migration and §12 must never
  diverge. §12 already incorporates the gaps a prose reading misses (`aliases.type`,
  `dup_flags.run_id`, the full `stale_notes` columns, the dropped `subjects.page`),
  so transcription is mechanical, not a synthesis.
- **The named literals are already pinned in §12.3** — the eventplane
  system-identity `owner` value (`system@ikigenba`) and the `wiki.row_dead_lettered`
  / `wiki.ingest_refused` payload shapes (§8). No literal is invented in this phase.

**Touches:** removal of the legacy `wiki/cmd/wiki/` + `wiki/internal/**` Go tree;
`wiki/internal/db/migrations/*.sql` (the drop-legacy migration, the new `outbox`
migration, + the consolidated DDL transcribed from design §12); schema test +
`migrations_outbox_test.go` (byte-identity to `outbox.SchemaSQL`); and the top of
this file (locked fork decision).
**Verify:** the legacy Go tree is gone (`grep -rn 'wiki_ingest\|wiki_jobs'
wiki/cmd wiki/internal` is empty); `bin/check-migrations` passes (old migrations
untouched, only adds); migrations load forward-only and drop the legacy tables;
downgrade guard intact; the schema test asserts every table/column/constraint/index
**against design §12** (checked against the external spec, not against this phase's
own reading — so an omission cannot hide in both the DDL and the test). After
migrate, **`feed_offset` and `outbox` both exist** (consumer cursor preserved,
producer outbox added) and the `outbox` migration is byte-identical to
`outbox.SchemaSQL`. `go test ./wiki/internal/db/...`.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
because P1's artifacts are immutable and underpin all of Part I, the coordinator
confirms each load-bearing deliverable is present and individually covered *before
closing the phase* — not merely that the suite is green: (1) the locked digest-fork
decision recorded at the top of this file; (2) the legacy Go tree removed; (3) the
drop-legacy migration; (4) the `outbox` migration **and** its `migrations_outbox_test.go`
byte-identity assertion; (5) `feed_offset` preserved (not dropped); (6) **every** §12
table, index, and constraint transcribed (all nine application tables plus
`pages_fts`); and (7) the schema test asserting that full set against the external
§12 spec. A missing item here is a **loud failure at this boundary**, never a silent
partial that a green mocked DB test carries into the immutable foundation.

---

## [x] P2 — Service scaffold + config-injection seam

*Design §1, §10. Establishes the chassis and the config discipline every later
phase plugs into.*

P1 has already `git rm`ed the legacy `wiki/` Go tree, so `internal/` starts
**empty** — every package below is net-new and overwrites no stale file.

- `cmd/wiki/main.go` on the appkit verb dispatcher: `serve` / `version` /
  `manifest` / `migrate` / `backup` / `restore`. `GOWORK=off` production build
  green; wired into `go.work` for local dev.
- `internal/config`: config-from-env at the composition root, including the
  **per-call-site seam** — a typed config carrying `{prompt, model, effort}` for
  each of the eight LLM call sites of design §10 (extract, match, merge, compile,
  ask, and the three lint calls — dup judge, fold, stale repair) plus the embed
  model/dims. (The harness-callable *registry* below is a superset — it also
  tracks the canonical-name pick and the three retrieval lanes, the research
  doc's ten inference sites.) Validated at the `serve`
  boundary, injected inward; **no call site ever reads env or a constant**
  (design §10). Defaults may be placeholders here — each call-site phase fills
  its real default.
- `internal/llm`: the thin wrapper a call site calls with injected
  prompt+model+effort (so adding a call site is uniform). Structured-call and
  agent-run shapes, over the agentkit provider abstraction — so a call site's
  `model` may resolve to **either** an Anthropic or an OpenAI model (the P0a
  backend), chosen purely by config. **The wrapper is the enablement seam** — a
  site that goes through it is automatically harness-callable with a swapped
  triple (enablement obligation 1).
- **The call-site registry**: a documented, single-source list of the ten
  inference sites (extract, match, compile, merge, dup judge, canonical-name
  pick, ask, candidates, search, sweep). The registry is a **superset** of the
  eight config-injected sites above: each config-injected site names its own
  injected-config triple and callable entry point, while the canonical-name pick
  and the three retrieval lanes (candidates, search, sweep) are tracked as
  scoreable sites with **no triple of their own** — canonical-name pick rides the
  dup-judge call's triple and entry point (it is a field of the dup-judge output,
  design §6), and the retrieval lanes are zero-LLM. Sites are added to it as their
  phases land; P2 establishes the registry and the convention so "every site is
  harness-callable" is a checklist by the end, not a retrofit.
- MCP server skeleton (`internal/mcp`): register the tool surface
  (`ingest_text`, `ingest_url`, a status verb, `search`, `ask`, `timeline`) as
  stubs returning not-implemented; the `reflection` + `health` tools live.
- eventplane producer: declare the outbox and the **two** event types (§8); not
  yet emitted. eventplane **consumer** doors deferred to P3.
- **Box secret wiring for OpenAI**: the rewrite adds an OpenAI dependency
  (embeddings always; chat wherever a site pins `gpt-*`), so `wiki/bin/secrets` —
  which today seeds only `ANTHROPIC_API_KEY` + `WIKI_OWNER` — gains an
  `OPENAI_API_KEY` key (same `resolve()` from `~/.secrets/`, masked, never
  printed), and `wiki/.envrc` exports `OPENAI_API_KEY` for local dev. The box-side
  seeding *run* is P11d; this phase lands the committed script + `.envrc` change so
  `bin/ship`'s `main`-HEAD build carries it.

**Touches:** `wiki/cmd/wiki/`, `wiki/internal/{config,llm,mcp}/`, `wiki/bin/secrets`,
`wiki/.envrc`, manifest emit, `go.work`.
**Verify:** all verbs dispatch; `manifest`/`version` correct; MCP server starts
and lists tools; `go build` under `GOWORK=off`; no legacy package remains
(`grep -rn wiki_ingest wiki/internal wiki/cmd` empty); all 7 services still build.
**Eval hook:** the call-site registry exists; the `internal/llm` wrapper accepts
an injected `(prompt, model, effort)` triple and no site can reach a model
without it (obligations 1–2). A trivial test swaps the triple on a stub site.

---

## [x] P3 — The ingest side: `Accept`, the inbox, front doors

*Design §2.1, §2.2, §8. The whole write path, no integration yet.*

- `internal/inbox`: the single `Accept(owner, kind, source, mime, title, tags,
  bytes) → (id, sha256, dup, err)` — hash, size, inline-vs-spill decision, row
  insert, worker nudge. Synchronous, transactional, **never calls an LLM**.
- Payload store: inline (`content` ≤ `WIKI_INBOX_INLINE_MAX`, default 4096) vs.
  content-addressed `blobs/<aa>/<sha256>` (write→fsync→insert). The lone
  `ReadPayload(row)` accessor; readers dispatch on the row, never the threshold.
- Size cap: refuse `> WIKI_INGEST_MAX_BYTES` (default 131072) loudly at the
  door; **no ingest-side chunking**. Refusal emits `wiki.ingest_refused` (plain
  outbox write, pre-accept).
- Front doors: `ingest_text` → `Accept` directly; `ingest_url` → fetch+extract
  server-side then `Accept`; the **receipt contract** (return inbox id + sha256
  + dup flag — not a job id). The status verb polls integration state against an
  inbox id.
- eventplane **consumer doors** (`internal/consume`): one goroutine per
  subscribed feed; cursor commits **after** `Accept` returns (at-least-once,
  hash dedup). Per-handler mapping: domain event → `Accept(kind=event)`; dropbox
  file event → fetch `content_url` → `Accept(kind=document)`; `cron.<name>` →
  `Accept(kind=event, source="cron:<name>")`. Consumer doors stamp the
  system-identity `owner` from P1. Non-interactive doors notify on refusal.

**Touches:** `wiki/internal/{inbox,consume,mcp}/`, blob dir handling, outbox.
**Verify:** text/url accept; inline & spill paths; `sha256` always stored; dup
flag on re-accept; oversized refusal emits the event; consumer cursor advances
only post-`Accept`; crash before integration loses nothing. Unit + table tests.

---

## [x] P4 — The dispatcher-free worker spine (stubbed integrators)

*Design §3, §4.5 (runs lifecycle). The seam-forcing phase: the spine exists and
is verifiable before any real integrator does.*

- `internal/worker`: `WIKI_INTEGRATION_WORKERS` (default 4) identical goroutines,
  each looping over the **selection critical section** under one in-flight-set
  mutex: (1) a pending cron row whose bound job name(s) aren't in flight →
  `TryLock` job name; (2) else the oldest pending **document** row not in flight
  → `TryLock` row id; (3) else block on the wake `Cond`. **Cron before
  documents.** Event rows are invisible to selection. Run claimed work with **no
  lock held**, commit, drop the claim, loop.
- Per the locked fork (P1): if **Framing 2**, selection expands a cron row into
  `(cron-row, entry)` candidates; a cron row no job binds is stamped immediately
  as a no-op.
- The nudge is an **optimization, not the truth**: contentless wake; every wake
  (and boot) re-runs selection against the inbox. Wake sources: arrival nudge,
  run completion, shutdown (the `ineligible_until` timer joins in P5).
- **The swap-boundary contract** (`internal/integrate`): the two cross-phase
  types every integrator hands off through, pinned *here* — before any real
  integrator exists — so P6a–P8 drop in onto a working whole instead of reshaping
  the spine (this is the swap-is-mechanical bet of the Sizing principle made
  concrete):
  - **`Manifest`** — the in-memory work order, **never persisted** (the run id is
    its durable identity): extracted/compiled `subjects[]`, each annotated with
    its resolved subject_id + target page, a **per-page base `version` slot** (the
    `pages.version` the page was read at — design §3: "the manifest records the
    version merge read"; populated at merge-read time in P7a, consumed by P7b's
    optimistic-commit `WHERE subject=? AND version=?` guard — pinned **now** so
    P7b reads an existing field instead of reshaping the frozen contract), plus a
    **generalized `{text, cites[]}` claim shape** (the document pass fills `cites`
    with the one inbox row id; compile fills per-claim cites — §4.3, §5). Defined
    **empty-capable** so a stub can emit a minimal one (the stub fills the version
    slot with a dummy value to round-trip it — see Verify). Its **complete** field
    set, and the consumer that reads each field, are fixed by the *Manifest
    canonicity* standing rule above — the authoritative contract P4 transcribes
    into the Go type (the seam's §12), not re-derived from prose here.
  - **`Integrator`** — the interface the document-pass stub, the cron/no-op stub,
    the real document pass (P7a), and the real compile (P8) **all** satisfy:
    claim → run → produce a `Manifest`. The shared resolve→merge→commit pipeline
    and the end-of-run transaction consume a `Manifest`, never integrator-specific
    data — which is exactly what lets merge "not tell which integrator ran" (P8).
  - **Manifest field obligations — transcribed from *Manifest canonicity*, not
    re-derived here.** P4 pins the `Manifest` type against the authoritative field
    enumeration in the *Manifest canonicity* standing rule above (the external
    spec — the seam's §12), **not** against P4's own reading of the design's
    scattered prose. Every field each downstream consumer reads (P6b2, P7a, P7b,
    P8) is named there with its consumer; P4 transcribes that contract into the Go
    type **in full**, not whittled to the stub's minimal needs — so no field a
    later consumer needs is discovered only after the contract is frozen (the
    failure this split is built to prevent: a downstream concurrency phase —
    P7b/P7b2 — silently reshaping a spine type). A field a later phase finds
    missing is added **to that enumeration first** (and the producers updated),
    exactly as a schema gap is added to §12 first — the seam and its consumers
    never diverge.
- `internal/run`: the `runs` lifecycle — insert `running` before execution (the
  one write outside the commit); the **generic end-of-run transaction wrapper**
  that **takes a `Manifest` and** atomically writes the run's terminal
  `succeeded` + `integrated_by` stamp (real page/registry writes arrive in P7a,
  but the transaction *shape* — and its `Manifest` input — is built and tested
  here). **Stamp only at commit, never at claim.** Boot sweep flips orphaned
  `running` → `crashed`. `TryLock` keys: job-name for cron/digest/lint, row-id
  for the document pass.
- **Stub integrators**: a document-pass stub and a cron/no-op stub that
  **implement `Integrator`** and exercise claim → run → commit → stamp without
  LLM work — each **emitting a minimal `Manifest`** the end-of-run transaction
  round-trips (a stub can be told to fail, for P5).

**Touches:** `wiki/internal/{worker,run,integrate}/`.
**Verify:** rows get claimed exactly once (no double-claim under N workers);
cron-before-document priority; crash between claim and commit leaves the row
**pending** and restart re-selects it; boot sweep marks orphans `crashed`;
selection stays oldest-first (no starvation). The stub integrators **implement
the `Integrator` interface and round-trip a populated `Manifest`** — including a
populated per-page base `version` slot — through the end-of-run transaction (the
*round-trip* test: a populated manifest survives the commit). **Separately, a
table-driven `Manifest`-completeness test asserts the seam's *completeness* against
the canonical contract:** its cases are the *Manifest canonicity* enumeration
verbatim — one per field, paired with its declared consumer — and it fails the phase
if any named field is absent from the Go type (checked against the **external spec**,
not P4's own reading; the in-memory mirror of the §12 schema test). The round-trip
test alone **cannot** catch an omitted field — the stub never populates a field the
type lacks — so *completeness* is the table-driven test's job *here*, not a P7b-time
audit. Concurrency tests.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
P4 freezes the load-bearing swap-boundary contract, so — not merely that the suite is
green — the coordinator confirms before closing the phase: (1) the `Manifest` Go type
and the `Integrator` interface exist; (2) the **table-driven `Manifest`-completeness
test** is present and its cases cover **every** field in the *Manifest canonicity*
enumeration with its declared consumer (a missing case is a missing assertion — the
exact omission this gate exists to make loud); (3) the stub integrators round-trip a
fully-populated `Manifest` — including the per-page base `version` slot — through the
generic end-of-run transaction; (4) the claim-once + boot-sweep concurrency properties
hold. A field absent from the enumeration's coverage is a **loud failure at this
boundary**, never a silent gap that surfaces at P7b with no additive escape hatch.

---

## [x] P5 — Failure policy: bounded retries + dead-letter

*Design §7, §8. Completes the spine's resilience story on top of P4's stubs.*

- The pending predicate: `integrated_by='' AND dead_at IS NULL AND
  (ineligible_until IS NULL OR ineligible_until <= now)`.
- `ineligible_until` set on **every** failed run (no transient/persistent
  dispatch): `now + random(2–4) × avg_run_duration × 2^(failures−1)`,
  `avg_run_duration` from recent `runs` floored at 60s; `failures` counted since
  the last re-queue. Workers gain the **timer wake source** (armed to the
  earliest future `ineligible_until`).
- `dead_at` set at the threshold `WIKI_RUN_ATTEMPTS_MAX` (default 5); clears
  `ineligible_until` in the same UPDATE; **emits `wiki.row_dead_lettered`** in
  that transaction. `requeued_at` scopes the retry counter (re-queue = clear
  `dead_at`, grant a fresh budget). `runs` stays the single source of truth (no
  denormalized counter).
- The check lives **at failure time**, applied by whatever marks the run (and
  the boot sweep when marking `crashed`); selection stays policy-free. **All
  failures count** toward the threshold (no exempt type).

**Touches:** `wiki/internal/run/` (failure path), `wiki/internal/worker/` (timer
wake), outbox.
**Verify:** failed stub run backs off then re-selects; N failures dead-letter +
emit the event + clear `ineligible_until`; re-queue restores a fresh budget;
crashed orphans count one attempt. Time-driven tests with an injected clock.

---

## [x] P6a — Document pass: registry primitives + extract (manifest input)

*Design §4.2, §4.1 (registry). The first real integrator's front half: the page
registry, the `normalize` function, and the extract call. Resolve and candidates
are P6b, match and the manifest P6b2; merge/commit close the loop in P7a–P7b2.
Split from the old monolithic P6
— two of the system's hardest prompts (extract, match) plus the candidates design
in one phase overran the one-subagent cold-start budget — cut along the
extract-output data contract (§4.2) so each half carries one hard prompt.*

- `internal/page` registry primitives: `subjects` + `aliases`; the `normalize`
  function (NFKC, casefold, trim, collapse whitespace, strip diacritics —
  pure, versioned, rebuildable).
- **Extract** (`internal/integrate`): one full-context structured, tool-less LLM
  call (config-injected prompt+model+effort). Input = whole document + a
  mechanical **context header** ("received on", never "today is") + the subject
  schema. Output JSON: subjects `{type, kind, name, aliases, claims[],
  occurred_at}`. Within-document co-reference only; names as the document states
  them; self-contained claims; keep-it-relative on unresolvable time;
  salience gate (when in doubt, do not extract). Dialog-aware clause for
  transcripts.
- **The P6a→P6b seam** is the extract output: schema-valid `subjects[]` JSON, the
  data contract design §4.2 already pins. P6a ends there; P6b consumes it. Nothing
  in P6a touches resolution, the registry lookup, or the manifest.

**Touches:** `wiki/internal/{integrate,page,llm,config}/`.
**Verify:** extract goldens with a mocked LLM; `normalize` unit tests; extract
emits schema-valid `subjects[]` (the §4.2 contract / the P6b seam). No resolution,
no writes to pages yet.
**Prompt gate (standing, offline — see *Prompt-default validation*):** the extract
config-default prompt is non-placeholder and carries its six §4.2 sections; the
extract parser schema-validates a committed, hand-authored, schema-faithful response fixture
into `subjects[]`. Deterministic unit assertions that fail the phase if the prompt
is a stub — independent of keys, so they hold even when the checkpoint below skips.
**Checkpoint (phase-owned, the first of three):** run the accumulated integration
tier (`go test -tags=integration ./wiki/...`, keys present) — extract is the first
live LLM site, so this is the earliest a wrong/renamed model id, an effort level
the model rejects, or an unparseable-JSON / mis-shaped-schema prompt can surface.
A **red run pauses the march** for investigation (advisory, not a deploy gate). If
`ANTHROPIC_API_KEY`/`OPENAI_API_KEY` or the network are absent, emit a visible
`INTEGRATION CHECKPOINT SKIPPED — no keys` line — **never pass as if it ran**.
This rides the phase's `Verify` so `/finish` triggers it when closing P6a.
**Eval hook:** extract registered as a harness-callable site (obligation 1);
extract output is reachable as a golden alongside its source document
(obligation 4).
**Integration test:** the standing tier's first slice (extract half) — real
extract on a blunt fixture document through the **live pinned triple**: assert
extract returns schema-valid `subjects[]` (required fields present, claims free of
pronouns / document-relative refs). Match's slice lands in P6b2; no page assertions
yet — the commit lands in P7a.

---

## [x] P6b — Document pass: resolve + candidates (the mechanical resolution half)

*Design §4.3, §4.1 (registry). The first real integrator's **mechanical**
resolution arms and the candidate-retrieval step — **zero LLM, fully
deterministic**. The match judgment and the manifest are **P6b2**; merge/commit
close the loop in P7a–P7b2. Carved off P6b under the phase-budget re-check rule:
the match prompt (one of the system's two hardest) plus its whole validation
surface (prompt gate + eval hook + a live integration slice) overran the
resolution arms it shipped with — the same cut, along the match call boundary,
that split P7a2 off P7a. Consumes P6a's extracted `subjects[]`.*

- **Resolve**: per subject build the key set, one alias query → **one id**
  (resolved, no LLM) / **many ids** (the candidate set is those subjects → hand
  to P6b2's match, dup-flag the pair) / **zero ids** → candidates: two FTS queries
  (name/alias vs registry names; claim text vs page bodies), **zero candidates →
  create, no LLM**.
- **Candidates**: the two FTS queries (same type, top ~5, deterministic) are the
  shortlist builder match consumes; the candidate FTS thresholds are config
  (eval-harness knobs), not constants.
- **The P6b→P6b2 seam** is the resolution outcome per subject: **resolved** (one
  id), **create** (zero candidates), or a **shortlist** (the candidate subjects)
  handed to match. P6b never calls an LLM and never assembles the manifest — that
  is P6b2's job.

**Touches:** `wiki/internal/{integrate,page,llm,config}/`.
**Verify:** resolve's three arms (one/many/zero ids) deterministic; candidates'
two FTS queries return the shortlist; the resolved/create/shortlist outcome is the
P6b2 seam. No LLM, no writes to pages.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
resolve (three arms), candidates (two FTS queries), and the resolution-outcome
seam are each present and individually tested before the phase closes.
**Eval hook:** the candidates retrieval lane registered as a harness-callable site
(obligation 1); the candidate FTS thresholds are config (obligation 2).

---

## [x] P6b2 — Document pass: match + the manifest (manifest out)

*Design §4.3, §4.1 (registry), §10. **No new resolution mechanics** — P6b's arms
already produce the shortlist. This phase lands the **match LLM call**, its real
config-default prompt + validation surface, and the **manifest** the document
pass hands downstream. Carved off P6b under the phase-budget re-check rule (see
P6b). Consumes P6b's resolution outcome; merge/commit close the loop in
P7a–P7b2.*

- **Match**: the one resolution LLM call — structured, tool-less, judges the
  shortlist at once, **binary** `same(id) | no_match`; identity not similarity;
  **doubt is no_match**; candidate-pair side channel feeds `dup_flags`. Excerpt =
  canonical name + full alias list + first `WIKI_MATCH_EXCERPT_CHARS` (default
  600) of page body.
- **Manifest**: populate the `Manifest` type **pinned in P4** (don't redefine it)
  — every extracted subject annotated with its resolved subject_id + target page
  + claims; the document pass fills the generalized `{text, cites[]}` claim shape
  with the one inbox row id. The **per-page base `version` slot is part of the
  pinned type** but is filled at merge-read time (P7a), not here — P6b2 leaves it
  unset, per P4's field obligations. P6b2 is the type's first real producer; its
  in-memory, never-persisted nature (the run id is its durable identity) is P4's
  contract. **P6b2 also records the manifest's `dup_pairs`** — match's
  candidate-pair side channel plus any many-ids pairs handed up from P6b, each in
  canonical order — the field P7a's commit reads to write `dup_flags` (per the
  *Manifest canonicity* enumeration).

**Touches:** `wiki/internal/{integrate,page,llm,config}/`.
**Verify:** match binary contract; manifest assembled correctly from P6b's
resolution outcome. No writes to pages yet.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
the match call, the match prompt-default gate, and manifest population are each
present and individually tested before the phase closes.
**Prompt gate (standing, offline — see *Prompt-default validation*):** the match
config-default prompt is non-placeholder and carries its five §4.3 sections; the
match parser schema-validates a committed, hand-authored, schema-faithful response fixture into
the binary `same(id) | no_match` verdict plus the `dup_pairs` side-channel.
Deterministic, key-independent.
**Eval hook:** match registered as a harness-callable site (obligation 1);
`WIKI_MATCH_EXCERPT_CHARS` is config (obligation 2); match returns its verdict
**and** the `dup_pairs` side-channel as distinct outputs (obligation 3).
**Integration test:** the standing tier's match slice — real match on a blunt
fixture (live pinned triple): assert match returns a clean binary verdict whose
`same(id)` resolves to a real subject. No page assertions yet — the commit lands
in P7a.

---

## [x] P7a — Document pass: merge core + the plain end-of-run commit (happy path)

*Design §4.4, §4.5, §6 (the `stale_notes` writer). Closes the document-pass
loop — the airtight transaction the whole spine was built for — on the
**single-pass happy path**; **P7a2** lands merge's real prompt + validation
surface, then P7b/P7b2 harden the commit for concurrency. Split from the old
monolithic P7, then **P7a2 carved off this phase under the phase-budget re-check
rule** (the merge prompt is the system's hardest and shipped bundled with the
airtight transaction). Merge here runs against a **placeholder config-default
prompt** under mocks — P2's "defaults may be placeholders; each call-site phase
fills its real default," and merge's real default is P7a2's job.*

- `internal/page` pages store: prose body + thin frontmatter (`subject`, `type`,
  `kind`, `title`); inline `[inbox-id]` citations; **lead discipline** (the
  cross-prompt obligation merge owes match). FTS5 kept current in the commit.
- **Merge**: one agent run per document (config-injected; the **real prompt lands
  in P7a2**). Input = the manifest **only**, never the original document. Write
  set = the manifest's pages exactly; read set looser (neighbors). Fold each
  subject's claims as prose (weave new, corroborate known with the new citation,
  corral contradictions with both sides + citations). Tools: read + write pages
  only.
- **The one end-of-run transaction**, made real: updated/created pages (= the
  manifest's write-set pages = the subjects' target pages, exactly) + registry
  inserts + `dup_flags` (inserted from the manifest's `dup_pairs` via `FlagDup`,
  canonical order) + the `stale_notes` merge appended (from the manifest's
  `stale_notes[]` carrier, with their `cites` — §6) + the run row + `integrated_by`
  (and `occurred_at` first-writer-wins from the manifest, events only) + the
  **`pages_fts` external-content sync** for each written page (explicit FTS5
  `'delete'` with the OLD title/body — held from merge's read — then re-insert at
  the page's `rowid`; **no triggers**, per design §12/§4.5; per-page, not a
  rebuild). Zero mid-run
  partial writes — the manifest carries the whole write set, so nothing reaches the
  DB outside this commit. This fills
  the generic end-of-run transaction wrapper P4 built and tested with stubs — now
  writing real pages/registry. **Merge records the base `version` it read for
  each page into the manifest's per-page version slot** (the value P7b's guard
  will consume — design §3's "the version merge read"); `version` is bumped per
  write, but the **conflict-handling `WHERE`-guard and the conflict loops are
  P7b/P7b2**: P7a is the single-writer happy path.
- **`stale_notes` writer hook**: when merge touches a read-only neighbor page it
  contradicts, it surfaces a stale note (subject, note, `cites`) **through the
  manifest's `stale_notes[]` carrier** (per the *Manifest canonicity* enumeration),
  which the end-of-run transaction writes **in its existing commit** (design §6) —
  the transaction owns the write, never reaching into merge's side effects. This is
  the producer side `lint-stale` (P9c) consumes — built here so P9c's work-list is
  not empty by construction.
- Replace the P4 document-pass **stub** with this real integrator — the **same
  `Integrator` interface**, emitting the **same `Manifest`**, so the spine is
  unchanged (the swap is mechanical, exactly as P4 set up and tested).

**Touches:** `wiki/internal/{integrate,page,run}/`.
**Verify:** full `ingest_text` → pages happy path (mocked LLM); the stub→real
swap leaves the spine green (P4's concurrency tests still pass); the manifest's
per-page base `version` slot is populated with the value merge read (so P7b's
guard has it); a `stale_notes` row is appended when merge contradicts a neighbor;
after the commit, `pages_fts` is consistent with `pages` for every written page —
a `MATCH` over a newly-created page returns it, and after a page **update** the
OLD body no longer matches while the NEW body does (proving the `'delete'`-then-insert
sync, not a stale append);
provenance chain (answer-less: page cites inbox id → `ReadPayload`). End-to-end
test through the spine. The merge prompt's offline gate, the integration
checkpoint, and the eval hook are **P7a2** (next), not here.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
the real pages/registry end-of-run transaction, the `pages_fts` external-content
sync, the stub→real `Integrator` swap, the populated per-page base `version` slot,
and the `stale_notes` writer hook are each present and individually tested before
the phase closes.

---

## [x] P7a2 — Document pass: the real merge prompt + its validation surface

*Design §4.4, §6.1, §10. **No new spine behavior** — P7a's merge already runs and
commits. This phase writes merge's **real config-default prompt** — the system's
hardest (six sections plus agent-tool discipline, lead discipline, and the §6.1
`superseded` obligation) — and stands up the checks that prove it. It is carved
off P7a under the **phase-budget re-check rule**: that prompt plus its whole
validation surface (offline gate + the second integration checkpoint + the eval
hook + the live integration slice) overran the one cold-start budget the airtight
transaction had already filled.*

- **The real merge prompt as config default** — replaces P7a's placeholder. Six
  §4.4 sections, the `superseded` obligation (§6.1), and the lead-discipline
  obligation merge owes the match call site. (Open items: prompts are deferred
  config defaults — merge's lands here.)
- The stub→real swap and the end-of-run transaction are unchanged from P7a; this
  phase swaps only the prompt default and adds the gates below — so the spine
  stays green.

**Touches:** `wiki/internal/{integrate,config}/`, a committed merge response fixture.
**Verify:** P7a's happy-path and concurrency tests still pass with the real
default prompt wired in (the swap is prompt-only — no spine change).
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
the real merge config-default prompt and its prompt-default gate are present and
individually tested before the phase closes (P7a2 ships no new spine behavior, so
this is its load-bearing deliverable set).
**Prompt gate (standing, offline — see *Prompt-default validation*):** the merge
config-default prompt is non-placeholder and carries its six §4.4 sections,
including the `superseded` obligation (§6.1); merge's output parser schema-validates
a committed, hand-authored, schema-faithful response fixture into a page-write + `superseded`
list. Deterministic, key-independent — holds even when the checkpoint below skips.
**Checkpoint (phase-owned, the second of three):** run the accumulated integration
tier (`go test -tags=integration ./wiki/...`, keys present) — this is the first
full document-pass slice, so extract + match + merge now run their live triples
end-to-end (page row + matching `[inbox-id]` citation on real output). A **red run
pauses the march** for investigation (advisory, not a deploy gate). Absent
keys/network → emit a visible `INTEGRATION CHECKPOINT SKIPPED — no keys` line,
**never a silent pass**. Rides the phase's `Verify` so `/finish` fires it.
**Eval hook:** merge registered as a harness-callable site (obligation 1); two of
merge's mechanical invariants — write-set conformance and claim-cite presence —
exposed as the deterministic pass/fail the harness scores (obligation 5; the
citation-preservation gate lands in P7b); the merged page's lead stays
match-recoverable (merge's closed-loop obligation to match).
**Integration test:** the first full document-pass slice — `ingest_text` a blunt
fixture through the **real** pipeline on the live pinned triple; assert a page row
exists with ≥1 `[inbox-id]` citation matching its source row, and the registry
subject + alias exist. Structure only — never whether the prose is *good*.

---

## [x] P7b — Document pass: optimistic commit + the lost-update loop + the §6.1 gate

*Design §3 (optimistic commit / the lost-update conflict), §6.1. Hardens P7a's
commit for the N-worker pool with the **page-update** race: the version-guarded
commit, the re-run-merge-only loop, the bounded retry it shares with P7b2, and
the citation-preservation gate. The **duplicate-mint** race is **P7b2**, built on
this phase's bound. Split from the old P7 so the concurrency machinery is its own
cold-start unit on top of a working merge+commit; the second conflict arm is
carved into P7b2 under the phase-budget re-check rule (two distinct conflict types
bundled with the §6.1 gate and a full concurrency suite overran one cold start).*

- **Optimistic commit**: the commit guarded by `WHERE subject=? AND version=?`
  — the version read from **the manifest's per-page base-`version` slot (pinned
  in P4, populated by merge in P7a)**, so P7b consumes an existing field and
  reshapes nothing; zero rows → conflict → roll back, re-read, **re-run merge
  only** for that page, recommit (the **lost-update** conflict type).
- **The conflict-loop bound** (reused by P7b2): cap **3 commit attempts** then
  fail naming **conflict-retry exhaustion**; `conflicts` counted on `runs`;
  post-exhaustion re-selection delayed via `ineligible_until` (P5). Built here
  with the lost-update loop as its first user; P7b2's duplicate-mint arm reuses it
  unchanged.
- **Citation-preservation gate** (§6.1): merge also emits a `superseded` list
  (the manifest's per-page `superseded` carrier); at commit, `old − new` citations
  must equal the declared list, else **failed call** (retried in-run, never
  committed). On a conflict-driven re-merge, the gate runs against the **re-run's
  fresh `superseded`** (re-derived from the new merge output), never the stale
  original.

**Touches:** `wiki/internal/{integrate,page,run}/`.
**Verify:** the conflict-loop bound retries and exhausts cleanly; the lost-update
conflict re-runs merge only; the citation gate rejects undeclared loss;
`conflicts` counted on `runs`; post-exhaustion delay applied. Concurrency tests on
top of P7a's happy path. (Duplicate-mint is P7b2.)
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
the version-guarded commit, the lost-update loop, the shared 3-attempt bound, and
the §6.1 gate are each present and individually tested before the phase closes.
**Eval hook:** the third mechanical invariant — citation preservation — now
exposed as the same deterministic pass/fail the harness scores merge on
(completing obligation 5 for merge alongside P7a2's two).
**Integration test:** re-ingest a fixture that forces a re-merge and assert the
§6.1 citation-preservation gate holds through a conflict-driven re-run (live
pinned triple). Structure only.

---

## [x] P7b2 — Document pass: the duplicate-mint conflict

*Design §3 (the duplicate-subject-minting conflict). **No new commit core** —
P7b's optimistic commit and the conflict-loop bound already run. This phase adds
the **second** conflict arm: two runs minting the same not-yet-registered subject.
Carved off P7b under the phase-budget re-check rule (see P7b). Reuses P7b's bound
and the existing stage functions; reshapes nothing.*

- **Duplicate-subject minting** caught by `UNIQUE(type, norm)` → roll back →
  **restart at resolve** for the colliding subject only (the lookup now hits the
  winner's freshly-committed aliases). **Extract is never re-entered** — nothing
  another run did invalidates what *this* document said.
- **Reuses P7b's conflict-loop bound** verbatim: the same 3-attempt cap,
  `conflicts`-on-`runs` counting, and post-exhaustion `ineligible_until` delay —
  the duplicate-mint arm is a second entry point into the existing loop, not a new
  one. A *found-it* alias attachment hitting UNIQUE on the same subject_id is a
  harmless `ON CONFLICT DO NOTHING`; a different subject_id is bridging evidence
  routed through this same arm.

**Touches:** `wiki/internal/{integrate,page,run}/`.
**Verify:** duplicate-mint restarts at resolve (not extract); the loser's restart
resolves onto the winner's subject; the shared bound caps and exhausts cleanly
across both conflict types; a same-subject_id alias collision is a no-op.
Concurrency tests on top of P7b.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
the UNIQUE-caught restart-at-resolve arm and its reuse of P7b's bound are present
and individually tested before the phase closes.
**Integration test:** force two concurrent runs to mint the same subject (live
pinned triple) and assert exactly one subject is created, the loser re-resolving
onto it. Structure only.

---

## [x] P8 — Digest pass: compile + the jobs config + digest concurrency

*Design §5, §3 (cron rows as durable batch authorization), §6 (jobs config). The
first reuse of the shared resolve→merge→commit pipeline.*

- **`jobs` config** (config, not a table; design §6): `name`, `trigger`,
  `select` (digest-only). Selection treats digest and lint entries identically.
  **Boot-time partition check**: selectors must partition event rows — overlap →
  refuse to boot; a consumed source matched by no selector → surface it.
- **Compile** (`internal/integrate`): plays extract's role for event piles —
  same structured, tool-less, golden-testable call shape, targeting extract's
  output schema directly (**no prose-digest artifact**). Resolves `occurred_at`
  from event payloads. **Per-claim `cites`** (events presented with inbox ids
  visible). Implements the **same P4 `Integrator` interface** and emits a
  `Manifest`, entering the shared pipeline at **resolve** — merge can't tell
  which integrator ran.
- **Cron-fired wiring**: a `cron.<name>` row → the worker looks up bound entries
  and runs each bound digest as its own `runs` row with `caused_by` = cron-row
  id (a tiny fan-out). Default daily; cadence is config; **no volume trigger**.
- **Digest concurrency**: at most one in-flight run per batch-entry name
  (job-name `TryLock`, per the locked P1 fork); **stamp by id list, never by
  selector** (re-evaluating at commit would silently drop mid-run arrivals).
  **Cron-row stamping** = the worker-local **completion-time join** ("do all
  bound entries for this `caused_by` now have a *succeeded* run?" → stamp once,
  WHERE-guarded as a no-op on races). A failed run leaves the cron row pending
  (the retry authorization).
- **Batch failure policy** (§7): policy acts on `runs.caused_by` (the cron row),
  dead-letter granularity = the causing row only, never event rows.

**Touches:** `wiki/internal/{integrate,worker,run,config}/`, the no-op cron stub
from P4 replaced.
**Verify:** events accumulate as invisible rows; cron row fires the bound
digest(s); compile goldens; completion-time join stamps the cron row only when
all bound runs succeeded; partition-overlap refusal at boot; batch failure
leaves the cron row pending.
**Deliverable gate (boundary — see *Phase completion is a checklist, not a green gate*):**
the `jobs` config + boot partition check, compile (its prompt gate), the cron
fan-out wiring, the completion-time join stamp, and the batch failure policy are
each present and individually tested before the phase closes.
**Prompt gate (standing, offline — see *Prompt-default validation*):** the compile
config-default prompt is non-placeholder and carries extract's six-section skeleton
with its four §5 deltas; the compile parser schema-validates a committed,
hand-authored, schema-faithful response fixture into `subjects[]` with per-claim
`cites` + `occurred_at`.
Deterministic, key-independent.
**Eval hook:** compile registered as a harness-callable site (obligation 1) and
emits per-claim `cites` + `occurred_at` as distinct, scorable outputs (the
citation-mis-attribution risk the harness measures — obligation 3).
**Integration test:** real compile on a blunt event pile (live pinned triple);
assert the emitted `subjects[]` carry per-claim `cites` that resolve to real inbox
ids and an `occurred_at`, and that the pile integrates through the shared pipeline
to a page.

---

## [x] P9a — Lint: `lint-dups` + the shared lint plumbing

*Design §6 (and §6.1 already gated in P7b). The first lint job, and the home of
the `dup_flags` helper + `jobs`-config + `lint_run` plumbing every later lint job
reuses. Each lint job is a worker-selected job with one `runs` row per attempt;
failure policy applies verbatim. Split from the old monolithic P9 — whose three
jobs "differ wildly in shape and cost" (design §6) — so each fits one cold-start
context.*

- **`dup_flags` storage helper**: `FlagDup(x,y)` (sorts to canonical order,
  `INSERT … ON CONFLICT DO NOTHING`); `UNIQUE` + `CHECK(subject_a < subject_b)`
  already in P1. (Reused by P9b's sweep.)
- **`lint-dups`** — dup-queue consumption. A flag is evidence, not a verdict:
  first a dedicated **judge** call (both full pages + complete alias lists),
  binary-ish → **merge** / **dismiss** (permanent) / **can't-tell-yet** (stamp
  `judged_version_a/_b`; re-judge only when a page version advances).
  Subject-merge: **older ULID wins** mechanically (judge picks only the canonical
  *name*); loser hard-deleted in one transaction (repoint aliases, rewrite open
  `dup_flags`, **fold** prose, delete loser page+subject, set winner
  `canonical_name`, mark row `merged`). One run per trigger, **one transaction
  per pair**. **Judge and fold are two separate tool-less calls** (fold inherits
  the §6.1 citation gate).
- **The shared lint plumbing** (lands here with the first lint job): register
  lint entries in the `jobs` config (no selector); the manual `lint_run(job)` MCP
  verb is just another front door that `Accept`s a trigger row. P9b and P9c
  register their entries against this.

**Touches:** `wiki/internal/lint/`, `wiki/internal/{page,config,mcp}/`.
**Verify:** `lint-dups` in isolation (mocked LLM); `FlagDup` canonical-orders and
de-dups; subject-merge hard-delete leaves no dangling loser id; can't-tell-yet
skips until a version advances; one transaction per pair; `lint_run` `Accept`s a
trigger row. Per-job tests.
**Prompt gate (standing, offline — see *Prompt-default validation*):** the dup-judge
and fold config-default prompts are non-placeholder (§6, both tool-less calls); the
judge parser schema-validates a committed, hand-authored, schema-faithful response fixture into
the ternary `merge | dismiss | can't-tell-yet` verdict, and the fold parser into a
body + `superseded` list (§6.1). Deterministic, key-independent.
**Eval hook:** the dup judge registered as a harness-callable site (obligation 1),
with the canonical-name pick scored as its own dimension off the dup-judge call's
canonical-name output field — not a separate registered call site (the surviving id
is mechanical, §6); the judge's ternary verdict
(`merge | dismiss | can't-tell-yet`) preserved verbatim, not collapsed
(obligation 3); `dup_flags` retained as a golden source (obligation 4).
**Integration test:** real dup judge on a blunt obviously-same and
obviously-different pair (live pinned triple); assert the verdict is one of
`merge | dismiss | can't-tell-yet`, and on a `merge` the fold yields a body that
passes the §6.1 citation gate. Blunt pairs only — subtle identity stays Part II.

---

## [x] P9b — Lint: `lint-sweep` (semantic duplicate sweep, zero-LLM)

*Design §6. The proactive walker that flags duplicates built from disjoint
streams (Bob-from-email vs Robert-from-CRM). Depends on P9a's `FlagDup` + jobs
plumbing. **Fully mechanical — zero LLM** — so it carries no integration-tier
slice (it has no live call site).*

- **`lint-sweep`** — semantic duplicate sweep, **flag-only**, **zero LLM**: for
  each subject run the same two candidate FTS queries (P6b); pairs above the flag
  threshold → `FlagDup`. Idempotent via the pair UNIQUE (settled pairs bounce
  off). Wide scan, rare cadence. Register the entry in the `jobs` config (no
  selector).

**Touches:** `wiki/internal/lint/`, `wiki/internal/config/`.
**Verify:** sweep flags pairs above threshold; idempotent on re-run; settled
(`merged`/`dismissed`) pairs are not re-flagged. Per-job test, no LLM.
**Eval hook:** the sweep's per-lane flag thresholds are config the harness sweeps
(obligation 2).

---

## [ ] P9c — Lint: `lint-stale` (staleness repair)

*Design §6, §6.1. Consumes the `stale_notes` rows P7a's merge writes. Depends on
P9a's jobs plumbing and P7a's writer hook.*

- **`lint-stale`** — staleness repair backed by `stale_notes` (the flag-only
  writers were built in P7a — merge/fold append a note with `cites` in their
  existing commit). Work list = open notes; one **tool-less call per subject**
  batching its open notes (page + notes + cited payloads in; rewritten page +
  per-note disposition out, one transaction). Inherits the §6.1 gate. Register
  the entry in the `jobs` config (no selector).

**Touches:** `wiki/internal/lint/`, `wiki/internal/{page,config}/`.
**Verify:** stale repair consumes open notes; one transaction per subject;
the rewritten page passes the §6.1 gate; each note's disposition is recorded.
Per-job test (mocked LLM).
**Prompt gate (standing, offline — see *Prompt-default validation*):** the
stale-repair config-default prompt is non-placeholder (§6, one tool-less call per
subject); its parser schema-validates a committed, hand-authored, schema-faithful
response fixture into a rewritten page + per-note disposition + `superseded` list (§6.1).
Deterministic, key-independent — holds even when the integration test below skips.
**Eval hook:** the stale-repair call takes its `(prompt, model, effort)` from
injected config (design §10 — it is a call site, though not one of the ten
scored registry sites); its rewritten-page output inherits merge's mechanical
invariants exposed as deterministic pass/fail (obligation 5).
**Integration test:** real stale repair on a blunt fixture note (live pinned
triple); assert the rewritten page passes the §6.1 citation-preservation gate.
Structure only.

---

## [ ] P10 — The read side: ask + search + timeline

*Design §9.1, §9.2 (tools 1–5; `related` deferred), §9.3 (search verb contract,
lexical-only for now). Needs pages (P7+).*

- **Search verb** — zero-LLM, the public side door: input `query` + `limit`
  (default 3, cap 10); **registry-first** resolution (exact alias pins rank 1,
  the retriever fills the rest); a hit is the **whole page**; rank order only, no
  scores; nothing prepended. (Lexical FTS5 only here; the hybrid retriever
  primitive lands in P11 behind the same interface.)
- **`timeline(from, to)`** — public verb, zero-LLM registry query over
  `type=event` subjects with `occurred_at` in the window (ISO-8601 prefix args,
  lexicographic range). Description frames it as "list event subjects in a date
  window," never "answer questions about a period."
- **Ask** — hosted-ask-first, **synchronous**, **strictly read-only** (no
  writes, no flight lock, no transaction; runs fully parallel with integration).
  Inner agent (config-injected) with a server-side budget (max turns / tokens /
  wall-clock) and the **six** inner tools — `search`, `lookup`, `read_page`,
  `read_source` (→ `ReadPayload`, size-capped), `timeline`; **`related` is
  goldens-gated and NOT built**. **Answer contract**: page-level citations
  (subject id + title); inbox ids only when `read_source` was used; contradictions
  surfaced, never resolved.
- **`asks` table** lifecycle (insert `running`, finalize at end, boot sweep
  marks orphans `crashed`); stores `question`.
- **Steering tool descriptions**: `search` = exact/known-item fetch; `ask` = the
  default for any question ("answers come back cited; do not assemble answers
  from search results yourself"). Description text is part of the design surface.

**Touches:** `wiki/internal/{read,index,mcp}/`, `wiki/internal/db/` (asks).
**Verify:** search registry-first + whole-page contract; timeline window query;
ask happy path (mocked inner agent) returns page-cited answer; "wiki has nothing
on this" over fabrication; asks lifecycle + orphan sweep.
**Prompt gate (standing, offline — see *Prompt-default validation*):** the ask
config-default prompt is non-placeholder and carries its six §9.2 sections; the ask
answer parser schema-validates a committed, hand-authored, schema-faithful response fixture into
the page-level citation contract (§9.2). Deterministic, key-independent. (search /
timeline are zero-LLM — no prompt gate.)
**Eval hook:** ask and the search retrieval lane registered as harness-callable
sites (obligation 1); the ask turn/token/wall-clock budget is config
(obligation 2); `asks.question` + answer + citations stored as the free real-data
golden source the research doc names (obligation 4).
**Integration test:** real `ask` over the fixture wiki (live pinned triple) on one
answerable question and one known-gap question; assert answer-xor-abstention,
every cited subject id resolves, and the gap question abstains. Real `search`
returns whole-page hits, registry-first.

---

## [ ] P11 — The embedding lane: hybrid retriever

*Design §9.3. Designed now, sequenced last — FTS5-first was build ordering only.
Slots behind the retriever interface P10 already consumes.*

- **One hybrid retriever primitive, two lanes (BM25 + vector), RRF-fused**
  (k=60, top ~50/lane in), serving three call sites: the search verb / ask's
  search tool, resolution's candidates step (P6b), and `lint-sweep` (P9b). Vector
  lane **independently switchable per call site**, on only when measurement shows
  lift. **Sweep does not fuse** — per-lane thresholds (eval-harness knobs).
- **Embedding unit = one vector per page**: canonical name + alias list + body
  (truncated). `page_vectors(subject, embedded_version, model, vector)`.
- **Write path = async catch-up**, never in the integration commit: an
  in-process embedder goroutine wakes on run completions (the nudge pattern),
  sweeps a work-list query (vector missing / behind `version` / wrong `model`),
  batches API calls. Stale vectors **serve until replaced** (reads never compare
  `embedded_version` to `pages.version`); **model match IS a read-side validity
  condition** (only `model = current` rows; cross-model cosine is garbage).
- **Vector storage = plain table, brute-force cosine scan in Go** (pure-Go
  chassis rules out sqlite-vec; ~5k × 1024-dim is <10ms — exact, testable, zero
  tuning). **Read-side degradation**: a failed read-time embed → lexical-only.
- **Provider**: the **P0b embeddings client** — OpenAI `text-embedding-3-large`
  @ 1024 dims (`WIKI_EMBED_MODEL` + `WIKI_EMBED_DIMS`); `OPENAI_API_KEY`
  presence-gated at the composition root (absent key → lexical-only, the
  `ANTHROPIC_API_KEY` pattern). A model/dims change is a config change the
  catch-up worker absorbs (first deploy and model change are the same code path).

**Touches:** `wiki/internal/index/`, `wiki/internal/config/`, the catch-up
goroutine, `wiki/internal/db/` (page_vectors).
**Verify:** brute-force cosine correctness; catch-up work-list (missing / stale /
wrong-model); RRF fusion ranking; per-call-site lane switch; absent-key and
read-failure both fall back to lexical; model-mismatch rows excluded from reads.
**Checkpoint (phase-owned, the third and final):** run the accumulated integration
tier (`go test -tags=integration ./wiki/...`, keys present) over the **full Part I
pipeline** — every live call site (extract, match, merge, compile, the lint judges,
ask, the embed round-trip) now exercised end-to-end on its pinned triple. A **red
run pauses the march** for investigation (advisory, not a deploy gate). Absent
keys/network → emit a visible `INTEGRATION CHECKPOINT SKIPPED — no keys` line,
**never a silent pass**. Rides the phase's `Verify` so `/finish` fires it; this
closes the standing tier.
**Part-I exit obligation (see *Prompt-default validation*):** this checkpoint and
the P6a/P7a2 checkpoints must each have **at least one recorded non-skipped green
run** before Part II (P12) begins; an all-`SKIPPED` history leaves Part I
**not-done**, not done-green. That keyed run is **not** performed inside this phase
(it may be keyless here, hence the `SKIPPED` line above): it is discharged by the
dedicated **P11k** gate below, which a keyed box runs at the P11→P12 boundary. (The
offline prompt gates above hold regardless; this obligation is the live-model half
the checkpoints, when skipped, defer to P11k.)
**Eval hook:** RRF `k`, `WIKI_EMBED_MODEL`/`WIKI_EMBED_DIMS`, and the per-lane
thresholds are config the harness sweeps (obligation 2); the per-call-site lane
switch is exactly the "lexical-only vs hybrid, per site, scored" deliverable the
research doc (§8–10) makes first-class — the retriever exposes both lanes so the
harness can measure recall lift vs cost. **Completes the call-site registry:**
all ten sites are now harness-callable (obligation 1 fully satisfied).
**Integration test:** real embed round-trip on the live `WIKI_EMBED_MODEL` /
`WIKI_EMBED_DIMS`; assert a `page_vectors` row with `len(vector) ==
WIKI_EMBED_DIMS` and `model == current`, and that the hybrid retriever returns
ranked whole-page hits. **Closes the standing tier** — the full Part I pipeline
now has an end-to-end real-model liveness check.

---

## [ ] P11k — Keyed Part-I validation gate (keys required)

*The single keyed, live-model step the otherwise-phase-only `/finish` march needs an
explicit owner for (see *Prompt-default validation*, mechanism 2). It ships no new
product code; it discharges the **Part-I exit obligation** and refreshes the
recorded fixtures the offline prompt gates stub. Run on a keyed box. This is the one
phase that **must not** be marked done while skipped — every other real-model check
in the plan is advisory and skip-on-no-keys; this one is the hard Part-I exit, a
human handoff rather than an autonomous step.*

- **Run the three accumulated integration checkpoints to green.** `go test
  -tags=integration ./wiki/...` with `ANTHROPIC_API_KEY` + `OPENAI_API_KEY` and the
  network present, exercising every live call site (extract, match, merge, compile,
  the lint judges, ask, the embed round-trip) end-to-end on its pinned `(prompt,
  model, effort)` triple. This produces the **recorded non-skipped green run** the
  P6a / P7a2 / P11 checkpoints each owe the exit obligation.
- **Capture and commit the recorded real-model fixtures.** For every call-site
  prompt gate (P6a extract, P6b2 match, P7a2 merge, P8 compile, P9a judge + fold, P9c
  stale repair, P10 ask), capture one real-model response from the live triple and
  commit it, **replacing the hand-authored stub fixture** the offline gate shipped
  with. From here on the offline (a-ii) parser/schema test runs against *real*
  output, so parser/schema drift is caught offline too — closing the half a
  hand-authored stub cannot.
- **keys-REQUIRED, HALT-not-SKIP.** Unlike the per-phase checkpoints (advisory,
  skip-on-no-keys), this gate is the Part-I exit. If `ANTHROPIC_API_KEY` /
  `OPENAI_API_KEY` or the network are **absent it does not pass** — it **halts the
  march and surfaces to the human** with a visible `PART-I EXIT GATE BLOCKED — no
  keys` line, because an all-`SKIPPED` history is exactly the "every real-model
  check went dark" state the exit obligation forbids. A **red** run likewise blocks
  P12 (Part II would otherwise score call sites proven only by mocks). This is the
  one place the plan trades the advisory stance for a hard stop, and it is a human
  handoff, **never** a CI/deploy gate.

**Touches:** committed integration-test fixtures under `wiki/` (the recorded
real-model responses); no product code.
**Verify:** all three checkpoints (P6a / P7a2 / P11 slices) recorded green on a keyed
box; every call-site prompt gate's fixture replaced with a recorded real-model
response and its offline (a-ii) test re-green against it; the gate halts visibly
when keys/network are absent rather than passing. **This phase, uniquely, is not
"done" while skipped.**

---

## [ ] P11d — First production deploy to `int`

*The wiki rewrite goes live. A keyed, box-side human step like P11k — **not** a
`/finish`-autonomous phase (it ssh-es to the box and runs `opsctl`). Placed at the
Part I→Part II boundary so the live service starts accruing the real-data goldens
Part II anchors against (enablement obligation 4: ingested documents for extract,
the `asks` table for ask). Deploys the **provisional** config defaults — P16's
report later swaps in the tuned ones (re-deploy: P16d).*

Follows the suite deploy model (`CLAUDE.md`); this replaces an already-running
legacy wiki, so it is a clean cutover on a disposable box DB, not a fresh install:

- **Seed box secrets.** `aws sso login --profile int`, then **`wiki/bin/secrets`**
  → non-destructive read-modify-write of the wiki's slice of
  `/ikigenba/int/app-config`: `ANTHROPIC_API_KEY` + the new `OPENAI_API_KEY`
  (P2's script change) + `WIKI_OWNER`. Other apps' keys in the shared blob are
  left untouched.
- **Verify service plumbing.** `opsctl init-box` is already done for the box and
  `opsctl setup wiki` likewise (the legacy unit/dirs exist) — confirm, don't
  re-create. A genuinely fresh box would need both first.
- **Ship + stage + deploy** (from the `main` worktree): `bin/bump wiki major` (a
  clean rewrite is a major bump) → `bin/ship wiki` → `ssh int sudo opsctl stage
  wiki v<ver> --artifact /tmp/wiki-v<ver>` → `ssh int sudo opsctl deploy wiki
  v<ver>`. `deploy` regenerates `etc/manifest.env`, backs up the DB (the schema
  advances), `migrate`s (the drop-legacy migration then the consolidated DDL),
  atomic-swaps `current`, restarts the unit, prunes.
- **Restart the dashboard** so it re-reads the new wiki manifest — the MCP tool
  surface changed (`ingest_text` / `ingest_url` / a status verb / `search` / `ask`
  / `timeline`).

**Touches:** no product code — operator actions plus the already-committed P2
`wiki/bin/secrets` / `wiki/.envrc` change.
**Verify:** `opsctl status` shows wiki on the new version; `opsctl tail wiki` is
clean; MCP `health` / `reflection` are live through the dashboard; an `ingest_text`
round-trips to a page; **the embedding lane is actually active** (a `page_vectors`
row appears via catch-up, not a silent lexical-only fallback) — the live proof that
both `OPENAI_API_KEY` and `ANTHROPIC_API_KEY` reached the running process. Rollback
is `ssh int sudo opsctl rollback wiki`.

---

# Part II — the evaluation harness

The offline tool of `docs/wiki-redesign-research.md`: a runner that sweeps
`{generation} → {prompt, data} × {model} × {effort}` over the **real** call
sites (Part I's enablement makes this possible) and produces a per-generation
comparison table of score + cost + latency, with the dangerous-direction error
surfaced separately. It is a measurement tool — never CI, never a deploy gate.
These phases start only after **P11k** (every site exists and is harness-callable,
**and** the Part-I exit obligation is met) — the three integration checkpoints
(P6a / P7a2 / P11) each have at least one recorded non-skipped green run, captured by
the P11k keyed gate (see *Prompt-default validation*), so no call site reaches Part
II having been validated only by mocks. With **P11d** the service is now live on
`int`, so Part II's generators can anchor their goldens against real ingested
documents and real `asks` rows as they accrue (enablement obligation 4), not
synthetic data alone.

## [ ] P12 — Eval design lock

*Research doc "Open questions for the design doc." A plan executes a settled
design; the research fixes the *what* and *shape* but leaves six questions open.
This phase resolves them and records the result in a new
`docs/wiki-evaluation-design.md` (keeping the design→plan convention intact),
the way P1 locks the digest fork before code depends on it. Mostly authorship +
decisions, minimal code.*

Resolve and record:
- **Golden authorship vs real-data harvest** — the mix per site, and whether
  real data seeds the generator or only validates it.
- **Judge-model independence** — fix the judge model for the rubric/answer
  judges (merge, ask); it **must not** be a model under test (self-preference);
  set panel size for subjective criteria.
- **Test-set storage & identity** — how a `(dataset + prompt)` bundle is named,
  versioned, and stored on disk, and how a run pins which bundle it used so
  results stay attributable after a set is superseded.
- **Where the harness lives & cost control** — `bin/` tool vs `opsctl` vs a Go
  test target; and **output caching** keyed on `(test-set, case, prompt, model,
  effort)` so re-scoring is free (the sweep is a cartesian product of paid calls
  — caching is load-bearing, not an optimization).
- **Saturation detection** — the rule for declaring a generation saturated (top
  configs all above a score with no separation) so "mint the next generation" is
  triggered, not vibes.
- **Decision presentation** — how a run's matrix is shown so a human can pick:
  dangerous-direction error + cost + latency beside the headline score, never a
  single lumped rank.

**Touches:** `docs/wiki-evaluation-design.md` (new).
**Verify:** all six questions answered with rationale; the dataset format
`(case_id, site, generation, failure_tag, input, gold)` and the four scorer
kinds are pinned; reviewed in full before P13.

## [ ] P13 — The rig

*Research doc §"High-level plan" piece 1. Build once, prove with one site.*

- Dataset format + loader: `(case_id, site, generation, failure_tag, input,
  gold)`.
- The **runner**: injects a `(prompt, model, effort)` triple into the **real**
  call-site function (via Part I's call-site registry), captures the raw output
  plus **cost and latency**. "Production" is just one pinned triple the runner
  can also evaluate.
- The **results table** `config × metric`, always reported **per generation**;
  the outer sweep dimension is the generation (its prompt + data).
- **Output caching** keyed on `(test-set, case, prompt, model, effort)` per P12,
  so a re-score with a changed scorer costs zero provider calls.
- Wire **one site end-to-end** to prove the rig — Match (its identity corpus is
  the headline case and is shared by three sites).

**Touches:** the harness tool (location per P12), reading Part I's call sites.
**Verify:** the rig runs Match over a tiny fixture, sweeps `model × effort`
(incl. an OpenAI model via P0a), writes the table, and reproduces a second run
entirely from cache (zero paid calls).

## [ ] P14 — The scorer library

*Research doc piece 2. Four kinds cover all ten sites; every scorer reports the
**dangerous direction separately** (the research's asymmetry principle).*

- **Set-alignment** (extract, compile): align predicted↔gold subjects; subject
  **precision and recall separately** (over-extraction its own counter); type
  accuracy; claim recall (fuzzy/LLM-judged); self-containedness check; compile
  adds compression ratio + per-claim citation precision/recall + `occurred_at`.
- **Asymmetric confusion** (match, dup judge): binary/ternary confusion with
  **false-merge** (and false-split / false-dismiss) as named separate axes; the
  `dup_pairs` side-channel as its own recall number; a `can't-tell-yet`-when-
  evidence-present laziness metric.
- **Recall@k + RRF** (candidates / search / sweep): recall@k for candidates
  (recall is king — a miss mints a permanent dup), a ranking metric for search,
  pair-discovery recall for sweep.
- **Mechanical-checks + rubric-judge-panel** (merge, ask): merge — the §6.1
  citation-preservation gate, write-set conformance, claim-cite presence (the
  same deterministic checks Part I exposes, reused here for free) + a rubric
  panel (lead-identity, woven-not-ledgered, contradictions-corralled, no loss,
  no hallucination); ask — answer correctness, citation faithfulness, abstention
  on the gap set (fabrication rate is the headline), contradiction-surfacing,
  with **retrieval failure decomposed from synthesis failure**. Judge model +
  panel size per P12.

**Touches:** the harness scorer library.
**Verify:** each scorer against hand-built known-good/known-bad inputs;
dangerous-direction rates reported separately, never lumped into accuracy.

## [ ] P15 — Per-site test sets / generators

*Research doc piece 3. One generator per site producing versioned
`(dataset + prompt)` bundles (generations); shared corpora are the structural
saving.*

- **Shared corpora**: one synthetic **identity corpus** → Match + dup judge +
  candidates + sweep (one truth, four consumers); one synthetic **wiki** (pages
  with known facts, gaps, contradictions) → ask + search. Plus standalone sets
  for extract, compile, merge (manifests built directly), canonical-name pick.
- Each generator spans **blunt → subtle** case shapes (per the research's
  per-site data designs); later generations weight toward subtle. Generations
  are the only escalation mechanism — no per-case difficulty labels.
- **LLM-author the goldens, then an adversarial verification pass** over them
  (the redesign's own verify pattern); anchor against real data where it exists
  (ingested documents for extract, the `asks` table for ask — the free goldens
  Part I captured).
- Bundle naming/versioning/storage per P12; the harness selects a bundle by
  name so a later harder generation stands up beside the first. **This also
  produces the cross-subject / temporal-span ask goldens** that gate `related`
  (design §9.2) — un-gating it becomes a measured decision.

**Touches:** the harness generators + committed test-set bundles.
**Verify:** a gen-1 bundle exists for every site; the adversarial pass ran; the
harness loads a bundle by name; the shared corpora feed their multiple consumers.

## [ ] P16 — The sweep + report

*Research doc piece 4. The product: the table that turns scores into a decision —
and licenses the config defaults Part I deferred.*

- Run the full matrix `generation → {prompt, data} × model × effort` across all
  sites, **including OpenAI models** (enabled by P0a) — model-per-role and
  effort-per-role are first-class sweep axes alongside the deferred knobs
  (`WIKI_MATCH_EXCERPT_CHARS`, candidate/sweep thresholds, RRF `k`, embed
  model/dims, ask budget).
- Produce the **comparison report** per P12's presentation rule: one row per
  config, cells = score **+ cost + latency**, dangerous-direction error
  surfaced separately, always against a named generation; apply the saturation
  rule.
- The retrieval **side-by-side** (lexical-only vs hybrid per call site, recall
  lift vs cost) — the deliverable that licenses or declines the vector lane at
  each site (research §8–10).
- **The feedback loop**: the report's chosen configs become Part I's config
  defaults (the pinned `(prompt, model, effort)` per site, the deferred knobs).
  Document that this is how defaults get set — a human reads the matrix and picks.
  The chosen defaults are then shipped to `int` by the re-deploy phase (**P16d**).

**Touches:** the harness sweep driver + report renderer.
**Verify:** an end-to-end sweep on gen-1 produces the report; cost/latency and
dangerous-direction errors present per config; a worked "pick a config" example;
re-running re-scores from cache without new paid calls.

## [ ] P16d — Re-deploy `int` with the tuned config

*The closing step of the feedback loop: ship the config defaults P16's report
chose. A keyed, box-side human step, identical in mechanics to P11d — only the
committed config defaults (and any refreshed prompts) differ. No new product code
unless the report also licenses `related` (design §9.2) or the vector lane at a
site, which land as their own ordinary changes first.*

- Update the call-site config defaults to the report's picks — the pinned
  `(prompt, model, effort)` per site and the deferred knobs
  (`WIKI_MATCH_EXCERPT_CHARS`, candidate/sweep thresholds, RRF `k`, embed
  model/dims, ask budget); commit to `main`.
- `bin/bump wiki minor` → `bin/ship wiki` → `opsctl stage` → `opsctl deploy`,
  exactly as P11d. Because the integration tier is **pinned to whatever the live
  config currently pins**, it re-validates that the new picks at least *run*
  before they serve traffic — a config change can't quietly ship a prompt the
  model chokes on (*Integration testing*).
- Restart the dashboard only if the MCP tool surface changed (it usually has not).

**Touches:** `wiki/internal/config/` defaults (+ any prompt files); operator
deploy actions.
**Verify:** `opsctl status` shows the new version; the pinned defaults match the
report's chosen configs; a post-deploy `ask` / `ingest_text` smoke passes on the
live triples.

# Open items the plan inherits (filled during execution, not blockers)

- **Exact prompts** for the LLM call sites and whether a shared invariants block
  (lead discipline, citation rules, salience polarity) is factored out: each
  call-site phase writes its prompt as the **config default** for that site; the
  Part II report then arbitrates candidates against the pinned production prompt.
  Only rough section-shapes exist today (design §11).
- **Exact models per call site + config defaults** (lint cadences, ask budget
  knobs, embed model/dims): pinned as provisional defaults in the owning Part I
  phase, then **set for real by P16's report** and shipped to `int` by **P16d** —
  that feedback loop is the reason Part II is in this plan rather than a separate
  track.
- **The six eval design questions** (judge-model independence, test-set storage,
  harness location + caching, saturation detection, golden authorship-vs-harvest,
  decision presentation) are resolved in **P12**, not left open — they are
  prerequisites internal to Part II, recorded in `wiki-evaluation-design.md`.
