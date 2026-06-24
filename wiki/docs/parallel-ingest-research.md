# Parallelizing Ingest — Research (Highly Speculative)

> ⚠️ **STATUS: HIGHLY SPECULATIVE — RESEARCH ONLY. DO NOT BUILD FROM THIS.**
> This is an exploratory path captured for future investigation, not a committed
> design. Nothing downstream consumes it (the autonomous build reads only
> `product.md`, `design.md`, `plan.md`). It has **not** been validated against
> rate limits, real workloads, or the SQLite write model under load, and it is
> **not** reflected in any `DNN` Decision. Treat every claim here as a hypothesis
> to be tested, not a settled fact. Rewrite in place as understanding evolves —
> never append. If/when a path here earns confidence, it graduates into a proper
> `design/DNN.md` Decision + plan phases; until then it stays here, inert.

---

## 0. The question

> *Is there any chance of parallelizing ingest?*

Today ingest is strictly serial: `WorkerConcurrency = 1` (`internal/wiki/wiki.go:27`),
one `worker.Run` loop calling `ProcessNext` → `ClaimPending` → `processClaimed` in
a tight serial loop. Each job runs two LLM-heavy stages back to back — **extract**
(pull subjects + claims from the document) and **compile** (regenerate each
affected subject's page) — and commits them in a single SQLite write transaction
(`internal/wiki/service.go:282–345`).

The naive answer ("bump the worker count") is wrong, and the reason *why* is the
whole point of this doc.

## 1. The real problem is conceptual, not mechanical

The mechanical blockers (SQLite single-writer, the subject-resolution
read-modify-write race) are real but secondary. The load-bearing problem is:

**A page is a derived aggregate, and an ingest job does not own it.**

`plannedClaims` (`internal/wiki/service.go:409`) pulls the **full** claim set for
each affected subject, and the compiler produces the page from it. So the page is
already a **pure projection over the claims table, keyed by subject**:

```
page(X) == compile(X, claims(X))     -- the invariant
```

Today, **serialization is the only thing keeping that invariant true.** Picture two
jobs A and B that both mention subject X:

1. A reads `claims(X)` = {existing}, compiles `page(X)` from {existing ∪ A's}.
2. B reads `claims(X)` = {existing} (A not yet committed), compiles from {existing ∪ B's}.
3. Both write the whole of `page(X)`. **Last writer wins — the other's claims vanish
   from the page** even though they're still in the claims table.

That is a lost-update on a derived aggregate. It is not a lock you can add; it's
that **two jobs each believe they own "the page for X," and the page belongs to X,
not to either job.**

## 2. The reframe — the unit of serialization is the *subject*, not the *job*

The job is the wrong aggregate boundary. The right ones:

| Concept | Role | Concurrency property |
|---|---|---|
| **Claim** | an event — an independent fact attributed to a subject | append-friendly, naturally concurrent |
| **Subject** | the aggregate root claims attach to | the real unit of mutual exclusion |
| **Page** | a projection / materialized view of `claims(subject)` | must be *recomputed*, never *written by a passer-by* |

The decisive consequence:

> **Two jobs can run fully in parallel iff their affected-subject sets are disjoint.**
> They conflict only where they overlap on a subject — and there the conflict is on
> *compiling X*, which must (a) serialize per-X and (b) happen *after* both
> claim-sets are committed.

## 3. Candidate decomposition (hypothesis)

Split the fused job into three stages with different concurrency rules:

1. **Extract — parallel, no DB.** document → candidate `{subjects, claims}`. This is
   the expensive LLM stage and it is embarrassingly parallel. Biggest win lives here.

2. **Attach — one short serial point (or a DB uniqueness constraint on `norm_name`).**
   Resolve/dedup subjects, append claims, and **mark each affected subject dirty**.
   Cheap, no LLM. This is also where dedup correctness lives — the same correctness
   the serial worker provides today, concentrated into a small critical section
   instead of wrapping the whole (slow) job.

3. **Compile dirty subjects — parallel across *distinct* subjects, serial *per*
   subject, coalescing.** `page(X) = compile(X, claims(X))`, always recomputed from
   the committed claim set, therefore idempotent. Whoever wins the per-X compile
   produces the correct union, so nothing is clobbered. This is the *other*
   expensive LLM stage and it fans out across many subjects — real parallelism,
   bounded only by "at most one compile of X in flight."

Only stage 2 is inherently serial, and it is the cheap one.

## 4. The bonus insight — this fixes a problem that exists *right now, serially*

Because compile is fused into each ingest job, a batch of N documents that all
touch subject X recompiles `page(X)` **N times** — each an LLM compile over a
growing claim set, N−1 of them immediately superseded. (Observed live: ingesting
~12 Claude Shannon documents recompiled the `claude-shannon` page on every single
job.)

A dirty-set reconcile with coalescing recompiles each touched page **once** after
the batch settles. So decoupling is not only the key to parallelism — it
**eliminates redundant recompilation that wastes LLM calls even in the current
single-worker world.** `mark-dirty + debounce + recompute-from-source` is the one
mechanism that solves both the concurrency-clobber *and* the redundant-work
problem.

## 5. The consistency mechanism (sketch, unproven)

A "dirty" subject needs a generation guard so a claim landing mid-compile is not
lost:

- Each subject carries a monotonically increasing `claims_generation` (bumped on
  every claim attach/remove for that subject).
- A compile records the generation it started from: `compile X @ gen=N`.
- On commit, the page write succeeds only if the subject is still at `gen=N`. If a
  claim landed meanwhile (`gen=N+1`), the subject stays dirty and is re-queued for
  recompile.

This yields eventual consistency of the projection without any global lock. It is a
standard materialized-view reconcile pattern (event-sourcing: claim=event,
subject=aggregate, page=projection), but it has **not** been implemented or tested
here.

## 6. Open questions (the actually-hard decisions)

These are why this is research, not a design:

- **Where does the subject-resolution serialization point sit?** A single mutex
  around stage 2? A DB `UNIQUE(norm_name)` constraint + insert-or-get so dedup is
  enforced by the engine and stage 2 can itself be concurrent? The latter is more
  parallel but changes the failure model (constraint-violation retries).
- **What *is* "dirty"?** A column on `subjects`? A separate work queue table? A
  derived diff computed at compile time? Each has different crash-recovery and
  coalescing behavior.
- **Merge as a barrier.** A merge job rewrites claims/pages across many subjects
  under `mergeMu` (`internal/wiki/service.go:443`). It must be a serialization
  barrier against any ingest touching overlapping subjects. How does it interact
  with the dirty-set — does a merge just dirty the survivor and let the normal
  reconcile rebuild it?
- **Rate limits, not cores, are the real ceiling.** N parallel extractions +
  N parallel compiles multiply Anthropic TPM/RPM. The achievable speedup is capped
  by account throughput; the parallelism design must include a concurrency limiter
  keyed to the API budget, not to CPU count.
- **Ordering semantics change.** Today job N sees job N−1's subjects (FIFO). Parallel
  extract breaks "later sees earlier." That is believed fine because dedup
  correctness moves into stage 2, but it needs to be proven against the merge/alias
  resolution paths.
- **Crash recovery.** `RequeueWorking` (`internal/wiki/service.go:190`) currently
  re-runs a whole job on restart. With a three-stage pipeline, partial progress
  (claims attached, page not yet compiled) must recover correctly — the dirty flag
  is what makes a half-finished job safe to resume, but this is unverified.

## 7. Where this stands

Plausible and conceptually clean: **claims-as-events, page-as-coalesced-per-subject-
projection.** It promises both parallelism *and* a reduction in redundant compiles.
But it is a significant re-architecture of the ingest core, it has real unproven
risk in the consistency/crash/merge interactions, and the payoff depends on a
workload profile (big batch loads vs single-ingest latency) that has not been
measured. **Do not start building.** The next concrete step, if pursued, is to
measure the actual pain (throughput on batch loads? redundant-compile cost?) before
committing any of section 3 to a Decision.
