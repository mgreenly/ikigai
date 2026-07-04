# scripts — cleanup findings

## High-priority (named migrations)

Deploy format → tar.gz / versioned-slot on-box layout:

- scripts/bin/teardown:6,10,37,66 — Assumes the OLD flat on-box layout: preserves/removes `/opt/scripts/data` as "the DB AND the run folders". The current layout (deploy.md three-symlink swap; this service's own D09) stores the durable DB at `/opt/scripts/state/scripts.db` and runs under `/opt/scripts/cache/runs`. There is no `data/` tier — so "preserve `/opt/scripts/data`" would preserve nothing real and the actual DB in `state/` is unaddressed.
- scripts/bin/teardown:64 — `rm -rf "/opt/${APP}/bin" "/opt/${APP}/etc"` treats `bin/` and `etc/` as flat directories to delete. Current deploys use versioned release slots plus the `bin/run` / `etc/current` / `share/current` symlinks (deploy.md), not flat `bin/`+`etc/` dirs.
- scripts/Makefile:2-3 — Comment calls the `bin/` scripts "the production deploy spine (setup/deploy/start/stop on the box)". The production spine is now `bump → ship → stage → deploy` via opsctl (deploy.md); `bin/` here holds only local start/stop/teardown, and bin/start's own header says the on-box lifecycle "is owned by opsctl". No `setup`/`deploy` script exists in `bin/`.

Service names → registry/:
- none (scripts' hardcoded upstream ports in cmd/scripts/main.go feedDefaults — cron 3005, crm 3100, ledger 3101, dropbox 3200, prompts 3002 — match registry/project/design/D02.md exactly, and registry's own product.md notes services legitimately carry these literals as dev fallbacks; not stale).

## Other stale info

- scripts/project/README.md:9-11 — "Status: scaffold … no spec and no live build loop yet — the spine documents below are empty placeholders." Contradicted by the fully-populated spine: project/product/product.md (158 lines), project/design/design.md + D01–D09 + INDEX.md (9 decisions), populated loops/, and shipped code. (contradictory status)
- ✅ **DONE 2026-07-03** — scripts/project/design/design.md:60 — Lists the fixed verb set as `serve`/`version`/`manifest`/`migrate`/`backup`/`restore`. Superseded: current set is `serve`/`version`/`manifest`/`migrate`/`schema` (CLAUDE.md; cmd/scripts/main.go:6). `backup`/`restore` are box-level opsctl operations, not binary verbs (and this service's own D09.md:76 states exactly that). (superseded verb set)
- ✅ **DONE 2026-07-03** — scripts/project/design/design.md:19 — Points to `scripts/project/notes/ARCHITECTURE.md` and `scripts/project/notes/PLAN.md` as owning the existing domain. `project/notes/` does not exist. (dead path)
- scripts/project/product/product.md:15 — Says the existing domain "is owned by `scripts/project/notes/`". `project/notes/` does not exist. (dead path)
- scripts/project/design/D05.md:4,25-26,59-60,64 — Repeated references to `scripts/project/notes/README.md` and `scripts/project/notes/ARCHITECTURE.md` (a whole docs-update Decision targeting a directory that doesn't exist). (dead path)
- scripts/project/loops/build.md:50,65; scripts/project/loops/gather.md:105; scripts/project/loops/verify.md:58-59 — Build-loop prompts instruct grepping / editing `scripts/project/notes/*.md` (README.md, ARCHITECTURE.md). `project/notes/` does not exist. (dead path)
- scripts/project/README.md:15 — "End-user documentation for this service lives in `scripts/docs/`." `scripts/docs/` does not exist. (dead path — possibly aspirational)
- cmd/scripts/main.go:49,55,62 (comments) — Reference `PLAN.md §A4/§A11/§A12` for sourcing; that PLAN.md (project/notes/PLAN.md) does not exist. (dead path, code comment only)

## Notes

- bin/teardown is not covered by the root deploy.md runbook (opsctl owns box lifecycle), so its exact intended status is uncertain; regardless, its `/opt/scripts/{bin,etc,data}` paths contradict the current `state/`+`cache/`+symlink layout described in deploy.md and this service's D09.md.
- The `project/notes/` directory is referenced pervasively (design, product, loops) but is entirely absent — this looks like a directory that was planned/removed but whose pointers were never cleaned up. Treated as multiple dead-path findings above rather than one.
- Per scope, `scripts/project/plan/` was not inspected.
