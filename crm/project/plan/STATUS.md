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
