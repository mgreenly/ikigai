# cron — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/cron/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/cron/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — state the landing-page truth in `cron/cmd/cron/main.go`'s package-doc header (structural; docs-only)
Phase 04 ✅ realizes D6 — add the top-left Home link to the dashboard landing page; covers R-HOME-2K4P
Phase 05 ✅ realizes D7 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/cron/static/`; covers R-21DE-LOX3, R-22LA-ZGNS, R-23T7-D8EH, R-2513-R056, R-2690-4RVV
Phase 06 ✅ realizes D8 — normalize the composition root: move the `appkit.Spec` inline into `cmd/cron/main.go` as `cronSpec()`, delete `internal/cronapp`, re-point the manifest and composition-root-mount tests (structural; behavior-preserving)
Phase 07 ✅ realizes D9 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (landing/asset/mux/nginx tests relocate to `cmd/cron` over the shipped tree), boundary-crossing `CRON_WWW_PATH` export in `bin/start`; covers R-LPMQ-FKBR, R-LQUM-TC2G
Phase 08 ✅ realizes D10 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(store)` (create/list/get/update/delete) + `NewHandler`, local JSON-RPC transport and local health/reflection deleted, live `cron.<name>` reflection preserved via `Publishes`, surface stays exactly seven tools; covers R-LS2J-73T5
Phase 09 ✅ realizes D11 — adopt `registry`: add `require registry` + `replace registry => ../registry`, resolve cron's port via `registry.MustPort("cron")`, de-literalize the nginx-test `proxy_pass` assertions to `registry.BaseURL("cron")`, and add the source-scan + manifest/nginx drift guards; covers R-LTAF-KVJU, R-LUIB-YNAJ, R-LVQ8-CF18
Phase 10 ✅ realizes D12 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + guards remain; harnesses call `appkit/db` directly) and true up the `cmd/cron/main.go` package-doc header to the converted shape (structural)
Phase 11 ⬜ realizes D13 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `cron/etc/nginx.conf` (`= /srv/cron/`, `/srv/cron/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/cron/main_test.go` nginx content assertions; covers R-3V6H-7F1M, R-3WED-L6SB, R-3XM9-YYJ0
