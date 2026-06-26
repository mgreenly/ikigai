# scripts — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-7Q3D, R-LAND-9R5F, R-LAND-1S7G, R-LAND-3T9H
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-8U2J, R-ROUT-1V4K, R-ROUT-3W6L
- D3 → `project/design/D03.md` — Embedded Carbon design assets (scripts's own copy) — owns R-ASST-5X8M, R-ASST-7Y1N, R-ASST-9Z3P
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/scripts/` location — owns R-NGNX-2A5Q, R-NGNX-4B7R, R-NGNX-6C9S, R-NGNX-8D1T
- D5 → `project/design/D05.md` — Docs state current truth: state the landing-page surface in scripts's doctrine — none (structural; docs-only)

## Verification ids → Decision

- R-ASST-5X8M → D3 → `project/design/D03.md`
- R-ASST-7Y1N → D3 → `project/design/D03.md`
- R-ASST-9Z3P → D3 → `project/design/D03.md`
- R-LAND-1S7G → D1 → `project/design/D01.md`
- R-LAND-3T9H → D1 → `project/design/D01.md`
- R-LAND-7Q3D → D1 → `project/design/D01.md`
- R-LAND-9R5F → D1 → `project/design/D01.md`
- R-NGNX-2A5Q → D4 → `project/design/D04.md`
- R-NGNX-4B7R → D4 → `project/design/D04.md`
- R-NGNX-6C9S → D4 → `project/design/D04.md`
- R-NGNX-8D1T → D4 → `project/design/D04.md`
- R-ROUT-1V4K → D2 → `project/design/D02.md`
- R-ROUT-3W6L → D2 → `project/design/D02.md`
- R-ROUT-8U2J → D2 → `project/design/D02.md`
