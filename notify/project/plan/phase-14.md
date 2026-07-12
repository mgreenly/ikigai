# Phase 14 — Event-routing conformance (consumer side): canonical-key filters, kind/subject matching

*Realizes design Decision 16 (event-routing conformance, consumer side).
Depends on Phase 12 (the converted chassis composition root). Covers
R-ZCGU-FG9L, R-ZEWN-6ZQZ, R-ZG4J-KRHO, R-ZHCF-YJ8D.*

> **⛔ EXTERNAL ORDERING — operator-sequenced.** This phase consumes the
> **revised eventplane API** (`consumer.Event{Kind, Subject}` + `Key()` with
> `Type` deleted, `eventplane/routing.Match`, the kind/subject envelope +
> canonical-key SSE framing) specified in `eventplane/project/design/` and
> built by **eventplane plan phases 01–04 — those must be BUILT (green in
> `eventplane/`) before this phase runs**, and appkit must compile against
> that revision (the chassis consumer engine behind `Spec.Consumers` and the
> reflection `subscribes` rendering). It also depends on **crm's and prompts'
> producer conformance being built** for the end-to-end chain to be true on a
> running suite — crm emitting `crm:contact.created/<contact-id>` and prompts
> emitting `prompts:run.succeeded|run.failed/<prompt name>` (prompts D25);
> notify's own tests append those keys locally and do not wait on the
> upstream builds. Cross-module building is sequenced by the operator, not by
> this plan; if the eventplane revision is not yet in the tree, this phase
> cannot build and must be left `⬜`.

Observable end state:

- `internal/push/push.go`: `push.Subscription()` declares
  `{Source: "crm", Filter: "crm:contact.created/**"}`; the crm handler
  matches the declared filter against the event's canonical key with
  `routing.Match(sub.Filter, ev.Key())` (a `(false, err)` result is a
  non-match), and its decode error keys off `ev.Kind`/`ev.ID`. Payload
  handling, the async best-effort push, and `ErrSkip`-on-poison are
  untouched.
- `internal/push/prompts.go`: the `eventRunSucceeded`/`eventRunFailed`
  constants keep their string values as **kinds**; the handler classifies
  with `switch ev.Kind` (kind-exact, subject-agnostic — a nameless prompt's
  subjectless outcome still pushes); `PromptsSubscriptions()` declares
  Filters `"prompts:" + eventRunSucceeded + "/**"` and
  `"prompts:" + eventRunFailed + "/**"`, Descriptions stating the push fires
  for any prompt run's outcome, named or not.
- No call to `consumer.Subscription.Match` survives in notify's source; no
  reference to the deleted `Type` field remains (compile-enforced).
- No producer side appears: no `Spec.Feed`/`Spec.Events`/`Producer`, and
  **no new migration** — the embedded set stays exactly
  `001_schema_migrations.sql` + `002_feed_offset.sql` (`consumer.SchemaSQL`
  is unchanged by the revision; the byte-equality guard stands).
- Existing tests conform on their existing substrates: handler tests feed
  `consumer.Event{Kind, Subject, Payload}`; the integration/e2e tests append
  `outbox.Event{Kind, Subject, Payload}` through the revised outbox + real
  `FeedHandler`; the Spec-level D11 assertions (R-4DG9-3Q97, R-4EO5-HHZW)
  keep passing against the conformed subscription lists.

**Done when:** the suite is green (design Conventions commands from `notify/`,
plus `bin/check-migrations notify` per the plan done bar) and:

- R-ZCGU-FG9L, R-ZEWN-6ZQZ, R-ZG4J-KRHO, and R-ZHCF-YJ8D are each covered by a
  clearly-named test asserting the behavior its D16 Verification line states
  (the crm declared-filter match via the real routing matcher + mock-ntfy
  POST/no-POST; the prompts kind-exact classification including the
  subjectless nameless-run push and the `run.cancelled` non-push; the
  end-to-end revised-outbox → real-`FeedHandler` → `consumer.Run` → mock-ntfy
  chain; the exactly-two-migrations set with `002_feed_offset.sql`
  byte-identical to `consumer.SchemaSQL`);
- `grep -rn "ev\.Type\|Subscription.Match\|path\.Match" notify --include=*.go`
  (run from the repo root, or the equivalent from `notify/`) returns no
  matches in notify's Go source;
- `ls notify/internal/db/migrations/` lists exactly
  `001_schema_migrations.sql` and `002_feed_offset.sql` (no new migration).
