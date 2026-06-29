# Phase 07 — eventplane epoch re-mint by exclusion

*Realizes design Decision 6 (epoch re-mint by exclusion + boot-reconstruction invariant) — the eventplane slice. Depends on Phase 06 (the non-state region the sidecar lives in and the boot reconstruction that recreates it).*

The event-plane generation/epoch sidecar lives in the non-state region, so a
restore (which resets non-state, Phase 08) leaves it absent and the next boot mints
a fresh epoch by construction — no explicit per-restore delete step. The observable
end state proven here in the `eventplane` package: an absent sidecar mints a new
generation token distinct from the prior one, and a consumer cursor minted under
the old epoch is rejected at connect after the producer re-mints, forcing a resync
rather than a silent resume onto reused sequences.

**Done when:** `bin/test` exits 0 and:
- R-4BT8-D54J — boot with the sidecar absent (the post-restore condition) mints a
  generation token differing from the token present before (real fs + eventplane
  `outbox` `loadOrMintGeneration`).
- R-4D14-QWV8 — a cursor minted under the pre-restore epoch is rejected with
  `stale-epoch` at connect after re-mint, forcing resync — proven against the
  **real** eventplane (`outbox.FeedHandler()` on an `httptest` server + the real
  consumer engine), not a stub.
