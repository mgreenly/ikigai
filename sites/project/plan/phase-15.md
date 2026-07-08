# Phase 15 — Data model: add `public` + `created_by`, `SetVisibility`, and `Move` (additive)

*Realizes design Decision 15 (data model — additive half) and 16 (`Layout.Move`). Depends on Phase 14 (`SiteDir`/`SiteBase`).*

Add the new columns, struct fields, and methods without dropping the old ones, so
callers can migrate while the suite stays green.

- New **additive** migration (via `bin/create-migration sites add_public_created_by`)
  adding `public INTEGER NOT NULL DEFAULT 0` and `created_by TEXT NOT NULL DEFAULT ''`
  to `sites`; `tier`/`published`/`published_at` remain for now.
- `internal/sites/store.go`: add `Public bool` and `CreatedBy string` to `Site`;
  `scanSite` and the `Create`/`Get`/`List` column lists read the new columns
  (keeping the old ones). Change `Create` to `Create(ctx, name, createdBy string)`
  inserting `public=0`, `created_by=createdBy`. Add
  `SetVisibility(ctx, name string, public bool) error` (flips `public`, bumps
  `updated_at`, `ErrNotFound` on absent). `Publish`/`Unpublish` stay (still called
  by the MCP layer until Phase 16).
- `internal/sites/layout.go`: add `Move(slug string, toPublic bool) error`
  relocating the site directory between `SiteBase(false)` and `SiteBase(true)`
  (no-op when already there; tolerates a missing source).
- Update the `Create` callers in `internal/mcp` to pass a creator string
  (temporary literal is fine — Phase 16 threads the real Identity) so the build
  stays green.

**Done when:** the sites suite is green (`go build`/`go vet`/`gofmt -l .`/`go test`)
with tests over a real migrated SQLite DB covering R-QRDS-EIS1 (`created_by`
round-trips), R-QSLO-SAIQ (`public` defaults false), R-QTTL-629F (`SetVisibility`
toggles + `ErrNotFound`), and a filesystem test covering R-QW9D-XLQT (`Move`
relocates the directory and empties the old path; same-visibility `Move` is a
no-op).
