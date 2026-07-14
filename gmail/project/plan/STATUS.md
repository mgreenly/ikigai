# gmail — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/gmail/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/gmail/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `gmail/AGENTS.md`/`CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-7Q9U
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/gmail/static/`; covers R-3X4A-Y8CI, R-3YC7-C037, R-40S0-3JKL, R-41ZW-HBBA
Phase 07 ✅ realizes D11 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod and resolve gmail's own port via `registry.MustPort("gmail")` in the Spec (producer — no peer feeds); covers R-9QEG-KF05
Phase 08 ✅ realizes D12 — prove no `127.0.0.1:3xxx` literal remains in gmail's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` so a renumber fails a gmail test; covers R-9RMC-Y6QU, R-9SU9-BYHJ
Phase 09 ✅ realizes D13 — composition-root normalization: move the `appkit.Spec` from `internal/gmailapp` into `cmd/gmail/main.go` as `gmailSpec()`, delete `internal/gmailapp`, repoint `main_test` (structural; pure relocation)
Phase 10 ✅ realizes D9 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/gmail`), boundary-crossing `GMAIL_WWW_PATH` export in `bin/start`; covers R-9LIV-1C1D, R-9MQR-F3S2 (retained D1/D2/D3/D4/D7/D8 ids re-proven on the new substrate)
Phase 11 ✅ realizes D10 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(client)` (the ten mailbox tools) + `NewHandler`, local transport and local health/reflection deleted, surface stays exactly twelve tools; covers R-9NYN-SVIR
Phase 12 ✅ realizes D14 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + outbox/load guards remain) and true up `gmail/AGENTS.md` to the converted service (structural)
Phase 13 ✅ realizes D15 — add `error_page 401 = @login_bounce;` to the two session-gated locations in `gmail/etc/nginx.conf` (`= /srv/gmail/`, `/srv/gmail/static/`), leaving the bearer tier untouched; purely additive, proven by extending the `cmd/gmail/nginx_test.go` nginx content assertions; covers R-3YU6-CQ9P, R-4022-QI0E, R-419Z-49R3
Phase 14 ✅ realizes D16 — attachment content endpoint: `Client.AttachmentGet` (offline-stub tested), the loopback-private `GET /attachment` handler (identity-header guard → 404, MIME-walk Content-Type, 404/502 mapping), ungated mount in `Handlers`, nginx `= /srv/gmail/attachment` 404 fragment; covers R-WVZH-M0IY, R-WX7D-ZS9N, R-WYFA-DK0C, R-WZN6-RBR1, R-X0V3-53HQ, R-X22Z-IV8F
Phase 15 ✅ realizes D17 — attachment references in `read`/`thread`: entries gain `attachment_id` + complete URL-encoded `content_url` (base from `registry.MustPort("gmail")`), inline parts stay metadata-only, `Tools`/`NewHandler` gain `contentBase`, descriptions state the reference truth; covers R-X3AV-WMZ4, R-X4IS-AEPT, R-X5QO-O6GI
Phase 16 ✅ realizes D18 — event-routing conformance (EXTERNAL: requires eventplane plan phases 01–04 built first): kinds `received`/`sent`/`deleted` with empty subject, `mcp.Events` becomes three `outbox.Family` entries, new timestamped outbox migration to the revised `SchemaSQL` (003 frozen, guard re-pointed), feed frames `event: gmail:received`; covers R-X6YL-1Y77, R-X86H-FPXW, R-X9ED-THOL, R-XAMA-79FA
Phase 17 ✅ realizes D16, D17 — attachment addressing by stable `part_id`: handler resolves the fresh token from its own fetch (rotating-token 404 fixed), `read`/`thread` references drop `attachment_id` and carry durable `content_url`; covers R-3G57-009Q, R-3HD3-DS0F, R-3IKZ-RJR4, R-3JSW-5BHT, R-3L0S-J38I, R-3M8O-WUZ7
Phase 18 ✅ realizes D19 — live attachment round-trip check: `//go:build live` send-to-self → resolve `content_url` → byte-equal → permanent delete, creds from env, failure (not skip) when absent; covers R-3NGL-AMPW
Phase 19 ✅ realizes D20 — structured MCP results/schemas/error codes in `internal/mcp`: `JSONResult`→`StructuredResult` on all ten mailbox tools, hand-authored `outputSchema` per tool, typed error codes via `errorResultFor`, no `JSONResult` token remains; covers R-8K29-UF3R, R-8LA6-86UG, R-8MI2-LYL5, R-8NPY-ZQBU, R-8OXV-DI2J, R-8RDO-51JX
Phase 20 ✅ realizes D20 — swap the `GET /attachment` endpoint to the shared loopback guard: delete the hand-copied two-header predicate, mount via `rt.HandleLoopback` (`X-Forwarded-Proto`-only), caller-asserted `X-Owner-Email` no longer rejected; covers R-8Q5R-R9T8
