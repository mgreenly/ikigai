# notify — cleanup findings

## High-priority (named migrations)
- ✅ **DONE 2026-07-03** — notify/etc/nginx.conf:12-13 — Registry migration: parenthetical "(When the service registry lands notify's port becomes 3201; update the literal below then.)" is a superseded future-tense TODO. The service registry has landed (top-level `registry/` folder + `bin/registry`), and the port literal is already 3201, so the note is stale and contradicts registry-as-source-of-truth.
- Deploy format → tar.gz: none. notify's deploy prose (CLAUDE.md "Manifest / deploy", Makefile, bin/*) matches the current `bin/ship` → `opsctl stage`/`deploy` tar.gz + three-symlink model. No flat-bin / single-binary-copy language found.

## Other stale info
- notify/CLAUDE.md:117 — `Consumes:["crm"]` in the deploy paragraph is stale; the code (`cmd/notify/main.go:65` `Consumes: []string{crmSource, promptsSource}`) and `etc/manifest.env:6` are `crm,prompts`. Contradicts CLAUDE.md's own lines 10-13. (contradictory)
- notify/CLAUDE.md:121 — `CONSUMES=crm` is stale; `etc/manifest.env:6` is `CONSUMES=crm,prompts`. (contradictory)
- notify/CLAUDE.md:117 — "the consumer loop run as an appkit `Worker`" (singular); notify now runs two Workers (separate crm and prompts consumer loops, `main.go:109`). (contradictory / understated)
- notify/CLAUDE.md:18 — links `../../docs/plan-notify-mcp-send.md`; file moved to `docs/archive/plan-notify-mcp-send.md`. (dead path)
- notify/CLAUDE.md:24 — links "normative" `../../docs/event-protocol.md`; now `docs/archive/event-protocol.md`. (dead path)
- notify/CLAUDE.md:26 — links `../../docs/event-plane-decisions.md`; now `docs/archive/event-plane-decisions.md`. (dead path)
- notify/CLAUDE.md:42 — cites dev mirror `../nginx/locations/notify.conf`; `nginx/locations/` is empty (only `.gitkeep`), so the mirror no longer exists. (dead path)
- notify/etc/nginx.conf:3 — "Per docs/path-routing-architecture.md"; no such file exists in `docs/` or `docs/archive/` (closest current doc is `docs/app-layout.md`). (dead path)
- notify/project/README.md:10-14 — states notify "has **no spec and no live build loop yet** — the spine documents below are empty placeholders." Contradicts reality: `project/product/product.md` (154 lines) and `project/design/design.md` (133 lines) are fully written. Leftover scaffolding template. (contradictory / stale template) ✅ **DONE** (scaffold blockquote removed suite-wide; every service README treated as fully built per operator directive, 2026-07-03)
- notify/project/README.md:15 — "End-user documentation for this service lives in `notify/docs/`"; no `notify/docs/` directory exists. (dead path)

## Notes
- notify/bin/secrets:11 references the wrapper at `/opt/notify/bin/run` — this is CURRENT, not stale: `deploy.md` shows the three-symlink swap covers `bin/run` (+ `etc/current`, `share/current`). Left unflagged.
- The archived event docs (event-protocol.md, event-plane-decisions.md, plan-notify-mcp-send.md) still exist under `docs/archive/`; their being archived may itself signal supersession, but I could not confirm their content is stale — only that CLAUDE.md's link paths are wrong.
- Design docs D01-D08 / design.md were spot-checked: design.md:56 already says `Consumes:[]string{"crm","prompts"}`, so the design side is current; the crm-only staleness is confined to CLAUDE.md.
