# dropbox — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9X, R-LAND-5E2Y, R-LAND-7G4Z, R-LAND-9J6A
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-2B5C, R-ROUT-4D7E, R-ROUT-6F9G
- D3 → `project/design/D03.md` — Embedded Carbon design assets (dropbox's own copy) — owns R-ASST-3H6J, R-ASST-5K8L, R-ASST-7M1N
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/dropbox/` location — owns R-NGNX-2P4Q, R-NGNX-4R6S, R-NGNX-6T8U, R-NGNX-8V1W
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-6P8T

## Verification ids → Decision

- R-ASST-3H6J → D3 → `project/design/D03.md`
- R-ASST-5K8L → D3 → `project/design/D03.md`
- R-ASST-7M1N → D3 → `project/design/D03.md`
- R-LAND-3C9X → D1 → `project/design/D01.md`
- R-LAND-5E2Y → D1 → `project/design/D01.md`
- R-LAND-7G4Z → D1 → `project/design/D01.md`
- R-LAND-9J6A → D1 → `project/design/D01.md`
- R-NGNX-2P4Q → D4 → `project/design/D04.md`
- R-NGNX-4R6S → D4 → `project/design/D04.md`
- R-NGNX-6T8U → D4 → `project/design/D04.md`
- R-NGNX-8V1W → D4 → `project/design/D04.md`
- R-ROUT-2B5C → D2 → `project/design/D02.md`
- R-ROUT-4D7E → D2 → `project/design/D02.md`
- R-ROUT-6F9G → D2 → `project/design/D02.md`
- R-HOME-6P8T → D7 → `project/design/D07.md`
