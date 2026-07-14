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
Phase 08 ✅ realizes D10 — prove no `127.0.0.1:30xx` literal remains in notify's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` so a renumber fails a notify test; covers R-RGNL-4E5P, R-RGDR-4F6Q
Phase 09 ✅ realizes D11 — consumer loops through `Spec.Consumers`: declare crm+prompts with the push subscription lists and handler factories, delete `runConsumer`/`runPromptsConsumer`/`Workers`/the `var rt` capture and the legacy `Consumes`/`Subscriptions` fields, migrate env names to the chassis `NOTIFY_<SRC>_FEED_URL`/`NOTIFY_<SRC>_FROM` convention (`.envrc` + boundary-crossing `bin/start` lines), retire R-RGCF-4B2L/R-RGPF-4C3M/R-RGEO-4D4N with their moved behavior; covers R-4DG9-3Q97, R-4EO5-HHZW
Phase 10 ✅ realizes D12 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/notify` over the shipped tree), boundary-crossing `NOTIFY_WWW_PATH` export in `bin/start`; covers R-4FW1-V9QL, R-4H3Y-91HA (retained D1/D2/D3/D4/D7/D8/D10 ids re-proven on the new substrate)
Phase 11 ✅ realizes D13 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(client)` (the `send` tool) + `NewHandler`, local transport and local health/reflection deleted, surface stays exactly three tools; covers R-4IBU-MT7Z
Phase 12 ✅ realizes D14 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + feed-offset guards remain) and true up `notify/AGENTS.md` to the converted service (structural)
Phase 13 ✅ realizes D15 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `notify/etc/nginx.conf` (`= /srv/notify/`, `/srv/notify/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/notify/main_test.go` nginx content assertions; covers R-3IZH-DPMO, R-3K7D-RHDD, R-3LFA-5942
Phase 14 ✅ realizes D16 — ⛔ external ordering (eventplane 01–04 + appkit conformance first; crm/prompts producer conformance for live end-to-end truth): consumer-side event-routing conformance — crm filter `crm:contact.created/**` matched via `routing.Match(sub.Filter, ev.Key())`, prompts kind-exact `switch ev.Kind` with canonical-key subscription filters, no producer/no migration; covers R-ZCGU-FG9L, R-ZEWN-6ZQZ, R-ZG4J-KRHO, R-ZHCF-YJ8D
Phase 15 ⬜ realizes D17 — structured MCP adoption for `send`: success returns `StructuredResult({"ok":true})`, declares an `outputSchema`, and error paths use typed codes (`validation`; ntfy rejection/unreachable → `source_unavailable`, retiring the local `upstream` code); no guard swap, no migration; covers R-A918-YY6H, R-AA95-CPX6, R-ACOY-49EK, R-ADWU-I159
