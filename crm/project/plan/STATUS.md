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
Phase 07 ⬜ realizes D9, D10, D11 — self-describing MCP discovery surface in `internal/mcp`: rewrite the `initialize` instructions (vocabulary + verb-flow + guide pointer), slim `saveDescription` (relocate the per-type field catalog), and add the flat read-only `guide` tool over an embedded `guide.md`; update the tools/list count to eight; covers R-PDZ7-HTAN, R-PF73-VL1C, R-PGF0-9CS1, R-PIUT-0W9F, R-PK2P-EO04, R-PLAL-SFQT, R-PMII-67HI
