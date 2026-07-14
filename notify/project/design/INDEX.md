# notify — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-3C8K, R-LAND-5D1M, R-LAND-7E4N, R-LAND-9F6P
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-4G2Q, R-ROUT-6H5R, R-ROUT-8J7S
- D3 → `project/design/D03.md` — notify's own Carbon design assets (shipped in `share/www/static`) — owns R-ASST-3K9T, R-ASST-5L2V, R-ASST-7M4W
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/notify/` location — owns R-NGNX-3N6X, R-NGNX-5P8Y, R-NGNX-7Q1Z, R-NGNX-9R3B
- D5 → `project/design/D05.md` — Docs state current truth: purge the stale "no UI" line — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-5N7S
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT (relative stylesheet link + `font-display: optional` + self-served `src` + `<head>` preload + session-gated nginx `/srv/notify/static/`) — owns R-8JS0-IQDX, R-8KZW-WI4M, R-8M7T-A9VB, R-8NFP-O1M0, R-8ONM-1TCP
- D9 → `project/design/D09.md` — Adopt `registry`: resolve notify's own loopback port by name at the composition root (own port via `MustPort`; peer feed addresses are chassis-resolved per D11) — owns R-RGSP-4A1K
- D10 → `project/design/D10.md` — Prove no loopback-port literal survives (source-scan guard) and guard the deploy artifacts (`etc/manifest.env`, `etc/nginx.conf`) against registry drift via registry-derived test expectations — owns R-RGNL-4E5P, R-RGDR-4F6Q
- D11 → `project/design/D11.md` — Consumer loops through `Spec.Consumers` (chassis-owned): the two-upstream declaration, hand-rolled worker deletion, env-name migration to `NOTIFY_<SRC>_FEED_URL`/`NOTIFY_<SRC>_FROM` — owns R-4DG9-3Q97, R-4EO5-HHZW
- D12 → `project/design/D12.md` — Web surface from `share/www` through the chassis (de-embed; `Spec.WWW`, delete `internal/web`) — owns R-4FW1-V9QL, R-4H3Y-91HA
- D13 → `project/design/D13.md` — MCP surface over `appkit/mcp`: `internal/mcp` becomes the `send` tool table — owns R-4IBU-MT7Z
- D14 → `project/design/D14.md` — Delete the chassis shims (`internal/db` wrappers) and true up the doctrine doc — none (structural)
- D15 → `project/design/D15.md` — The session-gated locations opt into the apex `@login_bounce`: a logged-out human navigation goes to sign-in, not a bare 401 (bearer tier deliberately excluded) — owns R-3IZH-DPMO, R-3K7D-RHDD, R-3LFA-5942
- D16 → `project/design/D16.md` — Event-routing conformance (consumer side): canonical-key subscription filters, kind/subject handler matching — owns R-ZCGU-FG9L, R-ZEWN-6ZQZ, R-ZG4J-KRHO, R-ZHCF-YJ8D
- D17 → `project/design/D17.md` — Structured MCP adoption: `send` returns `structuredContent`, declares an `outputSchema`, and carries typed error codes (`validation`, `source_unavailable`) from the closed vocabulary — owns R-A918-YY6H, R-AA95-CPX6, R-ACOY-49EK, R-ADWU-I159

## Verification ids → Decision

- R-3IZH-DPMO → D15 → `project/design/D15.md`
- R-3K7D-RHDD → D15 → `project/design/D15.md`
- R-3LFA-5942 → D15 → `project/design/D15.md`
- R-4DG9-3Q97 → D11 → `project/design/D11.md`
- R-4EO5-HHZW → D11 → `project/design/D11.md`
- R-4FW1-V9QL → D12 → `project/design/D12.md`
- R-4H3Y-91HA → D12 → `project/design/D12.md`
- R-4IBU-MT7Z → D13 → `project/design/D13.md`
- R-8JS0-IQDX → D8 → `project/design/D08.md`
- R-8KZW-WI4M → D8 → `project/design/D08.md`
- R-8M7T-A9VB → D8 → `project/design/D08.md`
- R-8NFP-O1M0 → D8 → `project/design/D08.md`
- R-8ONM-1TCP → D8 → `project/design/D08.md`
- R-A918-YY6H → D17 → `project/design/D17.md`
- R-AA95-CPX6 → D17 → `project/design/D17.md`
- R-ACOY-49EK → D17 → `project/design/D17.md`
- R-ADWU-I159 → D17 → `project/design/D17.md`
- R-ASST-3K9T → D3 → `project/design/D03.md`
- R-ASST-5L2V → D3 → `project/design/D03.md`
- R-ASST-7M4W → D3 → `project/design/D03.md`
- R-HOME-5N7S → D7 → `project/design/D07.md`
- R-LAND-3C8K → D1 → `project/design/D01.md`
- R-LAND-5D1M → D1 → `project/design/D01.md`
- R-LAND-7E4N → D1 → `project/design/D01.md`
- R-LAND-9F6P → D1 → `project/design/D01.md`
- R-NGNX-3N6X → D4 → `project/design/D04.md`
- R-NGNX-5P8Y → D4 → `project/design/D04.md`
- R-NGNX-7Q1Z → D4 → `project/design/D04.md`
- R-NGNX-9R3B → D4 → `project/design/D04.md`
- R-RGDR-4F6Q → D10 → `project/design/D10.md`
- R-RGNL-4E5P → D10 → `project/design/D10.md`
- R-RGSP-4A1K → D9 → `project/design/D09.md`
- R-ROUT-4G2Q → D2 → `project/design/D02.md`
- R-ROUT-6H5R → D2 → `project/design/D02.md`
- R-ROUT-8J7S → D2 → `project/design/D02.md`
- R-ZCGU-FG9L → D16 → `project/design/D16.md`
- R-ZEWN-6ZQZ → D16 → `project/design/D16.md`
- R-ZG4J-KRHO → D16 → `project/design/D16.md`
- R-ZHCF-YJ8D → D16 → `project/design/D16.md`
