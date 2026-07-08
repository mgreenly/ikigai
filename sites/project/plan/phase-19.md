# Phase 19 — Cleanup: drop the lifecycle columns and delete the working-tree/symlink machinery

*Realizes design Decision 15 (data model — cleanup half) and 16 (delete indirection). Depends on Phase 16 (MCP off the old methods/columns) and 18 (landing off the old fields).*

Now that nothing reads the old state, delete it.

- New **cleanup** migration (via `bin/create-migration sites drop_publish_lifecycle`)
  dropping `tier`, `published`, `published_at` from `sites` (SQLite `DROP COLUMN`
  or a STRICT-preserving rebuild). `002_sites.sql` and the additive Phase-15
  migration stay frozen.
- `internal/sites/store.go`: remove `Tier`/`Published`/`PublishedAt` from `Site`;
  `scanSite` and the `Create`/`Get`/`List` column lists read only the final column
  set (`name`, `public`, `created_by`, `source_path`, `created_at`, `updated_at`);
  delete `Store.Publish`, `Store.Unpublish`, and `ErrInvalidTier`.
- Delete `internal/sites/publish.go` (the `symlinkTarget`/`os.Symlink` body) and
  the `WorkingSeg`/`WorkingDir`/`WorkingBase`/`ServedSeg`/`ServedDir`/`ServedBase`/
  `ServedTierBase` helpers from `layout.go` (keep `SiteDir`/`SiteBase`).

**Done when:** the sites suite is green (`go build`/`go vet`/`gofmt -l .`/`go test`),
AND R-QQ5W-0R1C: a test asserts `pragma table_info(sites)` after the full migration
set excludes `tier`/`published`/`published_at` and includes `public`/`created_by`;
AND R-QYP6-P587: `test ! -e internal/sites/publish.go` and a scoped grep
`grep -rnE 'symlinkTarget|os\.Symlink|WorkingDir|ServedDir|ServedTierBase|ServedBase' internal/`
(the workspace `project/` tree excluded) returns no matches.
