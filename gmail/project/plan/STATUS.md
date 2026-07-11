# gmail — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/gmail/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/gmail/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `gmail/AGENTS.md`/`CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-7Q9U
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/gmail/static/`; covers R-3X4A-Y8CI, R-3YC7-C037, R-40S0-3JKL, R-41ZW-HBBA
Phase 07 ✅ realizes D11 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod and resolve gmail's own port via `registry.MustPort("gmail")` in the Spec (producer — no peer feeds); covers R-9QEG-KF05
Phase 08 ✅ realizes D12 — prove no `127.0.0.1:3xxx` literal remains in gmail's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` so a renumber fails a gmail test; covers R-9RMC-Y6QU, R-9SU9-BYHJ
Phase 09 ✅ realizes D13 — composition-root normalization: move the `appkit.Spec` from `internal/gmailapp` into `cmd/gmail/main.go` as `gmailSpec()`, delete `internal/gmailapp`, repoint `main_test` (structural; pure relocation)
Phase 10 ✅ realizes D9 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/gmail`), boundary-crossing `GMAIL_WWW_PATH` export in `bin/start`; covers R-9LIV-1C1D, R-9MQR-F3S2 (retained D1/D2/D3/D4/D7/D8 ids re-proven on the new substrate)
Phase 11 ✅ realizes D10 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(client)` (the ten mailbox tools) + `NewHandler`, local transport and local health/reflection deleted, surface stays exactly twelve tools; covers R-9NYN-SVIR
Phase 12 ✅ realizes D14 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + outbox/load guards remain) and true up `gmail/AGENTS.md` to the converted service (structural)
Phase 13 ✅ realizes D15 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `gmail/etc/nginx.conf` (`= /srv/gmail/`, `/srv/gmail/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/gmail/nginx_test.go` nginx content assertions; covers R-3YU6-CQ9P, R-4022-QI0E, R-419Z-49R3
