# sites — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9K, R-LAND-5E2M, R-LAND-7G4P, R-LAND-9J6R
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-4Q8B, R-ROUT-6S1D, R-ROUT-8U3F
- D3 → `project/design/D03.md` — Embedded Carbon design assets (sites's own copy) — owns R-ASST-3H7N, R-ASST-5K9Q, R-ASST-7M2S
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/sites/` landing root beside the existing static tiers — owns R-NGNX-3P6T, R-NGNX-5R8V, R-NGNX-7T1X, R-NGNX-9W4Z
- D5 → `project/design/D05.md` — Docs state current truth: state the standardized landing card in sites's self-description (no "no UI" claim to purge) — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-9S3W
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT (relative stylesheet link + `font-display: optional` + self-served `src` + `<head>` preload + session-gated nginx `/srv/sites/static/`) — owns R-629P-84O5, R-63HL-LWEU, R-64PH-ZO5J, R-65XE-DFW8, R-675A-R7MX
- D9 → `project/design/D09.md` — Resolve sites's own port and the dropbox mirror address by name through the shared `registry` (import + startup resolve at the composition root + committed `go.mod` replace; behavior-preserving) — owns R-7K2P-QN4D, R-7L9F-XW3H, R-7M4C-BV8J, R-7N6R-TZ2Q
- D10 → `project/design/D10.md` — `internal/files`: confined filesystem operations as native Go (ports the confined Read/Edit/Glob/Grep/Write/List/Mkdir + symlink-resolving ConfinePath; no agentkit, no JSON, no agent framing) — owns R-027Y-BQ1I, R-03FU-PHS7, R-04NR-39IW, R-05VN-H19L, R-073J-UT0A, R-08BG-8KQZ, R-09JC-MCHO, R-0AR9-048D, R-0D71-RNPR, R-0EEY-5FGG
- D11 → `project/design/D11.md` — Rewire the MCP file tools onto `internal/files` and drop `agentkit` (delete the bridge, hand-write the four schemas, cleaner structured results, typed confinement envelope, remove the `go.mod` require+replace; surface-preserving) — owns R-0FMU-J775, R-0GUQ-WYXU, R-0I2N-AQOJ, R-0JAJ-OIF8, R-0KIG-2A5X

## Verification ids → Decision

- R-027Y-BQ1I → D10 → `project/design/D10.md`
- R-03FU-PHS7 → D10 → `project/design/D10.md`
- R-04NR-39IW → D10 → `project/design/D10.md`
- R-05VN-H19L → D10 → `project/design/D10.md`
- R-073J-UT0A → D10 → `project/design/D10.md`
- R-08BG-8KQZ → D10 → `project/design/D10.md`
- R-09JC-MCHO → D10 → `project/design/D10.md`
- R-0AR9-048D → D10 → `project/design/D10.md`
- R-0D71-RNPR → D10 → `project/design/D10.md`
- R-0EEY-5FGG → D10 → `project/design/D10.md`
- R-0FMU-J775 → D11 → `project/design/D11.md`
- R-0GUQ-WYXU → D11 → `project/design/D11.md`
- R-0I2N-AQOJ → D11 → `project/design/D11.md`
- R-0JAJ-OIF8 → D11 → `project/design/D11.md`
- R-0KIG-2A5X → D11 → `project/design/D11.md`
- R-ASST-3H7N → D3 → `project/design/D03.md`
- R-ASST-5K9Q → D3 → `project/design/D03.md`
- R-ASST-7M2S → D3 → `project/design/D03.md`
- R-LAND-3C9K → D1 → `project/design/D01.md`
- R-LAND-5E2M → D1 → `project/design/D01.md`
- R-LAND-7G4P → D1 → `project/design/D01.md`
- R-LAND-9J6R → D1 → `project/design/D01.md`
- R-629P-84O5 → D8 → `project/design/D08.md`
- R-63HL-LWEU → D8 → `project/design/D08.md`
- R-64PH-ZO5J → D8 → `project/design/D08.md`
- R-65XE-DFW8 → D8 → `project/design/D08.md`
- R-675A-R7MX → D8 → `project/design/D08.md`
- R-7K2P-QN4D → D9 → `project/design/D09.md`
- R-7L9F-XW3H → D9 → `project/design/D09.md`
- R-7M4C-BV8J → D9 → `project/design/D09.md`
- R-7N6R-TZ2Q → D9 → `project/design/D09.md`
- R-NGNX-3P6T → D4 → `project/design/D04.md`
- R-NGNX-5R8V → D4 → `project/design/D04.md`
- R-NGNX-7T1X → D4 → `project/design/D04.md`
- R-NGNX-9W4Z → D4 → `project/design/D04.md`
- R-ROUT-4Q8B → D2 → `project/design/D02.md`
- R-ROUT-6S1D → D2 → `project/design/D02.md`
- R-ROUT-8U3F → D2 → `project/design/D02.md`
- R-HOME-9S3W → D7 → `project/design/D07.md`
