# Phase 16 — MCP surface: drop publish/unpublish, thread `created_by`, serve the live folder

*Realizes design Decision 20 (MCP surface). Depends on Phase 14 (`SiteDir`) and 15 (`Public`/`CreatedBy`/`SetVisibility`/`Move`).*

Switch the `ikigenba_sites_*` tools to the new model.

- Remove the `publish` and `unpublish` tools from the table; remove their
  handlers. Add a `set_visibility(name, public)` tool that calls
  `Store.SetVisibility` + `Layout.Move` in lockstep and returns the site.
- `create(name)` reads the request `Identity`'s owner email and calls
  `store.Create(ctx, name, identity.Owner)`, then creates `SiteDir(false, name)`
  (private by default).
- `delete(name)` removes the row and `os.RemoveAll(SiteDir(site.Public, name))`
  (no unpublish, no working-tree removal).
- The file tools (`file_write/read/edit/glob/grep/list`, `mkdir`) and `sync`
  resolve their root as `SiteDir(site.Public, name)` (from a `Get`), replacing
  `layout.WorkingDir(name)`; confinement via `internal/files.ConfinePath` unchanged.
- `renderSite` projects `name`, `public`, `created_by`, `url`
  (`<baseURL>public/<name>/` | `<baseURL>private/<name>/`), `created_at`,
  `updated_at`; drop `tier`/`published`/`published_at`. `errResult` drops the
  `ErrInvalidTier` case. `describe`/`list` text drops the publish lifecycle and
  working-tree language.
- `Store.Publish`/`Unpublish` remain defined (unused) — deleted in Phase 19.

**Done when:** the sites suite is green with handler-boundary tests covering
R-RDBZ-AE4J (no `publish`/`unpublish` in the tool table), R-RFRS-1XLX (`create`
records the Identity owner as `created_by`), R-RGZO-FPCM (`set_visibility` flips
the flag, moves the folder, and the returned `url` reflects the new tier),
R-RI7K-TH3B (a `file_write` lands under `SiteDir(site.Public, name)`), and
R-RJFH-78U0 (`delete` removes the row and the on-disk directory).
