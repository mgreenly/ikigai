# prompts agentkit migration — Design Index

Each Decision maps to its `DNN.md` file. Every `R-XXXX-XXXX` id maps to its Decision and file. Regenerate this index when a Decision is added or its Verification ids change. To resolve an id, grep this file or the Decision files directly.

## Decisions

| Decision | File | Title | Verification ids |
|----------|------|-------|-----------------|
| D1 | project/design/D01.md | Module dependency | none — structural |
| D2 | project/design/D02.md | Config struct | R-JTBA-4RDB, R-JUJ6-IJ40 |
| D3 | project/design/D03.md | Validation | R-JVR2-WAUP, R-JWYZ-A2LE, R-JY6V-NUC3, R-JZES-1M2S |
| D4 | project/design/D04.md | Provider factory | none — structural |
| D5 | project/design/D05.md | Built-in tools | R-K0MO-FDTH, R-K1UK-T5K6 |
| D6 | project/design/D06.md | Suite discovery | R-K32H-6XAV, R-K4AD-KP1K |
| D7 | project/design/D07.md | Runner | R-K5I9-YGS9, R-K6Q6-C8IY, R-K7Y2-Q09N, R-K95Z-3S0C |
| D8 | project/design/D08.md | DB migration | R-KBLR-VBHQ, R-KCTO-938F |
| D9 | project/design/D09.md | MCP schema | R-KE1K-MUZ4, R-KF9H-0MPT |
| D10 | project/design/D10.md | The landing page: a session-gated human web surface (`GET /{$}`) | R-LAND-PG01, R-LAND-NMVR, R-LAND-CARB, R-LAND-ROOT, R-LAND-UNGT |
| D11 | project/design/D11.md | Conform the landing page to the cron canonical template | none — structural |
| D12 | project/design/D12.md | A top-left Home link to the dashboard landing page | R-HOME-2T4X |
| D13 | project/design/D13.md | Self-serve the landing page's fonts and eliminate the FOUT | R-DFKP-IVZU, R-DGSL-WNQJ, R-DI0I-AFH8, R-DJ8E-O77X, R-DKGB-1YYM |
| D14 | project/design/D14.md | Adopt the shared `registry` for all loopback addressing | R-RG01-PORT, R-RG02-FEED, R-RG03-DBOX, R-RG04-NLIT |

## Verification ids → Decision

| id | Decision | File |
|----|----------|------|
| R-JTBA-4RDB | D2 | project/design/D02.md |
| R-JUJ6-IJ40 | D2 | project/design/D02.md |
| R-JVR2-WAUP | D3 | project/design/D03.md |
| R-JWYZ-A2LE | D3 | project/design/D03.md |
| R-JY6V-NUC3 | D3 | project/design/D03.md |
| R-JZES-1M2S | D3 | project/design/D03.md |
| R-K0MO-FDTH | D5 | project/design/D05.md |
| R-K1UK-T5K6 | D5 | project/design/D05.md |
| R-K32H-6XAV | D6 | project/design/D06.md |
| R-K4AD-KP1K | D6 | project/design/D06.md |
| R-K5I9-YGS9 | D7 | project/design/D07.md |
| R-K6Q6-C8IY | D7 | project/design/D07.md |
| R-K7Y2-Q09N | D7 | project/design/D07.md |
| R-K95Z-3S0C | D7 | project/design/D07.md |
| R-KBLR-VBHQ | D8 | project/design/D08.md |
| R-KCTO-938F | D8 | project/design/D08.md |
| R-KE1K-MUZ4 | D9 | project/design/D09.md |
| R-KF9H-0MPT | D9 | project/design/D09.md |
| R-LAND-CARB | D10 | project/design/D10.md |
| R-LAND-NMVR | D10 | project/design/D10.md |
| R-LAND-PG01 | D10 | project/design/D10.md |
| R-LAND-ROOT | D10 | project/design/D10.md |
| R-LAND-UNGT | D10 | project/design/D10.md |
| R-HOME-2T4X | D12 | project/design/D12.md |
| R-DFKP-IVZU | D13 | project/design/D13.md |
| R-DGSL-WNQJ | D13 | project/design/D13.md |
| R-DI0I-AFH8 | D13 | project/design/D13.md |
| R-DJ8E-O77X | D13 | project/design/D13.md |
| R-DKGB-1YYM | D13 | project/design/D13.md |
| R-RG01-PORT | D14 | project/design/D14.md |
| R-RG02-FEED | D14 | project/design/D14.md |
| R-RG03-DBOX | D14 | project/design/D14.md |
| R-RG04-NLIT | D14 | project/design/D14.md |
