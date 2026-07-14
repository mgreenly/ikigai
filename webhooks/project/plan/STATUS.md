# webhooks — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` followed by its zero-padded number, then a status marker — `✅` (done) or
`⬜` (not started) — then `realizes <Decision ids>` (or `realizes —` for a purely
structural phase), then `— <one cohesive objective>`. The build loop finds its
next unit of work with
`grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only that
phase's `project/plan/phase-NN.md`, and on completion flips that one marker
`⬜ → ✅`. A phase body file carries no marker of its own. This document
deliberately carries **no** bare status glyph outside the phase lines, so the
anchored grep matches only phase lines.

Phase 01 ✅ realizes D2 — Module scaffold & data model (migrations + Store)
Phase 02 ✅ realizes D3 — Webhook identity & secret lifecycle (Service/Clock seam, Create/Rotate)
Phase 03 ✅ realizes D5 — Event production (durable-before-ack Record)
Phase 04 ✅ realizes D4 — Public ingress endpoint /in/<name>
Phase 05 ✅ realizes D6 — MCP tool surface (the four owner tools)
Phase 06 ✅ realizes D1 — Composition root & chassis boot
Phase 07 ✅ realizes D7, D8 — nginx fragment, dev-harness wiring & e2e/onboarding
Phase 08 ✅ realizes D9 — human landing page: `internal/web` handler + embedded cron-canonical template/Carbon assets, wired at `GET /{$}` + `GET /static/` (ungated in-process) in `cmd/webhooks/main.go`; web-unit tests
Phase 09 ✅ realizes D7 — add the session-gated exact `= /srv/webhooks/` landing tier + `/srv/webhooks/static/` tier to `etc/nginx.conf`, validated by a content-assertion `nginx_test.go`
Phase 10 ✅ realizes D10 — adopt `registry`: `require`+`replace registry`, `Spec.Port` → `registry.MustPort("webhooks")`, add the source-scan guard, and re-point the manifest/nginx/e2e tests at `registry` so no `127.0.0.1:30xx` literal survives and a renumber fails a test; covers R-0D7X-9EB6, R-0EFT-N61V, R-0FNQ-0XSK
Phase 11 ✅ realizes D11 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/webhooks` over the shipped tree), boundary-crossing `WEBHOOKS_WWW_PATH` export in `bin/start`; covers R-0GVM-EPJ9, R-0I3I-SH9Y (retained D9 ids re-proven on the new substrate)
Phase 12 ✅ realizes D12 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(svc, baseURL)` (create/list/delete/rotate) + `NewHandler(svc, rt)`, local transport and local health/reflection deleted, surface stays exactly six tools; covers R-0JBF-690N (retained D6 tool ids re-proven through the assembled handler)
Phase 13 ✅ realizes D13 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + domain Store + outbox guard remain, harnesses call `appkit/db` directly), normalize the composition root to one inline `Spec` (no post-construction `.Handlers`/`.Producer` mutation), and true up the `main.go` doctrine comment (structural)
Phase 14 ✅ realizes D14 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `webhooks/etc/nginx.conf` (`= /srv/webhooks/`, `/srv/webhooks/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/webhooks/nginx_test.go` nginx content assertions; covers R-4B16-6FON, R-4C92-K7FC, R-4DGY-XZ61
Phase 15 ✅ realizes D15 — event-routing conformance (⛔ externally ordered on eventplane phases 01–04 + appkit): kind `received`, subject `/<hook name>`, one-family registry, new timestamped `outbox_routing` migration + re-pointed DDL drift guard (frozen `003` untouched); covers R-A3FB-J3ZK, R-A4N7-WVQ9, R-A5V4-ANGY, R-A730-OF7N
Phase 16 ✅ realizes D16 — structured MCP adoption (⛔ externally ordered on appkit phases 12–14): swap the four verbs' `JSONResult`→`StructuredResult`, declare a per-tool `outputSchema`, and replace the hand-built error envelope with closed-vocabulary `ErrorResult(code, msg)` (`ErrNameTaken`→`conflict`); covers R-DRUS-R3AP, R-DT2P-4V1E, R-DUAL-IMS3, R-DVIH-WEIS, R-DWQE-A69H, R-DXYA-NY06
