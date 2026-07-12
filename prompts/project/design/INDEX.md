# prompts agentkit migration — Design Index

Each Decision maps to its `DNN.md` file. Every `R-XXXX-XXXX` id maps to its Decision and file. Regenerate this index when a Decision is added or its Verification ids change. To resolve an id, grep this file or the Decision files directly.

## Decisions

| Decision | File | Title | Verification ids |
|----------|------|-------|-----------------|
| D1 | project/design/D01.md | Module dependency | none — structural |
| D2 | project/design/D02.md | Config struct | R-JTBA-4RDB, R-JUJ6-IJ40 |
| D3 | project/design/D03.md | Validation | R-JVR2-WAUP, R-JWYZ-A2LE, R-JY6V-NUC3, R-JZES-1M2S |
| D4 | project/design/D04.md | Provider factory | none — structural |
| D5 | project/design/D05.md | Built-in sandbox tools | R-64QY-QN1H, R-K1UK-T5K6 |
| D6 | project/design/D06.md | Suite discovery | R-K32H-6XAV, R-K4AD-KP1K, R-9JNO-RZM2, R-9KVL-5RCR, R-9M3H-JJ3G |
| D7 | project/design/D07.md | Runner | R-K5I9-YGS9, R-K6Q6-C8IY, R-K7Y2-Q09N, R-K95Z-3S0C |
| D8 | project/design/D08.md | DB migration | R-KBLR-VBHQ, R-KCTO-938F |
| D9 | project/design/D09.md | MCP schema | R-KE1K-MUZ4, R-KF9H-0MPT |
| D10 | project/design/D10.md | The landing page: a session-gated human web surface (`GET /{$}`) | R-LAND-PG01, R-LAND-NMVR, R-LAND-CARB, R-LAND-ROOT, R-LAND-UNGT |
| D11 | project/design/D11.md | Conform the landing page to the cron canonical template | none — structural |
| D12 | project/design/D12.md | A top-left Home link to the dashboard landing page | R-HOME-2T4X |
| D13 | project/design/D13.md | Self-serve the landing page's fonts and eliminate the FOUT | R-DFKP-IVZU, R-DGSL-WNQJ, R-DI0I-AFH8, R-DJ8E-O77X, R-DKGB-1YYM |
| D14 | project/design/D14.md | Adopt the shared `registry` for all loopback addressing | R-RG01-PORT, R-RG03-DBOX, R-RG04-NLIT |
| D15 | project/design/D15.md | Consumer loops through `Spec.Consumers` (chassis-owned) | R-DFV4-7W4Y, R-DH30-LNVN |
| D16 | project/design/D16.md | Web surface from `share/www` through the chassis (de-embed) | R-DIAW-ZFMC, R-DJIT-D7D1 |
| D17 | project/design/D17.md | MCP surface over `appkit/mcp`: `internal/mcp` becomes the tool table | R-DKQP-QZ3Q, R-DLYM-4QUF |
| D18 | project/design/D18.md | Delete the chassis shims (`internal/db` wrappers) and true up the doctrine doc | none — structural |
| D19 | project/design/D19.md | Progressive suite-tool discovery (deferred suite tools) | R-9NBD-XAU5, R-9OJA-B2KU, R-9PR6-OUBJ, R-A69O-ATWI |
| D20 | project/design/D20.md | Session-gated locations opt into the apex `@login_bounce` (bearer tier excluded) | R-3RIS-23TJ, R-3SQO-FVK8, R-3TYK-TNAX |
| D21 | project/design/D21.md | Content-plane acceptor: the `Fetch` sandbox tool | R-65YV-4ES6, R-676R-I6IV, R-68EN-VY9K, R-69MK-9Q09, R-6AUG-NHQY |
| D22 | project/design/D22.md | Content-plane holder: run sandbox files at `GET /run-content` | R-6C2D-19HN, R-6DA9-F18C, R-6EI5-SSZ1, R-6FQ2-6KPQ |
| D23 | project/design/D23.md | Box PDF tooling in the framing prompt; model-native PDF is a non-goal | R-6I5U-Y474 |
| D24 | project/design/D24.md | Event-routing conformance: triggers become canonical filter strings | R-6JDR-BVXT, R-6KLN-PNOI, R-6LTK-3FF7, R-6N1G-H75W, R-6O9C-UYWL, R-6PH9-8QNA, R-6QP5-MIDZ, R-6RX2-0A4O |
| D25 | project/design/D25.md | Event-routing conformance: producer kinds `run.succeeded`/`run.failed`, subject = /<prompt name> | R-6T4Y-E1VD, R-6UCU-RTM2, R-6VKR-5LCR, R-ZS8A-TVOF |

## Verification ids → Decision

| id | Decision | File |
|----|----------|------|
| R-3RIS-23TJ | D20 | project/design/D20.md |
| R-3SQO-FVK8 | D20 | project/design/D20.md |
| R-3TYK-TNAX | D20 | project/design/D20.md |
| R-64QY-QN1H | D5 | project/design/D05.md |
| R-65YV-4ES6 | D21 | project/design/D21.md |
| R-676R-I6IV | D21 | project/design/D21.md |
| R-68EN-VY9K | D21 | project/design/D21.md |
| R-69MK-9Q09 | D21 | project/design/D21.md |
| R-6AUG-NHQY | D21 | project/design/D21.md |
| R-6C2D-19HN | D22 | project/design/D22.md |
| R-6DA9-F18C | D22 | project/design/D22.md |
| R-6EI5-SSZ1 | D22 | project/design/D22.md |
| R-6FQ2-6KPQ | D22 | project/design/D22.md |
| R-6I5U-Y474 | D23 | project/design/D23.md |
| R-6JDR-BVXT | D24 | project/design/D24.md |
| R-6KLN-PNOI | D24 | project/design/D24.md |
| R-6LTK-3FF7 | D24 | project/design/D24.md |
| R-6N1G-H75W | D24 | project/design/D24.md |
| R-6O9C-UYWL | D24 | project/design/D24.md |
| R-6PH9-8QNA | D24 | project/design/D24.md |
| R-6QP5-MIDZ | D24 | project/design/D24.md |
| R-6RX2-0A4O | D24 | project/design/D24.md |
| R-6T4Y-E1VD | D25 | project/design/D25.md |
| R-6UCU-RTM2 | D25 | project/design/D25.md |
| R-6VKR-5LCR | D25 | project/design/D25.md |
| R-9JNO-RZM2 | D6 | project/design/D06.md |
| R-9KVL-5RCR | D6 | project/design/D06.md |
| R-9M3H-JJ3G | D6 | project/design/D06.md |
| R-9NBD-XAU5 | D19 | project/design/D19.md |
| R-9OJA-B2KU | D19 | project/design/D19.md |
| R-9PR6-OUBJ | D19 | project/design/D19.md |
| R-A69O-ATWI | D19 | project/design/D19.md |
| R-DFKP-IVZU | D13 | project/design/D13.md |
| R-DFV4-7W4Y | D15 | project/design/D15.md |
| R-DGSL-WNQJ | D13 | project/design/D13.md |
| R-DH30-LNVN | D15 | project/design/D15.md |
| R-DI0I-AFH8 | D13 | project/design/D13.md |
| R-DIAW-ZFMC | D16 | project/design/D16.md |
| R-DJ8E-O77X | D13 | project/design/D13.md |
| R-DJIT-D7D1 | D16 | project/design/D16.md |
| R-DKGB-1YYM | D13 | project/design/D13.md |
| R-DKQP-QZ3Q | D17 | project/design/D17.md |
| R-DLYM-4QUF | D17 | project/design/D17.md |
| R-HOME-2T4X | D12 | project/design/D12.md |
| R-JTBA-4RDB | D2 | project/design/D02.md |
| R-JUJ6-IJ40 | D2 | project/design/D02.md |
| R-JVR2-WAUP | D3 | project/design/D03.md |
| R-JWYZ-A2LE | D3 | project/design/D03.md |
| R-JY6V-NUC3 | D3 | project/design/D03.md |
| R-JZES-1M2S | D3 | project/design/D03.md |
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
| R-RG01-PORT | D14 | project/design/D14.md |
| R-RG03-DBOX | D14 | project/design/D14.md |
| R-RG04-NLIT | D14 | project/design/D14.md |
| R-ZS8A-TVOF | D25 | project/design/D25.md |
