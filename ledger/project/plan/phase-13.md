# Phase 13 — Event-routing conformance: kind `recorded`, empty subject, family registry, outbox migration

*Realizes design Decision 15 (event-routing conformance). Depends on Phase 12
(the event payload's `external_ref` field feeds the registry Sample and the
reflection assertions). Covers R-FXKF-JD3L, R-FYSB-X4UA, R-G184-OOBO,
R-G2G1-2G2D.*

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

- `internal/ledger/events.go` replaces the
  `eventTransactionRecorded = "transaction.recorded"` type constant with
  `kindRecorded = "recorded"` (still declared once, shared by the emit site
  and the registry); `transactionRecordedEvent` returns
  `outbox.Event{Kind: kindRecorded, Subject: "", Payload: raw}` with the
  payload shape unchanged (including Phase 12's `external_ref`). The
  `Service`/`EventSink` seam is untouched (same tx via `persist`, one event
  per committed transaction — reversal mirrors included — `Ring()` after
  commit).
- `ledger.Events` becomes an `outbox.Registry` of one `outbox.Family` entry
  (kind `recorded`, empty subject, the same `sampleTransactionRecorded`
  Sample); `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with
  `bin/create-migration ledger outbox_routing` (never a hand-picked number),
  drops and recreates the outbox table with the revised `outbox.SchemaSQL`
  verbatim. `003_outbox.sql` is byte-untouched; the DDL drift guard in
  `internal/db/migrations_outbox_test.go` re-points at the newest outbox
  migration.
- Existing domain/reflection tests conform their expectations to the new kind
  (hard cutover — no `transaction.recorded` event name survives in Go
  source).

**Done when:** the suite is green (design Conventions commands, from
`ledger/`, plus `bin/check-migrations ledger` per the plan done bar) and:

- R-FXKF-JD3L, R-FYSB-X4UA, R-G184-OOBO, and R-G2G1-2G2D are each covered by
  a clearly-named test asserting the behavior its D15 Verification line
  states (kind `recorded`/subject `''` read back by SQL from real SQLite
  after a `Record` and a `Reverse`, atomicity preserved; the one-family
  reflection index/detail/unknown-kind through the assembled handler; the
  fresh-DB migration column check + newest-migration DDL guard + frozen
  `003`; the real-`FeedHandler` frame `event: ledger:recorded` with no
  `type` key);
- `grep -rn "transaction\.recorded" ledger --include=*.go` (run from the repo
  root, or the equivalent from `ledger/`) returns no matches in Go source;
- `git diff --stat -- ledger/internal/db/migrations/003_outbox.sql` is empty
  (the frozen migration is untouched) and `bin/check-migrations ledger`
  passes with the one new timestamped outbox migration present.
