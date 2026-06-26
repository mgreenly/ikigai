# gmail — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3F7K, R-LAND-5H9M, R-LAND-7J2N, R-LAND-9K4P
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-4M6Q, R-ROUT-6N8R, R-ROUT-8P1S
- D3 → `project/design/D03.md` — Embedded Carbon design assets (gmail's own copy) — owns R-ASST-3T5V, R-ASST-5W7X, R-ASST-7Y9Z
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/gmail/` location — owns R-NGNX-3B6C, R-NGNX-5D8E, R-NGNX-7F1G, R-NGNX-9H3J
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-7Q9U

## Verification ids → Decision

- R-ASST-3T5V → D3 → `project/design/D03.md`
- R-ASST-5W7X → D3 → `project/design/D03.md`
- R-ASST-7Y9Z → D3 → `project/design/D03.md`
- R-LAND-3F7K → D1 → `project/design/D01.md`
- R-LAND-5H9M → D1 → `project/design/D01.md`
- R-LAND-7J2N → D1 → `project/design/D01.md`
- R-LAND-9K4P → D1 → `project/design/D01.md`
- R-NGNX-3B6C → D4 → `project/design/D04.md`
- R-NGNX-5D8E → D4 → `project/design/D04.md`
- R-NGNX-7F1G → D4 → `project/design/D04.md`
- R-NGNX-9H3J → D4 → `project/design/D04.md`
- R-ROUT-4M6Q → D2 → `project/design/D02.md`
- R-ROUT-6N8R → D2 → `project/design/D02.md`
- R-ROUT-8P1S → D2 → `project/design/D02.md`
- R-HOME-7Q9U → D7 → `project/design/D07.md`
