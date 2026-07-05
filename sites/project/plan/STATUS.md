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
Phase 07 ✅ realizes D9 — adopt the shared `registry`: add `require registry` + committed `replace registry => ../registry` to `go.mod`, and in `cmd/sites/main.go` swap the own-port literal `Port: 3004`→`registry.MustPort("sites")` and the dropbox default `"http://127.0.0.1:3200"`→`registry.BaseURL("dropbox")` (resolved by name at the composition root; behavior-preserving; nginx fragment literal deliberately excluded); covers R-7K2P-QN4D, R-7L9F-XW3H, R-7M4C-BV8J, R-7N6R-TZ2Q
Phase 08 ✅ realizes D10 — build the native `internal/files` package: confined filesystem ops as plain Go over Go types (symlink-resolving `ConfinePath` + `ErrEscapes` sentinel, `Read`/`Write`/`Edit`/`Glob`/`Grep`/`List`/`Mkdir`), ported from `prompts`' worker bodies, no agentkit/JSON/agent framing, standalone unit-tested; covers R-027Y-BQ1I, R-03FU-PHS7, R-04NR-39IW, R-05VN-H19L, R-073J-UT0A, R-08BG-8KQZ, R-09JC-MCHO, R-0AR9-048D, R-0D71-RNPR, R-0EEY-5FGG
Phase 09 ✅ realizes D11 — rewire the MCP file tools onto `internal/files` and drop `agentkit`: delete the `agentkit/wire`+`tools` bridge in `internal/mcp/files.go`, hand-write the four file-tool schemas, cleaner structured results, unify confinement on `files.ConfinePath` (sentinel→`path_escapes_working_dir`), point `sync` at `files.Write`, and remove `require`/`replace agentkit` from `go.mod` (surface-preserving; orphaned repo-root `agentkit/` left on disk); covers R-0FMU-J775, R-0GUQ-WYXU, R-0I2N-AQOJ, R-0JAJ-OIF8, R-0KIG-2A5X
Phase 10 ⬜ realizes D10 — fix `internal/files.Glob` recursive `**` matching: walk the confined search base and match each base-relative path one `/`-separated segment at a time (`*`/`?`/`[…]` never cross `/`; `**` spans any run of segments including zero) instead of delegating to `filepath.Glob`, so `**/*.css` finds `.css` at the base and any depth while single-segment `*` stays non-recursive; base-relative/sorted/`[]string{}`/`ErrEscapes` guarantees and the three existing Glob tests preserved; covers R-3ZP8-T0GP, R-40X5-6S7E
