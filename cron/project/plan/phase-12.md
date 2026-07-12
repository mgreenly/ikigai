# Phase 12 — Event-routing conformance: kind `tick` + subject `/<schedule name>`, live one-family reflection, outbox migration

*Realizes design Decision 14 (event-routing conformance). Depends on Phase 10
(a settled `cmd/cron/main.go`/`internal/mcp`/`internal/db`; mechanically
independent otherwise). Covers `R-PQH6-2RYI`, `R-PRP2-GJP7`, `R-PSWY-UBFW`,
`R-PU4V-836L`, `R-PVCR-LUXA`.*

> **⛔ EXTERNAL ORDERING — operator-sequenced.** This phase consumes the
> **revised eventplane API** (`outbox.Event{Kind, Subject}`, `outbox.Family`/
> `Registry`, the kind/subject envelope + canonical-key SSE framing,
> `routing.ValidSubject`/`Match`) specified in `eventplane/project/design/`
> and built by **eventplane plan phases 01–04 — those must be BUILT (green in
> `eventplane/`) before this phase runs**, and appkit must compile against
> that revision (`Spec.Publishes func() outbox.Registry` and the chassis
> reflection tool). Cross-module building is sequenced by the operator, not by
> this plan; if the eventplane revision is not yet in the tree, this phase
> cannot build and must be left `⬜`.

Observable end state:

- `internal/event/event.go` replaces `TypePrefix`/`Type(name)` with
  `const Kind = "tick"` and `Subject(name) = "/" + name`; `Build` emits
  `outbox.Event{Kind: Kind, Subject: Subject(name), Payload: …}` with the
  `{name, scheduled_for, fired_at}` payload unchanged. The tick seam
  (`internal/tick`) is untouched in behavior: same per-schedule tx appending
  the event with the `last_slot` advance, `Ring()` after the scan, the
  (schedule, slot) at-most-once guard preserved.
- `event.Publishes(store)` stays a LIVE provider but returns a one-family
  `outbox.Registry` — `{Kind: "tick", Subject: "/<schedule name>",
  Description: … with the live schedule names enumerated at reflection time,
  Sample: the shared payload sample}` — wired through `Spec.Publishes` /
  `rt.Publishes()` exactly as today; `Spec.Events` stays empty (no Append-time
  family gate; the crontab name CHECK remains the subject boundary).
- One **new timestamped migration**, minted with
  `bin/create-migration cron outbox_routing` (never a hand-picked number),
  drops and recreates the outbox table with the revised `outbox.SchemaSQL`
  verbatim. `003_outbox.sql` is byte-untouched; the DDL drift guard in
  `internal/db/migrations_outbox_test.go` re-points at the newest outbox
  migration.
- Wire-visible prose conforms (D14 item 5): the `internal/mcp` instructions
  and tool descriptions name `cron:tick/<name>` instead of `cron.<name>`
  (schemas, error envelopes, and the seven-tool surface unchanged); the
  doctrine headers in `cmd/cron/main.go`, `internal/event`, `internal/tick`,
  and `internal/crontab` are trued to the kind/subject shape.
- Existing event/tick/mcp tests conform their expectations to the new
  addressing (hard cutover — no `cron.<name>`/`"cron."` reference survives in
  Go source; frozen plan/migration history keeps its wording).

**Done when:** the suite is green (design Conventions commands, from `cron/`,
plus `bin/check-migrations cron` per the plan done bar) and:

- R-PQH6-2RYI, R-PRP2-GJP7, R-PSWY-UBFW, R-PU4V-836L, and R-PVCR-LUXA are each
  covered by a clearly-named test asserting the behavior its D14 Verification
  line states (kind/subject rows read back by SQL from real SQLite with the
  at-most-once guard intact; the live one-family reflection through the
  assembled MCP handler tracking create/delete; the fresh-DB migration column
  check + newest-migration DDL guard with `003_outbox.sql` frozen; the real
  `FeedHandler` frame `event: cron:tick/bill-sweep` with no `type` key; the
  `ValidSubject`/literal-`Match` subject-safety checks);
- `grep -rn '"cron\."\|cron\.<name>\|TypePrefix' cron --include='*.go'` (run
  from the repo root, or the equivalent from `cron/`) returns no matches in Go
  source;
- `git diff --stat -- cron/internal/db/migrations/003_outbox.sql` is empty
  (the frozen migration is untouched) and `bin/check-migrations cron` passes
  with the one new timestamped migration present.
