# gmail — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3F7K, R-LAND-5H9M, R-LAND-7J2N, R-LAND-9K4P
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-4M6Q, R-ROUT-6N8R, R-ROUT-8P1S
- D3 → `project/design/D03.md` — gmail's own Carbon design assets (shipped in `share/www/static`) — owns R-ASST-3T5V, R-ASST-5W7X, R-ASST-7Y9Z
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/gmail/` location — owns R-NGNX-3B6C, R-NGNX-5D8E, R-NGNX-7F1G, R-NGNX-9H3J
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-7Q9U
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT — owns R-3X4A-Y8CI, R-3YC7-C037, R-3ZK3-PRTW, R-40S0-3JKL, R-41ZW-HBBA
- D9 → `project/design/D09.md` — Web surface from `share/www` through the chassis (de-embed; `Spec.WWW`, delete `internal/web`) — owns R-9LIV-1C1D, R-9MQR-F3S2
- D10 → `project/design/D10.md` — MCP surface over `appkit/mcp`: `internal/mcp` becomes the tool table — owns R-9NYN-SVIR
- D11 → `project/design/D11.md` — Adopt `registry`: resolve gmail's own loopback port by name — owns R-9QEG-KF05
- D12 → `project/design/D12.md` — Prove no loopback-port literal survives, and guard the deploy artifacts against registry drift — owns R-9RMC-Y6QU, R-9SU9-BYHJ
- D13 → `project/design/D13.md` — Composition-root normalization: the `appkit.Spec` inline in `cmd/gmail/main.go` — none (structural)
- D14 → `project/design/D14.md` — Delete the chassis shims (`internal/db` wrappers) and true up the doctrine doc — none (structural)
- D15 → `project/design/D15.md` — The session-gated locations opt into the apex `@login_bounce`: a logged-out human navigation goes to sign-in, not a bare 401 (bearer tier deliberately excluded) — owns R-3YU6-CQ9P, R-4022-QI0E, R-419Z-49R3
- D16 → `project/design/D16.md` — Attachment content endpoint: loopback `GET /attachment` (content plane holder) — owns R-WVZH-M0IY, R-WX7D-ZS9N, R-WYFA-DK0C, R-WZN6-RBR1, R-X0V3-53HQ, R-X22Z-IV8F
- D17 → `project/design/D17.md` — Attachment references in `read`/`thread` results (content plane references) — owns R-X3AV-WMZ4, R-X4IS-AEPT, R-X5QO-O6GI
- D18 → `project/design/D18.md` — Event-routing conformance: kinds `received`/`sent`/`deleted`, empty subject — owns R-X6YL-1Y77, R-X86H-FPXW, R-X9ED-THOL, R-XAMA-79FA

## Verification ids → Decision

- R-3X4A-Y8CI → D8 → `project/design/D08.md`
- R-3YC7-C037 → D8 → `project/design/D08.md`
- R-3YU6-CQ9P → D15 → `project/design/D15.md`
- R-3ZK3-PRTW → D8 → `project/design/D08.md`
- R-4022-QI0E → D15 → `project/design/D15.md`
- R-40S0-3JKL → D8 → `project/design/D08.md`
- R-419Z-49R3 → D15 → `project/design/D15.md`
- R-41ZW-HBBA → D8 → `project/design/D08.md`
- R-9LIV-1C1D → D9 → `project/design/D09.md`
- R-9MQR-F3S2 → D9 → `project/design/D09.md`
- R-9NYN-SVIR → D10 → `project/design/D10.md`
- R-9QEG-KF05 → D11 → `project/design/D11.md`
- R-9RMC-Y6QU → D12 → `project/design/D12.md`
- R-9SU9-BYHJ → D12 → `project/design/D12.md`
- R-ASST-3T5V → D3 → `project/design/D03.md`
- R-ASST-5W7X → D3 → `project/design/D03.md`
- R-ASST-7Y9Z → D3 → `project/design/D03.md`
- R-HOME-7Q9U → D7 → `project/design/D07.md`
- R-LAND-3F7K → D1 → `project/design/D01.md`
- R-LAND-5H9M → D1 → `project/design/D01.md`
- R-LAND-7J2N → D1 → `project/design/D01.md`
- R-LAND-9K4P → D1 → `project/design/D01.md`
- R-NGNX-3B6C → D4 → `project/design/D04.md`
- R-NGNX-5D8E → D4 → `project/design/D04.md`
- R-NGNX-7F1G → D4 → `project/design/D04.md`
- R-NGNX-9H3J → D4 → `project/design/D04.md`
- R-ROUT-4M6Q → D2 → `project/design/D02.md`
- R-ROUT-6N8R → D2 → `project/design/D02.md`
- R-ROUT-8P1S → D2 → `project/design/D02.md`
- R-WVZH-M0IY → D16 → `project/design/D16.md`
- R-WX7D-ZS9N → D16 → `project/design/D16.md`
- R-WYFA-DK0C → D16 → `project/design/D16.md`
- R-WZN6-RBR1 → D16 → `project/design/D16.md`
- R-X0V3-53HQ → D16 → `project/design/D16.md`
- R-X22Z-IV8F → D16 → `project/design/D16.md`
- R-X3AV-WMZ4 → D17 → `project/design/D17.md`
- R-X4IS-AEPT → D17 → `project/design/D17.md`
- R-X5QO-O6GI → D17 → `project/design/D17.md`
- R-X6YL-1Y77 → D18 → `project/design/D18.md`
- R-X86H-FPXW → D18 → `project/design/D18.md`
- R-X9ED-THOL → D18 → `project/design/D18.md`
- R-XAMA-79FA → D18 → `project/design/D18.md`
