# sites — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9K, R-LAND-5E2M, R-LAND-7G4P, R-LAND-9J6R
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-4Q8B, R-ROUT-6S1D, R-ROUT-8U3F
- D3 → `project/design/D03.md` — Embedded Carbon design assets (sites's own copy) — owns R-ASST-3H7N, R-ASST-5K9Q, R-ASST-7M2S
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/sites/` landing root beside the existing static tiers — owns R-NGNX-3P6T, R-NGNX-5R8V, R-NGNX-7T1X, R-NGNX-9W4Z
- D5 → `project/design/D05.md` — Docs state current truth: state the standardized landing card in sites's self-description (no "no UI" claim to purge) — none (structural; docs-only)

## Verification ids → Decision

- R-ASST-3H7N → D3 → `project/design/D03.md`
- R-ASST-5K9Q → D3 → `project/design/D03.md`
- R-ASST-7M2S → D3 → `project/design/D03.md`
- R-LAND-3C9K → D1 → `project/design/D01.md`
- R-LAND-5E2M → D1 → `project/design/D01.md`
- R-LAND-7G4P → D1 → `project/design/D01.md`
- R-LAND-9J6R → D1 → `project/design/D01.md`
- R-NGNX-3P6T → D4 → `project/design/D04.md`
- R-NGNX-5R8V → D4 → `project/design/D04.md`
- R-NGNX-7T1X → D4 → `project/design/D04.md`
- R-NGNX-9W4Z → D4 → `project/design/D04.md`
- R-ROUT-4Q8B → D2 → `project/design/D02.md`
- R-ROUT-6S1D → D2 → `project/design/D02.md`
- R-ROUT-8U3F → D2 → `project/design/D02.md`
