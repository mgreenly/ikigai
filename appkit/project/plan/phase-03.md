# Phase 03 — `bin/start` stages a prod-shaped runtime manifest root

Realizes **D3**. Repo-root shell (outside `appkit/`). Depends on Phase 01 (the
reader must already go through `current`, or local dev breaks).

## Build (`bin/start`)

- After building each service binary, stage its manifest into a prod-shaped tree
  under `tmp/`:
  - `mkdir -p $RUN_DIR/opt/<svc>/etc/<ver>`, copy `<svc>/etc/manifest.env` to
    `$RUN_DIR/opt/<svc>/etc/<ver>/manifest.env`.
  - create the relative symlink `$RUN_DIR/opt/<svc>/etc/current -> <ver>`
    (use `ln -sfn <ver> …/etc/current`).
  - `<ver>` is a stable local literal — read `<svc>/VERSION` if present, else
    `dev`. Its value does not matter, only the `etc/<ver>/manifest.env` + `current`
    shape.
- Change `launch_dashboard` to export `DASHBOARD_MANIFEST_ROOT="$RUN_DIR/opt"`
  (was the repo root) and export `REGISTRY_ROOT="$RUN_DIR/opt"` wherever
  `bin/registry` is consulted (e.g. notify's resolution). Update the comment at
  `bin/start:66` that documents the repo-root manifest root.
- The source tree is untouched; the staged tree lives under `tmp/` and is already
  wiped by `bin/stop --clean` (confirm it is under `$RUN_DIR`).

## Verify

- **R-YRNV-ET9B** — bring the suite up with `bin/start`; assert
  `$RUN_DIR/opt/<svc>/etc/current/manifest.env` resolves for each service, and
  `curl -s http://127.0.0.1:3000/services` (or through nginx :8080) returns the
  full service set **including `crm`**. This is the live end-to-end smoke; capture
  it as a short check (script or documented command) so it is repeatable.

> ⚠️ Only start/stop the suite from **this** worktree. If the shared ports are held
> by a stack from another worktree, stop and surface it — do not kill it.

## Done

The live `/services` smoke lists all services incl crm through the staged root.
Commit.
