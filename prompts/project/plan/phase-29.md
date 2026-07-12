# Phase 29 — Event-routing conformance: producer kinds `run.succeeded`/`run.failed`, subject = /<prompt name>, family registry, outbox migration

*Realizes design Decision 25 (producer conformance). Depends on Phase 28 (the reshaped `runs` trigger context the outcome payload reads). Covers R-6T4Y-E1VD, R-6UCU-RTM2, R-6VKR-5LCR, R-ZS8A-TVOF.*

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

- `internal/prompt/outcome.go`: `EventRunSucceeded`/`EventRunFailed` become
  **kind** constants (same string values); `outcomeEvent` emits
  `outbox.Event{Kind, Subject, Payload}` with subject `""` for a nameless
  prompt, else `"/" + prompt_name` (newlines → spaces); `outcomePayload`
  carries `trigger_kind`/`trigger_subject` (no `trigger_type`); the same-tx
  atomicity, at-most-once, and cancelled-emits-nothing rules are untouched.
- `prompt.Events` becomes an `outbox.Registry` of two `outbox.Family` entries
  (kinds `run.succeeded`/`run.failed`, subject description `/<prompt name>`,
  updated Samples); `Spec.Events` wiring unchanged.
- One **new timestamped migration**, minted with `bin/create-migration prompts
  outbox_routing` (never a hand-picked number), drops and recreates the outbox
  table with the revised `outbox.SchemaSQL` verbatim. `005_outbox.sql` is
  byte-untouched; the DDL drift guard in
  `internal/db/migrations_outbox_test.go` re-points at the newest outbox
  migration.
- Existing outcome/reflection tests conform their expectations to the new
  kind/subject shape (hard cutover — no envelope `type` survives in Go source).

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-6T4Y-E1VD**, **R-6UCU-RTM2**, **R-6VKR-5LCR**, **R-ZS8A-TVOF** — each
  covered by a clearly-named test asserting exactly the behavior its D25
  Verification line states (kind/subject/payload-keys read back by SQL from
  real SQLite across succeeded/failed/cancelled and the nameless-prompt case;
  the two-family reflection index/detail/unknown-kind; the fresh-DB migration
  column check + newest-migration DDL guard + frozen `005`; the
  real-`FeedHandler` frame `event: prompts:run.succeeded/collect-bills` with
  no `type` key).
- `git diff --stat -- prompts/internal/db/migrations/005_outbox.sql` is empty
  (the frozen migration is untouched) and exactly one new
  `*_outbox_routing.sql` timestamped migration is present.
- R-DLYM-4QUF stays green on the conformed reflection expectations (two
  families + `"**"` subscribes).
