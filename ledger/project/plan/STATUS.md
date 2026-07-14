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
Phase 12 ✅ realizes D14 — `external_ref` opt-in idempotency: new timestamped migration (nullable column + partial unique index `WHERE external_ref IS NOT NULL`), optional param on `record`/`reverse`, `duplicate_ref` typed error naming the existing transaction, event-payload field, describe convention note; covers R-FP14-UYWQ, R-FQ91-8QNF, R-FRGX-MIE4, R-FSOU-0A4T, R-FTWQ-E1VI, R-FV4M-RTM7, R-FWCJ-5LCW
Phase 13 ✅ realizes D15 — event-routing conformance (⛔ externally ordered on eventplane plan phases 01–04 + appkit): kind `recorded`, empty subject (`ledger:recorded`), family registry, new timestamped outbox migration per the revised `outbox.SchemaSQL` with the drift guard re-pointed and `003_outbox.sql` frozen; covers R-FXKF-JD3L, R-FYSB-X4UA, R-G184-OOBO, R-G2G1-2G2D
Phase 14 ⬜ realizes D16 — structured MCP adoption (⛔ externally ordered on appkit phases 12–14): swap `JSONResult`→`StructuredResult` + per-verb `outputSchema` on the six domain result verbs, re-sign `translateLedgerError` to the closed error vocabulary (`validation`/`not_found`/`conflict`/`internal`) deleting `jsonEscape` and the hand-built envelopes, `describe` kept a prose exception; covers R-9FRN-SGDT, R-9GZK-684I, R-9I7G-JZV7, R-9JFC-XRLW, R-9KN9-BJCL, R-9LV5-PB3A, R-9N32-32TZ, R-9OAY-GUKO, R-9PIU-UMBD, R-9QQR-8E22, R-9RYN-M5SR, R-9T6J-ZXJG, R-9UEG-DPA5
