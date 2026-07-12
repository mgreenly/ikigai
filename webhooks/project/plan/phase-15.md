# Phase 15 — Event-routing conformance: kind `received`, subject `/<hook name>`, family registry, outbox migration

*Realizes design Decision 15 (event-routing conformance; D5 and D2 were
rewritten in place to the same addressing — their retained ids are re-proven by
the existing suites conforming). Depends on Phase 13 (a settled composition
root and `internal/db` shape). Covers R-A3FB-J3ZK, R-A4N7-WVQ9, R-A5V4-ANGY,
R-A730-OF7N.*

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

- `internal/webhooks/events.go` replaces the
  `eventWebhookReceived = "webhook.received"` type constant with
  `kindReceived = "received"` (still declared once, shared by the emit site
  and the registry); `Service.Record` appends
  `outbox.Event{Kind: kindReceived, Subject: "/" + wh.Name, Payload: raw}`
  with the payload shape unchanged (`name`, `owner`, `received_at`,
  `content_type`, `body`). The D5 seam is untouched: same single transaction
  as `TouchLastTriggered`, `Ring()` after commit, ingress `202` only after
  commit.
- `webhooks.Events` becomes an `outbox.Registry` of one `outbox.Family` entry
  (kind `received`, subject description `/<hook name>`, the same filled
  Sample); `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with
  `bin/create-migration webhooks outbox_routing` (never a hand-picked
  number), drops and recreates the outbox table with the revised
  `outbox.SchemaSQL` verbatim. `003_outbox.sql` is byte-untouched; the DDL
  drift guard in `internal/db/migrations_outbox_test.go` re-points at the
  newest outbox migration (retiring the old `003`-byte-equality assertion,
  whose id R-T2W7-WFN1 left design with the behavior).
- Existing domain/ingress/MCP/e2e tests conform their expectations to the new
  kind/subject (hard cutover — no `webhook.received` event name survives in
  Go source).

**Done when:** the suite is green (design Conventions commands, from
`webhooks/`) and:

- R-A3FB-J3ZK, R-A4N7-WVQ9, R-A5V4-ANGY, and R-A730-OF7N are each covered by
  a clearly-named test asserting the behavior its D15 Verification line
  states (kind `received` + subjects `/deploy-hook`/`/alpha_1` read back by
  SQL from real SQLite with the atomic touch and unchanged payload keys; the
  one-family reflection index/detail/unknown-kind through the assembled
  handler; the fresh-DB migration column check + newest-migration DDL guard +
  frozen `003`; the real-`FeedHandler` frame
  `event: webhooks:received/deploy-hook` with no `type` key);
- `grep -rn "webhook\.received" webhooks --include=*.go` (run from the repo
  root, or the equivalent from `webhooks/`) returns no matches in Go source;
- `git diff --stat -- webhooks/internal/db/migrations/003_outbox.sql` is
  empty (the frozen migration is untouched) and
  `ls webhooks/internal/db/migrations/ | grep -c "_outbox_routing\.sql$"`
  prints exactly `1` (the one new timestamped outbox migration present).
