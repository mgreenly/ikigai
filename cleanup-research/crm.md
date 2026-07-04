# crm — cleanup findings

## High-priority (named migrations)
- ✅ **DONE 2026-07-03** — crm/etc/nginx.conf:12-13 — Registry migration. Parenthetical "(When the service registry lands crm's port becomes 3100; update the literal below then.)" is stale twice over: the top-level `registry/` folder has landed, and the port is already the literal 3100 everywhere. Misleads a reader into thinking registry is future work.
- crm/AGENTS.md:114 — Deploy-format migration (contradicts current tar.gz flow). Claims `manifest.env` "is emitted by `crm manifest` and regenerated on the box by `opsctl deploy` on every swap." Root deploy.md:22-24 states the opposite: "There is no `manifest`-verb / on-box manifest-generation step in the operator flow. The bundle already carries the authored `manifest.env`." Stale on-box-regeneration model.

## Other stale info
- ✅ **DONE 2026-07-03** — crm/AGENTS.md:30 — dead path: "The full design is in `project/notes/PLAN.md`". No `crm/project/notes/` dir exists; design now lives in `crm/project/design/`. (dead file path)
- ✅ **DONE 2026-07-03** — crm/project/design/design.md:18 — same dead path `crm/project/notes/PLAN.md` cited as owner of the CRM domain. (dead file path)
- ✅ **DONE 2026-07-03** — crm/bin/start:16 — calls `bin/build`, which does not exist (crm/bin/ holds only start+stop; main.go:12 confirms the bin/build run-wrapper was deleted). Local start path is broken/stale; build now goes through the Makefile. (dead script reference)
- crm/etc/nginx.conf:3 — references `docs/path-routing-architecture.md`; no such file exists (closest current suite docs: docs/app-layout.md, docs/service-registry-design.md). (dead file path)
- crm/etc/nginx.conf:27 — references `event-protocol.md §2`; the file now lives at `docs/archive/event-protocol.md`. (moved/dead file path, minor)

## Notes
- crm/etc/deploy.env — header says it is "Workstation-side routing for bin/* scripts," but shipping is now the repo-root `bin/ship crm`; unclear whether this per-service file is still consumed. Uncertain, not verified.
- crm/project/product/product.md:113 references `design/carbon.md` / `design/tokens.css` / `design/example.html` as the visual-system source of truth; these are suite/shared design paths I did not resolve — flagged only as uncertain.
- README project table (crm/project/README.md:27) lists a `notes/` folder that does not currently exist on disk; this is descriptive of an optional folder, not clearly stale.
