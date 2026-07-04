# dashboard — cleanup findings

## High-priority (named migrations)
- none — the dashboard is already aligned with both migrations:
  - **Service names / registry:** dashboard hardcodes only its *own* fixed port
    (`cmd/dashboard/main.go:49,99` → `3000`; `etc/manifest.env:4`), and
    `etc/nginx.conf:3` already calls it "its fixed registry port." Its `/services`
    inventory (`internal/server/inventory.go`) reads live manifests, not a
    hardcoded name list. No dashboard doc/code claims to own the authoritative
    service name→port table that `registry/` now holds.
  - **Deploy tar.gz:** `dashboard/AGENTS.md` describes deploy correctly as
    `bin/ship dashboard` → `opsctl stage` + `opsctl deploy` (versioned bundle,
    three-symlink swap). No flat-bin / per-binary-copy language survives. The
    `Makefile` `bin/dashboard` target is a local dev build artifact, not the
    deploy model.

## Other stale info
- AGENTS.md:52,72 — references `project/notes/phases.md` for per-phase scope; no `project/notes/` dir exists (only bugs/design/loops/product/requests/research). (dead path)
- AGENTS.md:50-72 — "Build phases … Phase 0 (current): structural web app, **no auth**" contradicts the fully-built service: googleidp, oauth AS, PAT, web sessions, `/internal/authn` are all implemented and tested. The app is well past Phase 0/1/2. (contradictory architecture/status)
- AGENTS.md:9,84,92 — `../crm.bak/` porting reference; that directory does not exist at repo root. The port is done; the reference is dead. (dead path)
- AGENTS.md:138-149 — cutover note is multiply stale: says "the live `ai` box" (current account is `int`); "migrations were renumbered name/timestamp-keyed → integer-keyed" contradicts the current timestamp convention (migrations dir now holds legacy `001`-`005` **plus** a timestamp migration `20260609011053_add_personal_tokens.sql`); "fresh DB migrates clean to v5" (there are 6 migrations now); and it cites `docs/runbook-dashboard-box-cutover.md`, which does not exist. (contradictory / dead path)
- AGENTS.md ~100-108 — describes `plugin/` and `.claude-plugin/marketplace.json` as living in this repo/dashboard; neither exists in `dashboard/`. (described path absent — may be future scope)
- project/README.md:9-15 — "**Status: scaffold.** … dashboard has **no spec and no live build loop yet** — the spine documents below are empty placeholders." Contradicts populated `product/product.md` (162 lines), `design/design.md` + D01–D10 + INDEX, and the fully built service. (contradictory status) ✅ **DONE** (scaffold blockquote removed suite-wide; every service README treated as fully built per operator directive, 2026-07-03)
- project/README.md:27 — lists a `notes/` folder in the workspace layout; no `notes/` dir exists. (dead ref; same root as AGENTS.md:52,72)

## Notes
- `push`/VAPID/subscription store is described in AGENTS.md (~"Build new") as a
  dashboard capability, but no push package exists under `internal/`. AGENTS.md's
  own phase list defers it to "Phase 2 and later," so this may be intended-future
  rather than stale. Uncertain — flagged for a human.
- Makefile:4 comment "version currently lives in package main; moves to
  internal/version later" — `internal/version` does not exist; version is still
  in `main`. This is a forward-looking note, not stale.
