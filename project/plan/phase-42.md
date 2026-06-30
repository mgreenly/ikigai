# Phase 42 — adopt scripts into the new layout (classify runs/)

*Realizes the scripts slice of design Decision 8 (per-service adoption) and Decision 11 (authored portable manifest). Depends on Phase 23, 31, 32.*

scripts adopts the new contract and consciously classifies its `runs/` trees: whatever is durable moves
under `state/`, the rest stays non-state (`cache/`, reset on restore). Committed `scripts/etc/manifest.env`
(authored, portable) and `scripts/etc/nginx.conf` are added; data paths resolve via `composeDataPaths`
(Phase 23); non-state is recreated on boot (D06).

**Done when:**
- A test tagged `R-8DF1-W89F` asserts `scripts/etc/manifest.env` is portable (no `/opt/…`, no path-override
  lines).
- A Go unit test tagged `R-8IAN-FB87` asserts `manifest.Emit(scripts-spec)` byte-agrees with the committed
  `scripts/etc/manifest.env`.
- A boot smoke tagged `R-4LKF-FB23` asserts a freshly set-up `/opt/scripts/` boots and passes `health`,
  recreating its non-state (`runs/` scratch included) from empty.
- `bin/test` exits 0.
