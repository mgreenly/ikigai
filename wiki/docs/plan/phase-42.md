# Phase 42 — Aliases table & name resolution

*Realizes design Decision 25 (aliases table & name resolution). Depends on Phase 02 (D3 data model / stores), Phase 07 (D4 ingest / `plannedSubject`), Phase 08 (D9 `ask`), Phase 19 (D11 paths).*

The wiki gains the durable forward-routing record a merge leaves behind, and a single shared name lookup that honors it on every path.

In `internal/wiki`:

- A new timestamped migration creates the `aliases` table — `norm_name TEXT NOT NULL UNIQUE`, `subject_id TEXT NOT NULL REFERENCES subjects(id) ON DELETE RESTRICT` (the schema's **first** foreign key — deliberate, per D25), `name`, `created_by`, `created_at` (RFC3339Nano), plus the `aliases_subject` index. Authored via `bin/new-migration wiki create_aliases`.
- `AliasStore` (constructed over the `sqlStore` handle like the other stores) with `Insert`, `RepointSubject(from, to)`, and `GetByNormName`.
- A `Resolver` (`NewResolver(db)`) whose `ResolveByName(ctx, name)` is **subjects-first, then a single alias hop**: live subject → return it; else alias → load the survivor via `Get(subject_id)`; else `ErrSubjectNotFound`. Single-hop is correct by construction (the FK + the merge's eager repoint guarantee every alias names a live, non-aliased survivor) — no chain-following, no cycle logic.

Both name-lookup call sites adopt the `Resolver`, so the alias effect reaches ingest *and* `ask` (which lives in a separate package and must not bypass it): `plannedSubject` resolves between its current `GetByNormName` miss and the mint; `ask.gatherPages` resolves the question's named subjects through it. The composition root constructs the `Resolver` and injects it where both consumers need it.

Resolution stays O(log n) — at most two indexed UNIQUE point lookups, no scan, no LLM.

**Done when:** R-BGPF-NVTU, R-BHXC-1NKJ, R-BJ58-FFB8, R-BKD4-T71X, R-BLL1-6YSM, R-BMSX-KQJB, R-BO0T-YIA0, R-BP8Q-CA0P are each covered by a clearly-named test (the table-constraint and FK `ON DELETE RESTRICT` behaviors against a real temp SQLite with `foreign_keys=ON`; the resolver and both call-site behaviors with an alias row installed directly via `AliasStore`), and the suite is green.
