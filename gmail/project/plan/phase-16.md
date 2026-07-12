# Phase 16 — Event-routing conformance: kinds `received`/`sent`/`deleted`, family registry, outbox migration

*Realizes design Decision 18 (event-routing conformance). Depends on Phase 15
(a settled `main.go`/`internal/mcp`; mechanically independent otherwise).
Covers `R-X6YL-1Y77`, `R-X86H-FPXW`, `R-X9ED-THOL`, `R-XAMA-79FA`.*

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

- `internal/gmail/events.go` + `sync.go` and `internal/mcp/tools.go` replace
  the `mail.received`/`mail.sent`/`mail.deleted` type constants with kind
  constants `received`/`sent`/`deleted`; `buildPayload` emits
  `outbox.Event{Kind: …, Subject: "", Payload: …}` with payload shapes
  unchanged; the engine/EventSink seam (same tx as the cursor advance,
  `Ring()` after commit) is untouched.
- `mcp.Events` becomes an `outbox.Registry` of three `outbox.Family` entries
  (kinds `received`/`sent`/`deleted`, empty subject descriptions, the same
  Sample payload structs); `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with
  `bin/create-migration gmail outbox_routing` (never a hand-picked number),
  drops and recreates the outbox table with the revised `outbox.SchemaSQL`
  verbatim. `003_outbox.sql` is byte-untouched; the DDL drift guard in
  `internal/db/migrations_outbox_test.go` re-points at the newest outbox
  migration.
- Existing sync/reflection tests conform their expectations to the new kinds
  (hard cutover — no `mail.` prefix survives outside frozen plan/migration
  history).

**Done when:** the suite is green (design Conventions commands, from `gmail/`)
and:

- R-X6YL-1Y77, R-X86H-FPXW, R-X9ED-THOL, and R-XAMA-79FA are each covered by a
  clearly-named test asserting the behavior its D18 Verification line states
  (kind/subject read back by SQL from real SQLite; the three-family reflection
  index/detail/unknown-kind; the fresh-DB migration column check + newest-
  migration DDL guard; the real-`FeedHandler` frame `event: gmail:received`
  with no `type` key);
- `grep -rn "mail\.received\|mail\.sent\|mail\.deleted" gmail --include=*.go`
  (run from the repo root, or the equivalent from `gmail/`) returns no matches
  in Go source;
- `git diff --stat -- gmail/internal/db/migrations/003_outbox.sql` is empty
  (the frozen migration is untouched) and the one new timestamped migration
  is present.
