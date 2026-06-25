# cron — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/cron/main.go`
Phase 02 ⬜ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/cron/` location, validated by a content-assertion test
Phase 03 ⬜ realizes D5 — state the landing-page truth in `cron/cmd/cron/main.go`'s package-doc header (structural; docs-only)
