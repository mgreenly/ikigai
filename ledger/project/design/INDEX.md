# ledger — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9D, R-LAND-5E1F, R-LAND-7G2H, R-LAND-9J4K
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-2M6N, R-ROUT-4P8Q, R-ROUT-6R1S
- D3 → `project/design/D03.md` — Embedded Carbon design assets (ledger's own copy) — owns R-ASST-3T7V, R-ASST-5W9X, R-ASST-7Y2Z
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/ledger/` location — owns R-NGNX-2B4C, R-NGNX-4D6E, R-NGNX-6F8G, R-NGNX-8H1J
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-4M6R

## Verification ids → Decision

- R-ASST-3T7V → D3 → `project/design/D03.md`
- R-ASST-5W9X → D3 → `project/design/D03.md`
- R-ASST-7Y2Z → D3 → `project/design/D03.md`
- R-LAND-3C9D → D1 → `project/design/D01.md`
- R-LAND-5E1F → D1 → `project/design/D01.md`
- R-LAND-7G2H → D1 → `project/design/D01.md`
- R-LAND-9J4K → D1 → `project/design/D01.md`
- R-NGNX-2B4C → D4 → `project/design/D04.md`
- R-NGNX-4D6E → D4 → `project/design/D04.md`
- R-NGNX-6F8G → D4 → `project/design/D04.md`
- R-NGNX-8H1J → D4 → `project/design/D04.md`
- R-ROUT-2M6N → D2 → `project/design/D02.md`
- R-ROUT-4P8Q → D2 → `project/design/D02.md`
- R-ROUT-6R1S → D2 → `project/design/D02.md`
- R-HOME-4M6R → D7 → `project/design/D07.md`
