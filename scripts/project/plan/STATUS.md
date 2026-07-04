# scripts — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/scripts/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/scripts/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — state the landing-page surface in scripts's doctrine (`project/notes/README.md` + `ARCHITECTURE.md`) (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-8R2V
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/scripts/static/`; covers R-M59W-5CAW, R-M6HS-J41L, R-M8XL-ANIZ, R-MA5H-OF9O, R-MBDE-270D
Phase 07 ✅ realizes D9 — root the rebuildable `runs/` tree under the service-owned `cache/` dir (not the root:root AppDir): collapse `scriptsRuntimeRoot` to `filepath.Dir(cfg.GenerationPath)` in every layout and fix the on-box boot crash-loop (`unlinkat /opt/scripts/runs: permission denied`); update the R-4LKF-FB23 boot/composition tests to the cache location; covers R-RUNS-CDIR, R-RUNS-BOOT
Phase 08 ✅ realizes D10 — adopt the shared `registry` for loopback-port resolution: own port via `registry.MustPort("scripts")`, peer feed defaults + dropbox base via `registry.BaseURL`, add the `registry` require + committed `replace registry => ../registry` to `go.mod`, align the composition-root and nginx-content tests, and add the guardrail test proving no `30xx` literal remains in non-test Go source (behavior-preserving; `etc/manifest.env` byte-identical); covers R-RGST-SELF, R-RGST-PEER, R-RGST-DBOX, R-RGST-NLIT, R-RGST-GMOD
