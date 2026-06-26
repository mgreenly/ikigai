# notify — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C8K, R-LAND-5D1M, R-LAND-7E4N, R-LAND-9F6P
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-4G2Q, R-ROUT-6H5R, R-ROUT-8J7S
- D3 → `project/design/D03.md` — Embedded Carbon design assets (notify's own copy) — owns R-ASST-3K9T, R-ASST-5L2V, R-ASST-7M4W
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/notify/` location — owns R-NGNX-3N6X, R-NGNX-5P8Y, R-NGNX-7Q1Z, R-NGNX-9R3B
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-5N7S

## Verification ids → Decision

- R-ASST-3K9T → D3 → `project/design/D03.md`
- R-ASST-5L2V → D3 → `project/design/D03.md`
- R-ASST-7M4W → D3 → `project/design/D03.md`
- R-LAND-3C8K → D1 → `project/design/D01.md`
- R-LAND-5D1M → D1 → `project/design/D01.md`
- R-LAND-7E4N → D1 → `project/design/D01.md`
- R-LAND-9F6P → D1 → `project/design/D01.md`
- R-NGNX-3N6X → D4 → `project/design/D04.md`
- R-NGNX-5P8Y → D4 → `project/design/D04.md`
- R-NGNX-7Q1Z → D4 → `project/design/D04.md`
- R-NGNX-9R3B → D4 → `project/design/D04.md`
- R-ROUT-4G2Q → D2 → `project/design/D02.md`
- R-ROUT-6H5R → D2 → `project/design/D02.md`
- R-ROUT-8J7S → D2 → `project/design/D02.md`
- R-HOME-5N7S → D7 → `project/design/D07.md`
