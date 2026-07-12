# Phase 23 — Event-routing conformance: kinds `create`/`modify`/`delete`, subject = mirror path, family registry, outbox migration

*Realizes design Decision 22 (event-routing conformance). Depends on phase 18
(the write-path emission sites) and phase 14 (origin threading); mechanically
independent of phase 22. Covers R-QB5T-GLB6, R-QCDP-UD1V, R-QDLM-84SK,
R-QETI-LWJ9.*

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

- `internal/dropbox/events.go` replaces the `file.created`/`file.modified`/
  `file.deleted` type constants with kind constants `create`/`modify`/`delete`
  (`FileEvent.Kind`); `buildFilePayload` emits `outbox.Event{Kind, Subject:
  <the event's /-rooted display path>, Payload}`; the payload drops its
  `event` discriminator field and keeps exactly `path`, `rev`, `content_hash`,
  `size`, `content_url`, `origin`, `occurred_at`. Both emission paths (the
  download apply path and the D16 write path) and the D18 origin threading are
  otherwise untouched (same tx, `Ring()` after commit).
- `dropbox.Events` becomes an `outbox.Registry` of three `outbox.Family`
  entries (kinds `create`/`modify`/`delete`, subject description
  `/<mirror path>`, the same Sample payload struct minus its `event` field);
  `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with
  `bin/create-migration dropbox outbox_routing` (never a hand-picked number),
  drops and recreates the outbox table with the revised `outbox.SchemaSQL`
  verbatim. `003_outbox.sql` is byte-untouched; the DDL drift guard in
  `internal/db/migrations_outbox_test.go` re-points at the newest outbox
  migration.
- Existing sync/write/reflection tests conform their expectations to the new
  kinds (hard cutover — no `file.` event name survives in Go source).

**Done when:** the suite is green (design Conventions commands, from
`dropbox/`, plus `bin/check-migrations dropbox`) and:

- R-QB5T-GLB6, R-QCDP-UD1V, R-QDLM-84SK, and R-QETI-LWJ9 are each covered by a
  clearly-named test asserting the behavior its D22 Verification line states
  (kind/subject/payload-keys read back by SQL from real SQLite across the pull,
  write, and folder-delete-fan-out paths; the three-family reflection
  index/detail/unknown-kind with no `event` sample field; the fresh-DB
  migration column check + newest-migration DDL guard + frozen `003`; the
  real-`FeedHandler` frame `event: dropbox:create/notes/meeting.md` with no
  `type` key);
- `grep -rn "file\.created\|file\.modified\|file\.deleted" dropbox --include=*.go`
  (run from the repo root, or the equivalent from `dropbox/`) returns no
  matches in Go source;
- `git diff --stat -- dropbox/internal/db/migrations/003_outbox.sql` is empty
  (the frozen migration is untouched) and `bin/check-migrations dropbox`
  passes with the one new timestamped migration present.
