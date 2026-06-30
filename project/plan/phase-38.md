# Phase 38 — adopt prompts into the new layout (classify sandboxes/ and runs/)

*Realizes the prompts slice of design Decision 8 (per-service adoption) and Decision 11 (authored portable manifest). Depends on Phase 23, 31, 32.*

prompts adopts the new contract and makes the **conscious data classification** the model demands:
durable `sandboxes/` move under `state/` (backed up), while rebuildable `runs/`-style operational output
stays in the non-state region (`cache/`, not backed up, reset on restore). Committed
`prompts/etc/manifest.env` (authored, portable) and `prompts/etc/nginx.conf` are added; data paths resolve
via `composeDataPaths` (Phase 23); non-state (including `runs/`) is recreated on boot (D06).

**Done when:**
- A test tagged `R-8DF1-W89F` asserts `prompts/etc/manifest.env` is portable (no `/opt/…`, no path-override
  lines).
- A Go unit test tagged `R-8IAN-FB87` asserts `manifest.Emit(prompts-spec)` byte-agrees with the committed
  `prompts/etc/manifest.env`.
- A boot smoke tagged `R-4LKF-FB23` asserts a freshly set-up `/opt/prompts/` — with `sandboxes/` under
  `state/` and `runs/` in non-state — boots and passes `health`, recreating its non-state from empty.
- `bin/test` exits 0.
