# Phase 16 — Event-routing conformance: producer kinds `succeeded`/`failed`, subject = /<script name>, family registry, outbox migration

*Realizes design Decision 18 (producer conformance). Depends on Phase 15 (the reshaped `runs` trigger context the completion payload reads). Covers R-82AG-F74Z, R-83IC-SYVO, R-84Q9-6QMD, R-85Y5-KID2.*

> **⛔ EXTERNAL ORDERING — operator-sequenced.** This phase consumes the
> **revised eventplane API** (`outbox.Event{Kind, Subject}`, `outbox.Family`/
> `Registry`, the kind/subject envelope + canonical-key SSE framing) specified
> in `eventplane/project/design/` and built by **eventplane plan phases 01–04
> — those must be BUILT (green in `eventplane/`) before this phase runs**, and
> appkit must compile against that revision (`Spec.Events outbox.Registry` and
> the chassis reflection tool). Cross-module building is sequenced by the
> operator, not by this plan; if the eventplane revision is not yet in the
> tree, this phase cannot build and must be left `⬜`.

Observable end state:

- `internal/script/outcome.go`: `EventSucceeded`/`EventFailed` become **kind**
  constants with values `"succeeded"`/`"failed"` (the redundant `scripts.`
  prefix drops — the source carries it); `completionEvent` emits
  `outbox.Event{Kind, Subject, Payload}` with subject `""` for an empty script
  name (the tombstoned-script failure path), else `"/" + name` (newlines →
  spaces); the payload's nested `trigger` object becomes
  `{source, kind, subject, event_id}` (no `type` key), all other payload
  fields unchanged; the same-tx atomicity, at-most-once, post-commit `Ring()`,
  and cancelled-emits-nothing rules are untouched.
- `script.Events` becomes an `outbox.Registry` of two `outbox.Family` entries
  (kinds `succeeded`/`failed`, subject description `/<script name>`, Samples
  updated to the reshaped trigger object with revised-model values);
  `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with `bin/create-migration scripts
  outbox_routing` (never a hand-picked number), drops and recreates the outbox
  table with the revised `outbox.SchemaSQL` verbatim. `004_outbox.sql` is
  byte-untouched. A **new** DDL drift guard
  `internal/db/migrations_outbox_test.go` (sibling-shaped; scripts had none)
  asserts the newest outbox migration contains the revised `outbox.SchemaSQL`
  verbatim.
- Existing outcome/reflection tests conform their expectations to the new
  kind/subject shape (hard cutover — no envelope `type` survives in Go source).

## Done when

The suite is green (design *Conventions* commands, from `scripts/`) and:

- **R-82AG-F74Z**, **R-83IC-SYVO**, **R-84Q9-6QMD**, **R-85Y5-KID2** — each
  covered by a clearly-named test asserting exactly the behavior its D18
  Verification line states (kind/subject/trigger-keys read back by SQL from
  real SQLite across succeeded/failed/cancelled and the empty-name case; the
  two-family reflection index/detail/unknown-kind; the fresh-DB migration
  column check + the newly added newest-migration DDL guard + frozen `004`;
  the real-`FeedHandler` frame `event: scripts:succeeded/nightly-export` with
  no `type` key).
- `git diff --stat -- scripts/internal/db/migrations/004_outbox.sql` is empty
  (the frozen migration is untouched) and exactly one new
  `*_outbox_routing.sql` timestamped migration is present.
- `grep -rn "scripts\.succeeded\|scripts\.failed" scripts --include=*.go`
  (from the repo root, or the equivalent from `scripts/`) returns no matches
  in Go source (the prefixed type strings are gone).
