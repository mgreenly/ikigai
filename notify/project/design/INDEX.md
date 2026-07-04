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
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT (relative stylesheet link + `font-display: optional` + self-served `src` + `<head>` preload + session-gated nginx `/srv/notify/static/`) — owns R-8JS0-IQDX, R-8KZW-WI4M, R-8M7T-A9VB, R-8NFP-O1M0, R-8ONM-1TCP
- D9 → `project/design/D09.md` — Adopt `registry`: resolve notify's own and its peers' loopback ports by name at the composition root (own port via `MustPort`, crm/prompts feed defaults via `BaseURL`, env overrides unchanged, go.mod require+replace) — owns R-RGSP-4A1K, R-RGCF-4B2L, R-RGPF-4C3M, R-RGEO-4D4N
- D10 → `project/design/D10.md` — Prove no loopback-port literal survives (source-scan guard) and guard the deploy artifacts (`etc/manifest.env`, `etc/nginx.conf`) against registry drift via registry-derived test expectations — owns R-RGNL-4E5P, R-RGDR-4F6Q

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
- R-8JS0-IQDX → D8 → `project/design/D08.md`
- R-8KZW-WI4M → D8 → `project/design/D08.md`
- R-8M7T-A9VB → D8 → `project/design/D08.md`
- R-8NFP-O1M0 → D8 → `project/design/D08.md`
- R-8ONM-1TCP → D8 → `project/design/D08.md`
- R-HOME-5N7S → D7 → `project/design/D07.md`
- R-RGSP-4A1K → D9 → `project/design/D09.md`
- R-RGCF-4B2L → D9 → `project/design/D09.md`
- R-RGPF-4C3M → D9 → `project/design/D09.md`
- R-RGEO-4D4N → D9 → `project/design/D09.md`
- R-RGNL-4E5P → D10 → `project/design/D10.md`
- R-RGDR-4F6Q → D10 → `project/design/D10.md`
