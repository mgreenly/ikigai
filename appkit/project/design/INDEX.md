# appkit — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to
its Decision/file. Resolve an id by grepping this index (or the Decision files
directly). Regenerate this manifest whenever a Decision is added or its
Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — Manifest readers resolve *through* the per-app `current` symlink (`appkit/inventory`) — owns R-YO06-9I18, R-YP82-N9RX
- D2 → `project/design/D02.md` — `bin/registry` resolves through `current` — owns R-YQFZ-11IM
- D3 → `project/design/D03.md` — Local dev runtime layout mirrors the box (`bin/start` stages a prod-shaped manifest root) — owns R-YRNV-ET9B
- D4 → `project/design/D04.md` — Retire the stable sibling path and its hand-placed artifacts — owns R-YSVR-SL00, R-YU3O-6CQP
- D5 → `project/design/D05.md` — WWW-root resolution in `appkit/config` (`share/current/www` on box, `./share/www` dev, `<APP>_WWW_PATH` override) — owns R-LWOU-OWWQ, R-LXWR-2ONF, R-LZ4N-GGE4
- D6 → `project/design/D06.md` — The `appkit/web` package: templates + static assets over an on-disk root — owns R-M0CJ-U84T, R-M1KG-7ZVI, R-M2SC-LRM7, R-M408-ZJCW, R-M585-DB3L
- D7 → `project/design/D07.md` — Chassis integration: `Spec.WWW`, the auto-mounted static route, `Router.WWW()` — owns R-M7NY-4UKZ, R-M8VU-IMBO, R-MA3Q-WE2D, R-MBBN-A5T2
- D8 → `project/design/D08.md` — The `appkit/mcp` JSON-RPC transport over a declared tool table, with structured results (protocol `2025-06-18`, `OutputSchema`, `StructuredResult`, typed `ErrorCode`, `-32603` handler faults) — owns R-MCJJ-NXJR, R-MDRG-1PAG, R-MEZC-FH15, R-MG78-T8RU, R-MHF5-70IJ, R-MIN1-KS98, R-MJUX-YJZX, R-WPNN-6Q9E, R-WQVJ-KI03, R-WTBC-C1HH, R-WUJ8-PT86, R-WVR5-3KYV
- D9 → `project/design/D09.md` — Chassis-owned standard tools: `health` and `reflection`, structured (declared `outputSchema`s, `structuredContent` results, coded unknown-kind error) — owns R-ML2U-CBQM, R-7EK6-8030, R-7FS2-LRTP, R-7GZY-ZJKE, R-7I7V-DBB3, R-WWZ1-HCPK, R-WY6X-V4G9, R-WZEU-8W6Y
- D10 → `project/design/D10.md` — Chassis-owned consumer loops: `Spec.Consumers` — owns R-4199-A0U9, R-42H5-NSKY, R-44WY-FC2C, R-464U-T3T1, R-47CR-6VJQ, R-48KN-KNAF, R-49SJ-YF14
- D11 → `project/design/D11.md` — Event-routing conformance: the chassis compiles and plumbs the family/kind revision — owns R-7JFR-R31S, R-7LVK-IMJ6
- D12 → `project/design/D12.md` — The loopback-only route class: `LoopbackOnly` + `Router.HandleLoopback`, `/feed` mount wrapped, predicate narrowed to `X-Forwarded-Proto` — owns R-X0MQ-MNXN, R-X1UN-0FOC, R-X32J-E7F1, R-X4AF-RZ5Q

## Verification ids → Decision

- R-4199-A0U9 → D10 → `project/design/D10.md`
- R-42H5-NSKY → D10 → `project/design/D10.md`
- R-44WY-FC2C → D10 → `project/design/D10.md`
- R-464U-T3T1 → D10 → `project/design/D10.md`
- R-47CR-6VJQ → D10 → `project/design/D10.md`
- R-48KN-KNAF → D10 → `project/design/D10.md`
- R-49SJ-YF14 → D10 → `project/design/D10.md`
- R-7EK6-8030 → D9 → `project/design/D09.md`
- R-7FS2-LRTP → D9 → `project/design/D09.md`
- R-7GZY-ZJKE → D9 → `project/design/D09.md`
- R-7I7V-DBB3 → D9 → `project/design/D09.md`
- R-7JFR-R31S → D11 → `project/design/D11.md`
- R-7LVK-IMJ6 → D11 → `project/design/D11.md`
- R-LWOU-OWWQ → D5 → `project/design/D05.md`
- R-LXWR-2ONF → D5 → `project/design/D05.md`
- R-LZ4N-GGE4 → D5 → `project/design/D05.md`
- R-M0CJ-U84T → D6 → `project/design/D06.md`
- R-M1KG-7ZVI → D6 → `project/design/D06.md`
- R-M2SC-LRM7 → D6 → `project/design/D06.md`
- R-M408-ZJCW → D6 → `project/design/D06.md`
- R-M585-DB3L → D6 → `project/design/D06.md`
- R-M7NY-4UKZ → D7 → `project/design/D07.md`
- R-M8VU-IMBO → D7 → `project/design/D07.md`
- R-MA3Q-WE2D → D7 → `project/design/D07.md`
- R-MBBN-A5T2 → D7 → `project/design/D07.md`
- R-MCJJ-NXJR → D8 → `project/design/D08.md`
- R-MDRG-1PAG → D8 → `project/design/D08.md`
- R-MEZC-FH15 → D8 → `project/design/D08.md`
- R-MG78-T8RU → D8 → `project/design/D08.md`
- R-MHF5-70IJ → D8 → `project/design/D08.md`
- R-MIN1-KS98 → D8 → `project/design/D08.md`
- R-MJUX-YJZX → D8 → `project/design/D08.md`
- R-ML2U-CBQM → D9 → `project/design/D09.md`
- R-WPNN-6Q9E → D8 → `project/design/D08.md`
- R-WQVJ-KI03 → D8 → `project/design/D08.md`
- R-WTBC-C1HH → D8 → `project/design/D08.md`
- R-WUJ8-PT86 → D8 → `project/design/D08.md`
- R-WVR5-3KYV → D8 → `project/design/D08.md`
- R-WWZ1-HCPK → D9 → `project/design/D09.md`
- R-WY6X-V4G9 → D9 → `project/design/D09.md`
- R-WZEU-8W6Y → D9 → `project/design/D09.md`
- R-X0MQ-MNXN → D12 → `project/design/D12.md`
- R-X1UN-0FOC → D12 → `project/design/D12.md`
- R-X32J-E7F1 → D12 → `project/design/D12.md`
- R-X4AF-RZ5Q → D12 → `project/design/D12.md`
- R-YO06-9I18 → D1 → `project/design/D01.md`
- R-YP82-N9RX → D1 → `project/design/D01.md`
- R-YQFZ-11IM → D2 → `project/design/D02.md`
- R-YRNV-ET9B → D3 → `project/design/D03.md`
- R-YSVR-SL00 → D4 → `project/design/D04.md`
- R-YU3O-6CQP → D4 → `project/design/D04.md`
