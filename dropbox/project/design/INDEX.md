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
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT (relative stylesheet link + `font-display: optional` + self-served `src` + `<head>` preload + session-gated nginx `/srv/dropbox/static/`) — owns R-LQXL-095Q, R-LS5H-E0WF, R-LTDD-RSN4, R-LULA-5KDT, R-LVT6-JC4I
- D9 → `project/design/D09.md` — Adopt `registry`: resolve dropbox's own loopback address by name (Spec.Port, content-base default, reflection example origin) — owns R-QJ8F-AXWP, R-QKGB-OPNE
- D10 → `project/design/D10.md` — Source-scan guard (no `127.0.0.1:30xx` literal) + deploy-artifact drift guard (manifest + nginx agree with `registry`) — owns R-QLO8-2HE3, R-QMW4-G94S
- D11 → `project/design/D11.md` — Web surface from `share/www` through the chassis (`Spec.WWW`, `rt.WWW()`; delete `internal/web`) — owns R-QO40-U0VH, R-QPBX-7SM6
- D12 → `project/design/D12.md` — MCP surface over `appkit/mcp`: `internal/mcp` becomes the `list`+`get` tool table; health/reflection chassis-registered — owns R-QQJT-LKCV
- D13 → `project/design/D13.md` — Delete the `internal/db` `Open`/`Migrate` shim (embed + guards remain) and true up `dropbox/CLAUDE.md` — none (structural; shim deletion + doc truth)
- D14 → `project/design/D14.md` — Streaming byte I/O in the mirror + streaming read route (`WriteFrom`/`Open`, `http.ServeContent`, fixed copy buffer) — owns R-JV0A-6XDB, R-JW86-KP40, R-JXG2-YGUP
- D15 → `project/design/D15.md` — First-class directories in the index (`directories` table, mkdir/rmdir/list/stat, recursive delete/move) — owns R-JZVV-Q0C3, R-K13S-3S2S, R-K2BO-HJTH, R-K3JK-VBK6
- D16 → `project/design/D16.md` — The filesystem write API: `Service` write methods + loopback routes (PUT/DELETE `/content`, `/mkdir`, `/move`, `/stat`) — owns R-K4RH-93AV, R-K5ZD-MV1K, R-K77A-0MS9, R-K8F6-EEIY, R-K9N2-S69N, R-KAUZ-5Y0C
- D17 → `project/design/D17.md` — Push-up: durable upload queue + Dropbox write client + uploader worker (overwrite, coalescing, echo suppression, poison/health) — owns R-KC2V-JPR1, R-KDAR-XHHQ, R-KEIO-B98F, R-KFQK-P0Z4, R-KGYH-2SPT, R-KJE9-UC77, R-KKM6-83XW, R-KLU2-LVOL, R-KN1Y-ZNFA
- D18 → `project/design/D18.md` — Origin-tagged file events (`origin` payload field: writing client id, or `dropbox`) — owns R-KO9V-DF5Z, R-KPHR-R6WO, R-KQPO-4YND
- D19 → `project/design/D19.md` — MCP write tools (`put`/`mkdir`/`delete`/`move`; capped base64 small-file convenience) — owns R-KRXK-IQE2, R-KT5G-WI4R, R-KUDD-A9VG
- D20 → `project/design/D20.md` — The `dropbox/docs/` filesystem-API reference + route-coverage guard — owns R-KVL9-O1M5, R-KWT6-1TCU
- D21 → `project/design/D21.md` — The session-gated locations opt into the apex `@login_bounce`: a logged-out human navigation goes to sign-in, not a bare 401 (bearer tier deliberately excluded) — owns R-3MN6-J0UR, R-3NV2-WSLG, R-3P2Z-AKC5

## Verification ids → Decision

- R-3MN6-J0UR → D21 → `project/design/D21.md`
- R-3NV2-WSLG → D21 → `project/design/D21.md`
- R-3P2Z-AKC5 → D21 → `project/design/D21.md`
- R-ASST-3H6J → D3 → `project/design/D03.md`
- R-ASST-5K8L → D3 → `project/design/D03.md`
- R-ASST-7M1N → D3 → `project/design/D03.md`
- R-HOME-6P8T → D7 → `project/design/D07.md`
- R-JV0A-6XDB → D14 → `project/design/D14.md`
- R-JW86-KP40 → D14 → `project/design/D14.md`
- R-JXG2-YGUP → D14 → `project/design/D14.md`
- R-JZVV-Q0C3 → D15 → `project/design/D15.md`
- R-K13S-3S2S → D15 → `project/design/D15.md`
- R-K2BO-HJTH → D15 → `project/design/D15.md`
- R-K3JK-VBK6 → D15 → `project/design/D15.md`
- R-K4RH-93AV → D16 → `project/design/D16.md`
- R-K5ZD-MV1K → D16 → `project/design/D16.md`
- R-K77A-0MS9 → D16 → `project/design/D16.md`
- R-K8F6-EEIY → D16 → `project/design/D16.md`
- R-K9N2-S69N → D16 → `project/design/D16.md`
- R-KAUZ-5Y0C → D16 → `project/design/D16.md`
- R-KC2V-JPR1 → D17 → `project/design/D17.md`
- R-KDAR-XHHQ → D17 → `project/design/D17.md`
- R-KEIO-B98F → D17 → `project/design/D17.md`
- R-KFQK-P0Z4 → D17 → `project/design/D17.md`
- R-KGYH-2SPT → D17 → `project/design/D17.md`
- R-KJE9-UC77 → D17 → `project/design/D17.md`
- R-KKM6-83XW → D17 → `project/design/D17.md`
- R-KLU2-LVOL → D17 → `project/design/D17.md`
- R-KN1Y-ZNFA → D17 → `project/design/D17.md`
- R-KO9V-DF5Z → D18 → `project/design/D18.md`
- R-KPHR-R6WO → D18 → `project/design/D18.md`
- R-KQPO-4YND → D18 → `project/design/D18.md`
- R-KRXK-IQE2 → D19 → `project/design/D19.md`
- R-KT5G-WI4R → D19 → `project/design/D19.md`
- R-KUDD-A9VG → D19 → `project/design/D19.md`
- R-KVL9-O1M5 → D20 → `project/design/D20.md`
- R-KWT6-1TCU → D20 → `project/design/D20.md`
- R-LAND-3C9X → D1 → `project/design/D01.md`
- R-LAND-5E2Y → D1 → `project/design/D01.md`
- R-LAND-7G4Z → D1 → `project/design/D01.md`
- R-LAND-9J6A → D1 → `project/design/D01.md`
- R-LQXL-095Q → D8 → `project/design/D08.md`
- R-LS5H-E0WF → D8 → `project/design/D08.md`
- R-LTDD-RSN4 → D8 → `project/design/D08.md`
- R-LULA-5KDT → D8 → `project/design/D08.md`
- R-LVT6-JC4I → D8 → `project/design/D08.md`
- R-NGNX-2P4Q → D4 → `project/design/D04.md`
- R-NGNX-4R6S → D4 → `project/design/D04.md`
- R-NGNX-6T8U → D4 → `project/design/D04.md`
- R-NGNX-8V1W → D4 → `project/design/D04.md`
- R-QJ8F-AXWP → D9 → `project/design/D09.md`
- R-QKGB-OPNE → D9 → `project/design/D09.md`
- R-QLO8-2HE3 → D10 → `project/design/D10.md`
- R-QMW4-G94S → D10 → `project/design/D10.md`
- R-QO40-U0VH → D11 → `project/design/D11.md`
- R-QPBX-7SM6 → D11 → `project/design/D11.md`
- R-QQJT-LKCV → D12 → `project/design/D12.md`
- R-ROUT-2B5C → D2 → `project/design/D02.md`
- R-ROUT-4D7E → D2 → `project/design/D02.md`
- R-ROUT-6F9G → D2 → `project/design/D02.md`
