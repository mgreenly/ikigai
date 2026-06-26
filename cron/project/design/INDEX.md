# cron — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9K, R-LAND-5E2L, R-LAND-7G4M, R-LAND-9J6N
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-2P8Q, R-ROUT-4R1S, R-ROUT-6T3U
- D3 → `project/design/D03.md` — Embedded Carbon design assets (cron's own copy) — owns R-ASST-3V7W, R-ASST-5X9Y, R-ASST-7Z2A
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/cron/` location — owns R-NGNX-3B6C, R-NGNX-5D8E, R-NGNX-7F1G, R-NGNX-9H3J
- D5 → `project/design/D05.md` — Docs state current truth: state the landing-page truth in cron's doctrine — none (structural; docs-only)

## Verification ids → Decision

- R-ASST-3V7W → D3 → `project/design/D03.md`
- R-ASST-5X9Y → D3 → `project/design/D03.md`
- R-ASST-7Z2A → D3 → `project/design/D03.md`
- R-LAND-3C9K → D1 → `project/design/D01.md`
- R-LAND-5E2L → D1 → `project/design/D01.md`
- R-LAND-7G4M → D1 → `project/design/D01.md`
- R-LAND-9J6N → D1 → `project/design/D01.md`
- R-NGNX-3B6C → D4 → `project/design/D04.md`
- R-NGNX-5D8E → D4 → `project/design/D04.md`
- R-NGNX-7F1G → D4 → `project/design/D04.md`
- R-NGNX-9H3J → D4 → `project/design/D04.md`
- R-ROUT-2P8Q → D2 → `project/design/D02.md`
- R-ROUT-4R1S → D2 → `project/design/D02.md`
- R-ROUT-6T3U → D2 → `project/design/D02.md`
