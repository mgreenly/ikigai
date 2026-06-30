# sites — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/sites/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/sites/` landing root beside the existing static tiers, validated by a content-assertion test
Phase 03 ✅ realizes D5 — state the standardized landing card in `cmd/sites/main.go`'s package doc comment (structural; docs-only — sites has no "no UI" claim to purge)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-9S3W
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/sites/static/` (proxied to upstream `/static/`); MUST run after cron's FOUT phase (byte-pinned to cron canonical); covers R-629P-84O5, R-63HL-LWEU, R-64PH-ZO5J, R-65XE-DFW8, R-675A-R7MX
