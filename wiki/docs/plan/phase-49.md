# Phase 49 — Alias-aware read-time link projection; collapse the last normalizer

*Realizes design Decision 12 (page links: read-time mention detection + markdown footer, alias-aware) and 25 (aliases table & name resolution — `AliasStore.ListAll`). Depends on Phase 48 (`AliasStore` keyed on `Normalize`) and Phase 20 (the original `Mentions`/`PageWithLinks`/`RenderFooter`).*

Read-time link projection matches subject **names only** and on the wrong
(space-form) normalizer, so a page that mentions a merged-away subject by a
folded short name links to nothing — the reported orphan-links bug. This phase
makes the projection alias-aware in both directions over the single normalizer,
and retires the space-form `normalize()` now that its last caller is gone.

**End state.**

- `internal/wiki/links.go` — `Mentions` becomes
  `Mentions(body string, others []SubjectKeys) []Subject`: it matches each
  candidate's `Keys` (its own `norm_name` ∪ its aliases' `norm_name`s) as a
  whole, edge-bounded run in `Normalize(body)`, and a match via an alias key
  yields the **canonical** subject. Matching runs over `Normalize` (D3), not the
  space-form normalizer.
- `internal/wiki/data_model.go` — the now-orphaned `func normalize` is deleted
  (Phase 48 removed the `aliases.go` callers; this phase removes the `links.go`
  callers, leaving none); its `data_model_test.go` unit test is dropped or
  retargeted onto `Normalize`. `Normalize` is the one and only normalizer.
- `internal/wiki/aliases.go` — new `AliasStore.ListAll(ctx) ([]Alias, error)`
  returning every alias row, consumed by the projection (D25).
- `internal/wiki/service.go` — `PageWithLinks` loads all subjects and all
  aliases (`AliasStore.ListAll`), groups aliases by `subject_id` into one
  `SubjectKeys` per subject, and projects both directions: outbound
  `Mentions(thisBody, allOtherSubjectKeys)`, inbound this-subject ∈
  `Mentions(thatBody, [thisKeys])`. Links render the canonical subject's `Name`
  and `type/slug` `Path` (D11), deduped and path-ordered; the self-exclusion,
  the empty-footer rule, and the untouched 12k cap (footer is render-time) are
  preserved. `RenderFooter` is unchanged.

**Done when:** the suite is green (per design *Conventions*) and these design
Verification ids are covered by clearly-named tests — the pure cases table-tested
directly on `Mentions`/`SubjectKeys`, the end-to-end case against a real temp
SQLite via `PageWithLinks` with the alias row installed directly (no merge
needed, per the id):

- **R-1WP9-CLM9** (D12, pure) — an alias key in the body resolves to the canonical
  survivor `W`, rendered `[Giorgio Vasari](entity/giorgio-vasari)`, never the
  variant text or a dead path.
- **R-1XX5-QDCY** (D12, pure) — inbound symmetry: a body naming `W` only by an
  alias key still counts as mentioning `W`.
- **R-1Z52-453N** (D12, real temp SQLite) — with an alias row `vasari → W`
  installed, `PageWithLinks` on a third page using the bare short name links to
  `W` rendered canonically; with **no** alias row the same body yields no link,
  proving the projection consults `AliasStore.ListAll`.
- **R-ZY11-SUQS** (D12, reworded) — a name that is neither an exact subject
  `norm_name` nor a registered alias `norm_name` produces no link, and a
  substring inside a larger token (`cat` in `category`) is not a match.

The same rewrite must keep D12's existing ids green under the new signature:
R-ZUDC-NJIP, R-ZVL9-1B9E, R-ZWT5-F303, R-ZZ8Y-6MHH, R-00GU-KE86.
