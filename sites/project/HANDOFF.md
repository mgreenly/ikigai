# sites — handoff (in-process serving redesign)

**Date:** 2026-07-08 · **Branch:** `static-sites` (NOT merged to `main`, NOT deployed) ·
**Next session:** live-verify sites end to end, then decide on merge.

## What changed and why

We removed the special case where **nginx served sites' public/private tiers off
disk** via `alias`. The old model had a `working/` tree, a per-visibility
symlinked `served/` tree, and a `publish`/`unpublish` lifecycle — a filesystem
re-encoding of DB state that only existed because nginx can't read the database.

New model:
- A site is a **public or private** folder at `state/www/<public|private>/<slug>/`.
  A site that exists is served; there is **no publish step and no draft state**.
- The **sites process serves every byte** under `/srv/sites/…` (new
  `internal/serve` package); nginx `proxy_pass`es both tiers (private keeps
  `auth_request /_session-authn`) and reads nothing off disk.
- DB row is now `name, public (bool), created_by, source_path, created_at,
  updated_at` (retired: `tier`, `published`, `published_at`).
- MCP: `publish`/`unpublish` are **gone**; `set_visibility(name, public)` flips
  the flag and moves the folder; `create` records the caller as `created_by`;
  file tools/`sync`/`delete` act on the live served folder.
- The `/srv/sites/` landing page now **lists the sites** (slug, public/private,
  creator, created-at) beside the version.

Full spec: `project/product/product.md`, `project/design/design.md` +
`D15`–`D20`, `project/plan/phase-14`–`19`.

## Current status — DONE and green (units only)

All six phases (14–19) were built and verified by the ralph loop; every phase has
a build + "verified green" commit on `static-sites` (tip `fa08f17`). As of this
handoff the **Go suite is green**: `cd sites && go build ./... && go vet ./... &&
gofmt -l . && go test ./...` all pass (cmd/sites, internal/{db,files,mcp,serve,sites}).

Artifacts landed: `internal/serve/{serve.go,serve_test.go}`; migrations
`20260708010236_add_public_created_by.sql` and
`20260708012637_drop_publish_lifecycle.sql`; `internal/sites/publish.go` deleted.

**What green does NOT prove:** the unit suite exercises handlers in isolation
(`httptest`, temp dirs, in-memory DB). **Nobody has driven the whole stack** —
nginx → process → disk, the MCP tools against a running service, or the landing
page in a browser. That is the next session's job.

## Next session — live verification runbook

From the repo root (`/mnt/projects/ikigenba/static-sites`):

1. **Fresh state, then bring up the suite.** `bin/stop --clean` (wipe the old dev
   DB — the drop-column migration should be exercised on a clean DB; state is
   disposable during the migration window), then `bin/start`. Confirm the
   `ikigenba_sites_*` MCP tools are present and `health` passes; if not, complain
   loudly (suite isn't up) before proceeding.
   - ⚠️ The dev front door regenerates `nginx/locations/sites.conf` from
     `sites/etc/nginx.conf` on `bin/start`. Confirm the regenerated fragment shows
     `proxy_pass … /public/` and `… /private/` (NOT `alias`). If a stale nginx on
     `:8080` is holding the port from another worktree, **do not kill it** — surface
     it and ask.

2. **Drive the product success criteria** (`project/product/product.md`) against
   the running service — each is the real check:
   - `create` a site → `file_write` an `index.html` into it → immediately GET its
     URL and get the page (no publish step).
   - A **public** site: GET its path with no dashboard session → served. A
     **private** site: GET without a session → `401`; with a session → served.
   - `set_visibility` public↔private → reachability flips; never both at once.
   - `delete` → its URL stops serving.
   - Open `<host>:8080/srv/sites/` in a browser (logged in) → Carbon page with the
     version + a row per site (slug, public/private, creator, created-at). Logged
     out → `401`.
   - Directory behavior: a dir with `index.html` serves it; a dir without → `404`
     (not a listing, not `403`); `..` traversal → `404`.
   - Sanity: PRM well-known + bearer `/mcp` + `/health` still behave.

3. **If all green:** the work is verifiable end to end → ready to merge
   `static-sites` → `main` (ask before merging). **Deploy is separate and
   explicit** — do not `opsctl`/`ssh int` unless told to.

## Open items / risks to watch

- **Not merged, not deployed.** Branch only.
- **`created_by` on `sync`-created sites.** `sync` create-or-reuses a site; confirm
  it sets a sensible `created_by` (it has no MCP identity the same way `create`
  does) — check the code path if a synced site shows an empty creator.
- **Drop-column migration on a non-fresh DB.** Verified on fresh DBs by the suite;
  `bin/stop --clean` sidesteps any existing-data edge. If we ever run it against a
  populated dev DB, re-check the STRICT-table `DROP COLUMN`.
- **Reserved names / slug collisions across the two folders** behave as before
  (slug is the PK, one row per site) — spot-check `set_visibility` doesn't strand a
  folder if the move is interrupted.

## Pointers

- Spec: `project/design/INDEX.md` (D1–D20 + id map), `project/plan/STATUS.md`.
- Code: `internal/serve` (serving), `internal/sites/{store,layout}.go` (data +
  paths), `internal/mcp/tools.go` (surface), `cmd/sites/main.go` (wiring +
  landing), `sites/etc/nginx.conf` (fragment), `share/www/landing.html` (template).
- To rebuild from scratch (operator-only): `cd sites && ralph project/loops/gather.md project/loops/build.md project/loops/verify.md`.
