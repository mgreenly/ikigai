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
