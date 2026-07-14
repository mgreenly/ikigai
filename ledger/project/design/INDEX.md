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
- D13 → `project/design/D13.md` — The session-gated locations opt into the apex `@login_bounce`: a logged-out human navigation goes to sign-in, not a bare 401 (bearer tier deliberately excluded) — owns R-3FBS-8EEL, R-3GJO-M65A, R-3HRK-ZXVZ
- D14 → `project/design/D14.md` — `external_ref`: opt-in idempotency for derived transactions (nullable column + partial unique index, duplicate rejected via the shared `conflict` error, event-payload field, reverse interaction) — owns R-FP14-UYWQ, R-FQ91-8QNF, R-FRGX-MIE4, R-FSOU-0A4T, R-FTWQ-E1VI, R-FV4M-RTM7, R-FWCJ-5LCW
- D15 → `project/design/D15.md` — Event-routing conformance: kind `recorded`, empty subject (`ledger:recorded`), family registry, new outbox migration — owns R-FXKF-JD3L, R-FYSB-X4UA, R-G184-OOBO, R-G2G1-2G2D
- D16 → `project/design/D16.md` — Structured MCP adoption: `StructuredResult` + per-verb `outputSchema` on the six domain result verbs, typed closed-vocabulary error codes, `describe` prose exception — owns R-9FRN-SGDT, R-9GZK-684I, R-9I7G-JZV7, R-9JFC-XRLW, R-9KN9-BJCL, R-9LV5-PB3A, R-9N32-32TZ, R-9OAY-GUKO, R-9PIU-UMBD, R-9QQR-8E22, R-9RYN-M5SR, R-9T6J-ZXJG, R-9UEG-DPA5

## Verification ids → Decision

- R-3FBS-8EEL → D13 → `project/design/D13.md`
- R-3GJO-M65A → D13 → `project/design/D13.md`
- R-3HRK-ZXVZ → D13 → `project/design/D13.md`
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
- R-9FRN-SGDT → D16 → `project/design/D16.md`
- R-9GZK-684I → D16 → `project/design/D16.md`
- R-9I7G-JZV7 → D16 → `project/design/D16.md`
- R-9JFC-XRLW → D16 → `project/design/D16.md`
- R-9KN9-BJCL → D16 → `project/design/D16.md`
- R-9LV5-PB3A → D16 → `project/design/D16.md`
- R-9N32-32TZ → D16 → `project/design/D16.md`
- R-9OAY-GUKO → D16 → `project/design/D16.md`
- R-9PIU-UMBD → D16 → `project/design/D16.md`
- R-9QQR-8E22 → D16 → `project/design/D16.md`
- R-9RYN-M5SR → D16 → `project/design/D16.md`
- R-9T6J-ZXJG → D16 → `project/design/D16.md`
- R-9UEG-DPA5 → D16 → `project/design/D16.md`
- R-ASST-3T7V → D3 → `project/design/D03.md`
- R-ASST-5W9X → D3 → `project/design/D03.md`
- R-ASST-7Y2Z → D3 → `project/design/D03.md`
- R-FP14-UYWQ → D14 → `project/design/D14.md`
- R-FQ91-8QNF → D14 → `project/design/D14.md`
- R-FRGX-MIE4 → D14 → `project/design/D14.md`
- R-FSOU-0A4T → D14 → `project/design/D14.md`
- R-FTWQ-E1VI → D14 → `project/design/D14.md`
- R-FV4M-RTM7 → D14 → `project/design/D14.md`
- R-FWCJ-5LCW → D14 → `project/design/D14.md`
- R-FXKF-JD3L → D15 → `project/design/D15.md`
- R-FYSB-X4UA → D15 → `project/design/D15.md`
- R-G184-OOBO → D15 → `project/design/D15.md`
- R-G2G1-2G2D → D15 → `project/design/D15.md`
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
