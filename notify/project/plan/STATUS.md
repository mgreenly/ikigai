# notify — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/notify/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/notify/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `notify/AGENTS.md`/`CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-5N7S
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/notify/static/`; covers R-8JS0-IQDX, R-8KZW-WI4M, R-8M7T-A9VB, R-8NFP-O1M0, R-8ONM-1TCP
Phase 07 ✅ realizes D9 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod; resolve notify's own port via `registry.MustPort("notify")` and the crm/prompts feed defaults via `registry.BaseURL(...) + "/feed"` at the composition root, env overrides unchanged; covers R-RGSP-4A1K, R-RGCF-4B2L, R-RGPF-4C3M, R-RGEO-4D4N
Phase 08 ⬜ realizes D10 — prove no `127.0.0.1:30xx` literal remains in notify's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` so a renumber fails a notify test; covers R-RGNL-4E5P, R-RGDR-4F6Q
