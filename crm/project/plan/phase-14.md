# Phase 14 — Event-routing conformance: kinds keep `contact.*`, subject = `/<contact id>`, family registry, outbox migration

*Realizes design Decision 18 (event-routing conformance). Depends on Phase 12
(a settled composition root and registry-adopted tests; mechanically
independent otherwise). Covers `R-8HHB-24SG`, `R-8IP7-FWJ5`, `R-8JX3-TO9U`,
`R-8L50-7G0J`.*

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

- `internal/crm/events.go` keeps the four constants' string values —
  `contact.created`/`contact.updated`/`contact.tagged`/`contact.untagged` are
  now **kind** constants (the entity noun is domain information; source `crm`
  is multi-entity) — and the event builders emit
  `outbox.Event{Kind: …, Subject: "/" + <contact ULID>, Payload: …}`:
  `contactEvents` subjects the snapshot event with the saved contact's id,
  `contactTagEvent` subjects each tag delta with the contact's id. Payload
  shapes unchanged; the `Service`/outbox seam (same tx as the domain write,
  `Ring()` after commit, tag diff off the `Summary` side-band) is untouched.
- `crm.Events` becomes an `outbox.Registry` of four `outbox.Family` entries
  (the four `contact.*` kinds, subject description `/<contact id>` on each,
  the same Sample payload values); `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with
  `bin/create-migration crm outbox_routing` (never a hand-picked number),
  drops and recreates the outbox table with the revised `outbox.SchemaSQL`
  verbatim. `003_outbox.sql` is byte-untouched; the DDL drift guard in
  `internal/db/migrations_outbox_test.go` re-points at the newest outbox
  migration.
- Existing events/service/reflection tests conform their expectations to the
  kind/subject shape (hard cutover). Note the kind **strings do not change**
  — unlike gmail/dropbox there is no old-prefix grep to run; the cutover is
  the `Type` → `Kind`+`Subject` field shape and the family registry.

**Done when:** the suite is green (design Conventions commands, from `crm/`)
and:

- R-8HHB-24SG, R-8IP7-FWJ5, R-8JX3-TO9U, and R-8L50-7G0J are each covered by a
  clearly-named test asserting the behavior its D18 Verification line states
  (kind + `/`-rooted contact-id subject read back by SQL from real SQLite with
  payload shapes unchanged; the four-family reflection index/detail/unknown-
  kind; the fresh-DB migration column check + newest-migration DDL guard; the
  real-`FeedHandler` frame `event: crm:contact.created/<contact ULID>` with
  no `type` key);
- `git diff --stat -- crm/internal/db/migrations/003_outbox.sql` is empty
  (the frozen migration is untouched) and the one new timestamped outbox
  migration is present.
