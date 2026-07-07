# dropbox — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/dropbox/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/dropbox/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `dropbox/CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-6P8T
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/dropbox/static/`; covers R-LQXL-095Q, R-LS5H-E0WF, R-LTDD-RSN4, R-LULA-5KDT, R-LVT6-JC4I
Phase 07 ✅ realizes D9 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod; resolve dropbox's own port via `registry.MustPort("dropbox")` (Spec.Port + the `DROPBOX_PORT` content-base default) and the `file.*` reflection example origin via `registry.BaseURL("dropbox")` at the composition root, env overrides unchanged; covers R-QKGB-OPNE, R-QJ8F-AXWP
Phase 08 ✅ realizes D10 — prove no `127.0.0.1:30xx` literal remains in dropbox's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` so a renumber fails a dropbox test; covers R-QLO8-2HE3, R-QMW4-G94S
Phase 09 ✅ realizes D11 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/dropbox` over the shipped tree), boundary-crossing `DROPBOX_WWW_PATH` export in `bin/start`; covers R-QO40-U0VH, R-QPBX-7SM6 (retained D1/D2/D3/D4/D7/D8/D10 ids re-proven on the new substrate)
Phase 10 ⬜ realizes D12 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(svc)` (the `list`+`get` tools) + `NewHandler`, local transport and local health/reflection deleted, surface stays exactly four tools; covers R-QQJT-LKCV
Phase 11 ⬜ realizes D13 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + load/outbox guards remain) and true up `dropbox/CLAUDE.md` to the converted service (structural)
