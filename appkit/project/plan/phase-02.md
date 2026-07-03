# Phase 02 — `bin/registry` reads through `current`

Realizes **D2**. Repo-root shell (outside `appkit/`); part of this layout-parity
fix so both manifest readers address the identical path.

## Build (`bin/registry`)

- `manifest_path()` returns `${REGISTRY_ROOT}/$1/etc/current/manifest.env`
  (was `${REGISTRY_ROOT}/$1/etc/manifest.env`).
- The list loop globs `"$REGISTRY_ROOT"/*/etc/current/manifest.env`, and the
  name-extraction that strips the trailing `/etc/manifest.env` becomes
  `/etc/current/manifest.env`.
- `REGISTRY_ROOT` default (`/opt`) is unchanged.

## Tests (`bin/registry.test.sh`)

- **R-YQFZ-11IM** — update the fixture writer so each service's manifest is placed
  at `<root>/<name>/etc/<ver>/manifest.env` with a real `current → <ver>` symlink.
  Assert the addr/feed/mcp lookups and the list subcommand resolve services through
  `etc/current/manifest.env`, and add a case where a service has only the sibling
  `<root>/<name>/etc/manifest.env` (no `current`) and is **not** resolved.

## Done

`bin/registry.test.sh` passes. Commit.
