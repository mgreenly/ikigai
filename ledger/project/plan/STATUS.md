# ledger — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/ledger/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/ledger/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `ledger/AGENTS.md`/`CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-4M6R
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/ledger/static/`; covers R-7AW0-4QF8, R-7DBS-W9WM, R-7EJP-A1NB, R-7FRL-NTE0, R-7GZI-1L4P
Phase 07 ✅ realizes D9 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod, resolve ledger's own port via `registry.MustPort("ledger")`, re-point the manifest/nginx tests at `registry`, and add the source-scan guard; covers R-4VDW-DRQH, R-4WLS-RJH6, R-4XTP-5B7V, R-4Z1L-J2YK
Phase 08 ✅ realizes D10 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/ledger` over the shipped tree), boundary-crossing `LEDGER_WWW_PATH` export in `bin/start`; covers R-509H-WUP9, R-51HE-AMFY (retained D1/D2/D3/D4/D7/D8/D9 ids re-proven on the new substrate)
Phase 09 ✅ realizes D11 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(svc)` (the seven domain verbs) + `NewHandler`, local transport and local health/reflection deleted, surface stays exactly nine tools; covers R-52PA-OE6N
Phase 10 ✅ realizes D12 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + load/outbox guards remain) and true up `ledger/AGENTS.md` to the converted service (structural)
Phase 11 ✅ realizes D13 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `ledger/etc/nginx.conf` (`= /srv/ledger/`, `/srv/ledger/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/ledger/main_test.go` nginx content assertions; covers R-3FBS-8EEL, R-3GJO-M65A, R-3HRK-ZXVZ
