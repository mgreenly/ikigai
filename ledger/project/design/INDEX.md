# ledger — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9D, R-LAND-5E1F, R-LAND-7G2H, R-LAND-9J4K
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-2M6N, R-ROUT-4P8Q, R-ROUT-6R1S
- D3 → `project/design/D03.md` — ledger's own Carbon design assets (shipped in `share/www/static`) — owns R-ASST-3T7V, R-ASST-5W9X, R-ASST-7Y2Z
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/ledger/` location — owns R-NGNX-2B4C, R-NGNX-4D6E, R-NGNX-6F8G, R-NGNX-8H1J
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-4M6R
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT (relative stylesheet link + `font-display: optional` + self-served `src` + `<head>` preload + session-gated nginx `/srv/ledger/static/`) — owns R-7AW0-4QF8, R-7DBS-W9WM, R-7EJP-A1NB, R-7FRL-NTE0, R-7GZI-1L4P
- D9 → `project/design/D09.md` — Adopt `registry`: resolve ledger's own loopback port by name, and guard the port literals (source-scan + manifest/nginx drift) — owns R-4VDW-DRQH, R-4WLS-RJH6, R-4XTP-5B7V, R-4Z1L-J2YK
- D10 → `project/design/D10.md` — Web surface from `share/www` through the chassis (de-embed via `Spec.WWW`) — owns R-509H-WUP9, R-51HE-AMFY
- D11 → `project/design/D11.md` — MCP surface over `appkit/mcp`: `internal/mcp` becomes the seven-domain-tool table — owns R-52PA-OE6N
- D12 → `project/design/D12.md` — Delete the chassis shims (`internal/db` wrappers) and true up the doctrine doc — none (structural)

## Verification ids → Decision

- R-4VDW-DRQH → D9 → `project/design/D09.md`
- R-4WLS-RJH6 → D9 → `project/design/D09.md`
- R-4XTP-5B7V → D9 → `project/design/D09.md`
- R-4Z1L-J2YK → D9 → `project/design/D09.md`
- R-509H-WUP9 → D10 → `project/design/D10.md`
- R-51HE-AMFY → D10 → `project/design/D10.md`
- R-52PA-OE6N → D11 → `project/design/D11.md`
- R-7AW0-4QF8 → D8 → `project/design/D08.md`
- R-7DBS-W9WM → D8 → `project/design/D08.md`
- R-7EJP-A1NB → D8 → `project/design/D08.md`
- R-7FRL-NTE0 → D8 → `project/design/D08.md`
- R-7GZI-1L4P → D8 → `project/design/D08.md`
- R-ASST-3T7V → D3 → `project/design/D03.md`
- R-ASST-5W9X → D3 → `project/design/D03.md`
- R-ASST-7Y2Z → D3 → `project/design/D03.md`
- R-HOME-4M6R → D7 → `project/design/D07.md`
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
