# Refine log — wiki redesign

One entry per refine pass. Each names the single biggest remaining risk to
successful completion at the time, the fix applied, and what to watch next.

---

## Iteration 1 (2026-06-13) — P1 lacked the boundary completion gate

**Built state.** Zero redesign code exists — the `wiki/` tree is still entirely
legacy. The whole deliverable is the `/finish` march through ~30 phases
(P0→P16d). All work so far is documentation, refined into an exhaustively
self-consistent design + plan. Spot-checks confirm the plan is *well-grounded*,
not speculative: every external code anchor checked is real — the ikigai-cli
OpenAI port source (`/mnt/projects/ikigai-cli/app-root/internal/provider/openai/openai.go`),
`agentkit/model/registry.go:168` (`ProviderOpenAI` + `gpt-5.5`), the
`clientFactory` at `prompts/internal/runner/runner.go`, the legacy migrations P1
tears down (`002_wiki.sql` → `wiki_ingest`/`wiki_jobs`, `003_feed_offset.sql` →
`feed_offset`), `eventplane/{outbox,consumer}/schema.go` `SchemaSQL`, and the
P0c backend hooks — and every referenced research doc exists. The plan is
*over*-specified, not under-specified.

**Risk (the single biggest).** The plan's most important boundary backstop — the
standing rule *"Phase completion is a checklist, not a green gate"* — enumerated
the phases it covers as **"P6b, P6b2, P7a, P7a2, P7b, P7b2, P8"** and **omitted
P1**. But P1 is the one phase the plan elsewhere calls *"the foundation of all of
Part I, its artifacts are immutable."* P1 bundles three heavy workstreams (lock
the digest fork + tear down the legacy wiki + transcribe the entire §12 schema:
~9 tables, FTS5, two library-byte-identical migrations and their tests). A
cold-start subagent that runs out mid-P1 can commit a *partial* migration set and
a *partial* schema test referencing only the tables it reached — and
`go test ./wiki/internal/db/...` goes green, because the un-transcribed tables
have no test. That broken immutable foundation then underpins every downstream
phase, recoverable only by forward corrective migrations mid-march — the
**costliest recovery in the plan, at its least-recoverable phase.** This is
precisely the failure class the checklist rule exists to convert into a loud
boundary failure, and it was the glaring hole in that rule's own coverage. Every
other candidate risk I examined was already mitigated (seams pinned via
Manifest/schema canonicity; anchors verified; referenced docs present; phase
budgets re-checked for P6/P7/P9) — P1 was the exception the budget re-check and
the completion checklist both skipped.

**Fix (surgical, using the plan's own mechanism).** Two edits to
`wiki-redesign-plan.md`, nothing else:
1. Added **P1** to the *"Phase completion is a checklist"* rule's coverage list
   (now `P1, P6b, …, P8`), with a paragraph explaining why P1 belongs there for
   the *strongest* reason — its artifacts are immutable, so a partial commit is
   the costliest recovery in the plan; and noting that P1's existing §12-cross-check
   states the *intent* but only a boundary checklist forces the coordinator to
   confirm completeness independent of the green suite.
2. Added a **`Deliverable gate`** line to P1's `Verify` block enumerating its
   seven load-bearing deliverables (fork decision; legacy tree removed;
   drop-legacy migration; `outbox` migration + its byte-identity test;
   `feed_offset` preserved; all nine §12 tables + `pages_fts`; the schema test
   against the external §12 spec) — a missing item is now a loud failure at the
   boundary that introduced it.

**What to watch next.**
- **P1 is now gated but still dense.** If, when P1 actually runs, the coordinator
  finds the cold-start budget tight (legacy teardown + full §12 transcription +
  two library migrations + tests + the fork decision in one context), the next
  escalation is the *design-time* defense: split P1 via sub-letters (e.g. P1a =
  fork lock + legacy teardown + the two library-owned migrations and their
  byte-identity tests; P1b = the §12 application DDL + the schema test against
  §12), exactly as P6/P7/P9 were split. The completion gate added here is the
  cheaper boundary backstop; a split is the heavier design-time fix if the gate
  starts firing.
- **The plan may be at the point of diminishing refinement returns.** It has been
  hardened across ~15 doc iterations and is now over-specified relative to a
  zero-code starting point; most remaining "risks" are process polish, not
  execution blockers. The highest-value next move is likely **to start the march
  (P0)** and let real execution surface the real risks, rather than continue
  refining the document. If a future pass finds only speculative or minor gaps,
  recommend stopping and building.
- **No `docs/wiki-redesign-product.md` exists** (the convention's optional
  product doc). Success criteria live implicitly in the design constraints and
  the per-phase `Verify` gates. Not a blocker, but if scope disputes arise during
  the march, a product doc pinning the success bar would resolve them.
