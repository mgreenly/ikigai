# scripts — Design Index

Each Decision maps to its `project/design/DNN.md`; every `R-XXXX-XXXX` id maps to its Decision/file. Resolve an id by grepping this index (or the Decision files directly). Regenerate this manifest whenever a Decision is added or its Verification ids change.

## Decisions

- D1 → `project/design/D01.md` — The landing handler and its v1 content (service name + version) — owns R-LAND-7Q3D, R-LAND-9R5F, R-LAND-1S7G, R-LAND-3T9H
- D2 → `project/design/D02.md` — Route wiring: `GET /{$}` mounted ungated through `Spec.Handlers` — owns R-ROUT-8U2J, R-ROUT-1V4K, R-ROUT-3W6L
- D3 → `project/design/D03.md` — scripts's own Carbon design assets (shipped in `share/www/static`) — owns R-ASST-5X8M, R-ASST-7Y1N, R-ASST-9Z3P
- D4 → `project/design/D04.md` — nginx fragment: the exact-match session-gated `= /srv/scripts/` location — owns R-NGNX-2A5Q, R-NGNX-4B7R, R-NGNX-6C9S, R-NGNX-8D1T
- D5 → `project/design/D05.md` — Docs state current truth: state the landing-page surface in scripts's doctrine — none (structural; docs-only)
- D6 → `project/design/D06.md` — Conform the landing page to the cron canonical template — none (structural; markup-only)
- D7 → `project/design/D07.md` — A top-left Home link to the dashboard landing page — owns R-HOME-8R2V
- D8 → `project/design/D08.md` — Self-serve the landing page's fonts and eliminate the FOUT (relative stylesheet link + `font-display: optional` + self-served `src` + `<head>` preload + session-gated nginx `/srv/scripts/static/`) — owns R-M59W-5CAW, R-M6HS-J41L, R-M8XL-ANIZ, R-MA5H-OF9O, R-MBDE-270D
- D9 → `project/design/D09.md` — Runs live under the service-owned `cache/` dir, not the root-owned AppDir (`scriptsRuntimeRoot` returns `filepath.Dir(cfg.GenerationPath)` in every layout; fixes the on-box boot crash-loop) — owns R-RUNS-CDIR, R-RUNS-BOOT
- D10 → `project/design/D10.md` — Adopt `registry`: resolve scripts' own port and peer addresses by name at startup (own port via `MustPort`, dropbox base via `BaseURL`, `go.mod` require/replace, guardrail test that no `30xx` literal remains; peer feed defaults handed to the chassis by D11) — owns R-RGST-SELF, R-RGST-DBOX, R-RGST-NLIT, R-RGST-GMOD
- D11 → `project/design/D11.md` — Consumer loops through `Spec.Consumers` (chassis-owned) + composition-root normalization (delete `runConsumer`/`Workers`/the `var rt` capture/the legacy `Consumes`+`Subscriptions` fields; one fully-formed Spec literal) — owns R-8WN1-0VQI, R-8XUX-ENH7
- D12 → `project/design/D12.md` — Web surface from `share/www` through the chassis (de-embed; `Spec.WWW`, delete `internal/web`) — owns R-8Z2T-SF7W, R-90AQ-66YL
- D13 → `project/design/D13.md` — MCP surface over `appkit/mcp`: `internal/mcp` becomes the sixteen-tool domain table; chassis `health`+`reflection` added; runtime contract moves to `Spec.Health` — owns R-91IM-JYPA, R-92QI-XQFZ
- D14 → `project/design/D14.md` — Delete the `internal/db` open/migrate shim and true up the doctrine — none (structural)
- D15 → `project/design/D15.md` — The session-gated locations opt into the apex `@login_bounce`: a logged-out human navigation goes to sign-in, not a bare 401 (bearer tier deliberately excluded) — owns R-465K-NCPV, R-47DH-14GK, R-49T9-SNXY
- D16 → `project/design/D16.md` — Rename the bearer tier's prompts-named identity plumbing (`$prompts_owner`/`$prompts_client`/`@prompts_authn_500`) to scripts-owned names — owns R-4EOV-BQWQ, R-4FWR-PINF

## Verification ids → Decision

- R-465K-NCPV → D15 → `project/design/D15.md`
- R-47DH-14GK → D15 → `project/design/D15.md`
- R-49T9-SNXY → D15 → `project/design/D15.md`
- R-4EOV-BQWQ → D16 → `project/design/D16.md`
- R-4FWR-PINF → D16 → `project/design/D16.md`
- R-8WN1-0VQI → D11 → `project/design/D11.md`
- R-8XUX-ENH7 → D11 → `project/design/D11.md`
- R-8Z2T-SF7W → D12 → `project/design/D12.md`
- R-90AQ-66YL → D12 → `project/design/D12.md`
- R-91IM-JYPA → D13 → `project/design/D13.md`
- R-92QI-XQFZ → D13 → `project/design/D13.md`
- R-ASST-5X8M → D3 → `project/design/D03.md`
- R-ASST-7Y1N → D3 → `project/design/D03.md`
- R-ASST-9Z3P → D3 → `project/design/D03.md`
- R-HOME-8R2V → D7 → `project/design/D07.md`
- R-LAND-1S7G → D1 → `project/design/D01.md`
- R-LAND-3T9H → D1 → `project/design/D01.md`
- R-LAND-7Q3D → D1 → `project/design/D01.md`
- R-LAND-9R5F → D1 → `project/design/D01.md`
- R-M59W-5CAW → D8 → `project/design/D08.md`
- R-M6HS-J41L → D8 → `project/design/D08.md`
- R-M8XL-ANIZ → D8 → `project/design/D08.md`
- R-MA5H-OF9O → D8 → `project/design/D08.md`
- R-MBDE-270D → D8 → `project/design/D08.md`
- R-NGNX-2A5Q → D4 → `project/design/D04.md`
- R-NGNX-4B7R → D4 → `project/design/D04.md`
- R-NGNX-6C9S → D4 → `project/design/D04.md`
- R-NGNX-8D1T → D4 → `project/design/D04.md`
- R-RGST-DBOX → D10 → `project/design/D10.md`
- R-RGST-GMOD → D10 → `project/design/D10.md`
- R-RGST-NLIT → D10 → `project/design/D10.md`
- R-RGST-SELF → D10 → `project/design/D10.md`
- R-ROUT-1V4K → D2 → `project/design/D02.md`
- R-ROUT-3W6L → D2 → `project/design/D02.md`
- R-ROUT-8U2J → D2 → `project/design/D02.md`
- R-RUNS-BOOT → D9 → `project/design/D09.md`
- R-RUNS-CDIR → D9 → `project/design/D09.md`

_Retired: R-RGST-PEER (was D10) — the peer feed-URL default resolution it pinned became chassis-owned when D11 moved the consumer loops to `Spec.Consumers`; the behavior is pinned by appkit's `R-464U-T3T1`/`R-47CR-6VJQ`._
