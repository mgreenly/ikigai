# crm — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/crm/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/crm/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `crm/AGENTS.md`/`CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-3L5Q
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/crm/static/`; covers R-SRS9-B2RI, R-ST05-OUI7, R-SU82-2M8W, R-SVFY-GDZL, R-SWNU-U5QA
Phase 07 ✅ realizes D9, D10, D11 — self-describing MCP discovery surface in `internal/mcp`: rewrite the `initialize` instructions (vocabulary + verb-flow + guide pointer), slim `saveDescription` (relocate the per-type field catalog), and add the flat read-only `guide` tool over an embedded `guide.md`; update the tools/list count to eight; covers R-PDZ7-HTAN, R-PF73-VL1C, R-PGF0-9CS1, R-PIUT-0W9F, R-PK2P-EO04, R-PLAL-SFQT, R-PMII-67HI
Phase 08 ✅ realizes D12 — serve the web surface from `share/www` through the chassis: move landing.html + static/ to `crm/share/www/`, set `Spec.WWW`, render `GET /{$}` via `rt.WWW()`, delete `internal/web`, relocate tests to `cmd/crm` over the real tree, add the `bin/start` `CRM_WWW_PATH` dev export; covers R-MTM5-0PXH, R-MUU1-EHO6 (D1/D2/D3/D8 Go-side ids re-covered on the new substrate)
Phase 09 ✅ realizes D13 — MCP surface over `appkit/mcp`: `internal/mcp` reshaped to `Instructions` + `Tools(svc)` + `NewHandler`, local transport/health/reflection/envelope code deleted, `tools_test.go` rewired to the assembled handler; covers R-MW1X-S9EV (D9/D10/D11 ids re-covered through the new seam)
Phase 10 ✅ realizes D14 — delete the chassis shims: `internal/ids` removed in favor of `appkit/logging.NewULID`; `internal/db` reduced to the embedded migration set + outbox guard (structural; scoped greps + green suite)
Phase 11 ✅ realizes D15 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod and resolve crm's own port via `registry.MustPort("crm")` at the composition root (crm is a producer — no peer-feed defaults to convert); covers R-X04D-MBGE
Phase 12 ✅ realizes D16 — prove no `127.0.0.1:30xx` literal remains in crm's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` (`MustPort`/`BaseURL`) so a renumber fails a crm test; covers R-X1CA-0373, R-X2K6-DUXS
Phase 13 ✅ realizes D17 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `crm/etc/nginx.conf` (`= /srv/crm/`, `/srv/crm/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/crm/main_test.go` nginx content assertions; covers R-3BO3-336I, R-3CVZ-GUX7, R-3E3V-UMNW
Phase 14 ✅ realizes D18 — event-routing conformance: keep the four `contact.*` kind names, subject `/<contact id>` on every event, `crm.Events` → four `outbox.Family` entries, one new timestamped `outbox_routing` migration re-creating the outbox per the revised `outbox.SchemaSQL` with the drift guard re-pointed (⛔ requires the eventplane revision built first — operator-sequenced); covers R-8HHB-24SG, R-8IP7-FWJ5, R-8JX3-TO9U, R-8L50-7G0J
Phase 15 ⬜ realizes D19 — structured MCP adoption in `internal/mcp`: swap the five domain results `JSONResult`→`StructuredResult` (propagating the new marshal error), declare an `outputSchema` per domain verb (`guide` none), and map crm's domain errors onto appkit's closed-vocabulary `ErrorResult(code,msg)` (DuplicateError→conflict, retiring the nested `{"error":…}` envelope) — no guard swap (crm has no loopback guard site); covers R-5Y60-E30A, R-5ZDW-RUQZ, R-60LT-5MHO, R-61TP-JE8D, R-631L-X5Z2, R-65HE-OPGG
