# Phase 48 — Merge mints the forward-routing alias; AliasStore keys on the one normalizer

*Realizes design Decision 26 (the merge work item & execution — the alias insert) and 25 (aliases table & name resolution — the alias-key form). Depends on Phase 43 (merge execution: `mergeSubjects`, the one write tx) and Phase 42 (`AliasStore`, `Resolver`).*

The merge folds a loser subject into a winner but leaves **no forward-routing
alias** — `mergeSubjects` existence-checks the loser, discards it, and never
calls `aliases.Insert` (D26 step C-6 is absent). And `AliasStore` keys on the
deleted space-form `normalize()`, not the single `Normalize`, so even a stored
alias would be looked up under the wrong key. This phase makes a real merge
leave behind a redirect that `Resolver.ResolveByName` can follow.

**End state.**

- `internal/wiki/aliases.go` — `Insert` and `GetByNormName` key on `Normalize`
  (D3, the one normalizer), replacing their two space-form `normalize()` calls,
  so a written alias is read back under the same key form the `Resolver` uses
  (D25). (The space-form `normalize()` itself is not yet deleted here — `links.go`
  still calls it; Phase 49 removes the last caller and the function.)
- `internal/wiki/service.go` `mergeSubjects` — Phase A **loads and keeps** the
  loser subject (`SubjectStore.Get(merge.FromSubjectID)`), retaining its `Name`
  for the alias key instead of discarding the row. Phase C, inside the existing
  single write transaction and after the loser delete, gains the mandatory
  step C-6: `aliases.Insert(Normalize(loser.name) → winner)` on the tx-bound
  `AliasStore`. The strict ordering (repoint inbound aliases → delete loser →
  insert the new alias) and the all-or-nothing commit are unchanged; any error
  still rolls the whole merge back.
- The merge surface, queue, dispatch, and the rest of `mergeSubjects` (D26/D27)
  are untouched — this is the alias-insert gap and the alias-key form only.

**Done when:** the suite is green (per design *Conventions*) and these design
Verification ids are covered by clearly-named tests on the worker seam against a
real temp SQLite with a scripted mock compiler (no live LLM):

- **R-HUDR-AWS9** (D26) — after a real merge of loser `L` into winner `W` runs to
  `done`, `Resolver.ResolveByName(L.name)` returns `W`, proving step C-6 inserted
  a forward-routing alias keyed by `Normalize(L.name)` that the resolver follows.
  A merge that skips the insert fails this with `ErrSubjectNotFound`.

This phase also makes D26's already-shipped **R-NGVA-LS02** (the `aliases` row
`Normalize(L.name) → W` present after one commit) genuinely pass; its STATUS
marker stays `✅` and is not re-flipped.
