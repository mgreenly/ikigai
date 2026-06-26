# crm — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-2K7P, R-LAND-4M9Q, R-LAND-6N3R, R-LAND-8P5S
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-3T2V, R-ROUT-5W4X, R-ROUT-7Y6Z
- D3 → `project/design/D03.md` — Embedded Carbon design assets (crm's own copy) — owns R-ASST-2B8C, R-ASST-4D1E, R-ASST-6F3G
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/crm/` location — owns R-NGNX-2H5J, R-NGNX-4K7L, R-NGNX-6M9N, R-NGNX-8P1Q
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)

## Verification ids → Decision

- R-ASST-2B8C → D3 → `project/design/D03.md`
- R-ASST-4D1E → D3 → `project/design/D03.md`
- R-ASST-6F3G → D3 → `project/design/D03.md`
- R-LAND-2K7P → D1 → `project/design/D01.md`
- R-LAND-4M9Q → D1 → `project/design/D01.md`
- R-LAND-6N3R → D1 → `project/design/D01.md`
- R-LAND-8P5S → D1 → `project/design/D01.md`
- R-NGNX-2H5J → D4 → `project/design/D04.md`
- R-NGNX-4K7L → D4 → `project/design/D04.md`
- R-NGNX-6M9N → D4 → `project/design/D04.md`
- R-NGNX-8P1Q → D4 → `project/design/D04.md`
- R-ROUT-3T2V → D2 → `project/design/D02.md`
- R-ROUT-5W4X → D2 → `project/design/D02.md`
- R-ROUT-7Y6Z → D2 → `project/design/D02.md`
