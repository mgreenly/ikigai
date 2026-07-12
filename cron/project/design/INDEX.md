# cron — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C9K, R-LAND-5E2L, R-LAND-7G4M, R-LAND-9J6N
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-2P8Q, R-ROUT-4R1S, R-ROUT-6T3U
- D3 → `project/design/D03.md` — cron's own Carbon design assets (shipped in `share/www/static`) — owns R-ASST-3V7W, R-ASST-5X9Y, R-ASST-7Z2A
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/cron/` location — owns R-NGNX-3B6C, R-NGNX-5D8E, R-NGNX-7F1G, R-NGNX-9H3J
- D5 → `project/design/D05.md` — Docs state current truth: state the landing-page truth in cron's doctrine — none (structural; docs-only)
- D6 → `project/design/D06.md` — A top-left Home link to the dashboard landing page — owns R-HOME-2K4P
- D7 → `project/design/D07.md` — Self-serve the landing page's fonts and eliminate the FOUT — owns R-21DE-LOX3, R-22LA-ZGNS, R-23T7-D8EH, R-2513-R056, R-2690-4RVV
- D8 → `project/design/D08.md` — Composition-root normalization: the `Spec` is declared inline in `cmd/cron/main.go` — none (structural)
- D9 → `project/design/D09.md` — Web surface from `share/www` through the chassis (de-embed) — owns R-LPMQ-FKBR, R-LQUM-TC2G
- D10 → `project/design/D10.md` — MCP surface over `appkit/mcp`: `internal/mcp` becomes the tool table — owns R-LS2J-73T5
- D11 → `project/design/D11.md` — Adopt `registry`: resolve cron's port by name and guard the deploy artifacts against drift — owns R-LTAF-KVJU, R-LUIB-YNAJ, R-LVQ8-CF18
- D12 → `project/design/D12.md` — Delete the chassis shims (`internal/db` wrappers) and true up the doctrine header — none (structural)
- D13 → `project/design/D13.md` — The session-gated locations opt into the apex `@login_bounce`: a logged-out human navigation goes to sign-in, not a bare 401 (bearer tier deliberately excluded) — owns R-3V6H-7F1M, R-3WED-L6SB, R-3XM9-YYJ0
- D14 → `project/design/D14.md` — Event-routing conformance: kind `tick`, subject `/<schedule name>`, live one-family reflection — owns R-PQH6-2RYI, R-PRP2-GJP7, R-PSWY-UBFW, R-PU4V-836L, R-PVCR-LUXA

## Verification ids → Decision

- R-21DE-LOX3 → D7 → `project/design/D07.md`
- R-22LA-ZGNS → D7 → `project/design/D07.md`
- R-23T7-D8EH → D7 → `project/design/D07.md`
- R-2513-R056 → D7 → `project/design/D07.md`
- R-2690-4RVV → D7 → `project/design/D07.md`
- R-3V6H-7F1M → D13 → `project/design/D13.md`
- R-3WED-L6SB → D13 → `project/design/D13.md`
- R-3XM9-YYJ0 → D13 → `project/design/D13.md`
- R-ASST-3V7W → D3 → `project/design/D03.md`
- R-ASST-5X9Y → D3 → `project/design/D03.md`
- R-ASST-7Z2A → D3 → `project/design/D03.md`
- R-HOME-2K4P → D6 → `project/design/D06.md`
- R-LAND-3C9K → D1 → `project/design/D01.md`
- R-LAND-5E2L → D1 → `project/design/D01.md`
- R-LAND-7G4M → D1 → `project/design/D01.md`
- R-LAND-9J6N → D1 → `project/design/D01.md`
- R-LPMQ-FKBR → D9 → `project/design/D09.md`
- R-LQUM-TC2G → D9 → `project/design/D09.md`
- R-LS2J-73T5 → D10 → `project/design/D10.md`
- R-LTAF-KVJU → D11 → `project/design/D11.md`
- R-LUIB-YNAJ → D11 → `project/design/D11.md`
- R-LVQ8-CF18 → D11 → `project/design/D11.md`
- R-NGNX-3B6C → D4 → `project/design/D04.md`
- R-NGNX-5D8E → D4 → `project/design/D04.md`
- R-NGNX-7F1G → D4 → `project/design/D04.md`
- R-NGNX-9H3J → D4 → `project/design/D04.md`
- R-PQH6-2RYI → D14 → `project/design/D14.md`
- R-PRP2-GJP7 → D14 → `project/design/D14.md`
- R-PSWY-UBFW → D14 → `project/design/D14.md`
- R-PU4V-836L → D14 → `project/design/D14.md`
- R-PVCR-LUXA → D14 → `project/design/D14.md`
- R-ROUT-2P8Q → D2 → `project/design/D02.md`
- R-ROUT-4R1S → D2 → `project/design/D02.md`
- R-ROUT-6T3U → D2 → `project/design/D02.md`
