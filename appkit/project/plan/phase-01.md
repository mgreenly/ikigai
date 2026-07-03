# Phase 01 — `appkit/inventory` reads through `current`

Realizes **D1**. The center of the fix, fully inside `appkit/`.

## Build

- In `appkit/inventory/inventory.go`, change the glob in `Read` from
  `filepath.Join(root, "*", "etc", "manifest.env")` to
  `filepath.Join(root, "*", "etc", "current", "manifest.env")`. Nothing else in
  `Read` changes — parse via `appkit/manifest.Parse`, the `MCP=true` filter, the
  sort by `Name`, and the per-manifest skip-on-error all stay. `Name` still comes
  from `env["APP"]`, so the added `current` path element does not affect it.
- Update the package doc comment on `inventory.go` (it currently says
  "globs root/*/etc/manifest.env") to state the `current`-relative path and that
  `os.Open` follows the `current` symlink to the live version.

## Tests (`appkit/inventory/inventory_test.go`)

- **R-YO06-9I18** — build a temp root with one service whose manifest is at
  `<svc>/etc/current/manifest.env` (real `current` symlink → a version dir,
  `MCP=true`) and a second service whose manifest is only at the sibling
  `<svc>/etc/manifest.env` (no `etc/current/`). Assert `Read` returns the first and
  **not** the second.
- **R-YP82-N9RX** — one service with `etc/current → verA/` (manifest `MCP=true`):
  assert listed. Repoint `etc/current → verB/` (manifest `MCP=false`) and call
  `Read` again: assert no longer listed. Use real directories and real symlinks
  (the behavior under test is symlink resolution).
- Update any existing inventory_test.go fixtures that still write the sibling
  `etc/manifest.env` to the `etc/current/manifest.env` shape.

## Done

All ids covered by genuinely-asserting tests and the appkit suite is green:
from `appkit/`, `go build ./...`, `go vet ./...`, `gofmt -l .` (empty), and
`go test ./...` all pass. Commit.
