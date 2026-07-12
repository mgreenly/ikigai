# Phase 28 — Event-routing conformance: triggers become canonical filter strings (trigger surface + consumer)

*Realizes design Decision 24 (trigger surface + consumer conformance); D15's and D17's in-place truth-ups (`"**"` in-edges, filter-shaped trigger tools) land here. Depends on Phase 27. Covers R-6JDR-BVXT, R-6KLN-PNOI, R-6LTK-3FF7, R-6N1G-H75W, R-6O9C-UYWL, R-6PH9-8QNA, R-6QP5-MIDZ, R-6RX2-0A4O.*

> **⛔ EXTERNAL ORDERING — operator-sequenced.** This phase consumes the
> **revised eventplane API** (`routing.Key`/`Match`/`ValidKind`/`ValidSubject`,
> `outbox.Family`/`Registry.CouldMatch`, the kind/subject envelope, and
> `consumer.Event{Kind, Subject}` + `Event.Key()`) specified in
> `eventplane/project/design/` and built by **eventplane plan phases 01–04 —
> those must be BUILT (green in `eventplane/`) before this phase runs**, and
> appkit must compile against that revision (the chassis consumer engine,
> `Spec.Events outbox.Registry`, the reflection tool). Cross-module building is
> sequenced by the operator, not by this plan; if the eventplane revision is
> not yet in the tree, this phase cannot build and must be left `⬜`.

Observable end state:

- One **new timestamped migration**, minted with `bin/create-migration prompts
  trigger_filters` (never a hand-picked number), drops and recreates
  `prompt_triggers` as `(prompt_id, source, filter, created_at)` (PK
  `(prompt_id, filter)`, index on `source`) and reshapes `runs`' trigger
  context to `trigger_kind` + `trigger_subject` (no `trigger_type`). No rows
  carried (migration window). `006_prompt_redesign.sql` is byte-untouched.
- `internal/prompt/trigger.go`: `knownProducers` + the cron special case +
  `globMatch`/`path.Match` are replaced by the static `knownFamilies` kinds
  table, well-formedness validation (literal non-glob source segment before
  `:`, one of the six sources), family validation via
  `outbox.Registry.CouldMatch`, and `PromptsForEvent(ctx, source, key)`
  matching stored filters against the canonical key with `routing.Match`.
- `internal/prompt` model/service/store: `Trigger{PromptID, Source, Filter}`
  (Source derived from the filter), `RunByEvent(…, kind, subject, …)`,
  `startRun`/`materializeInput` write the `{source, kind, subject, event_id,
  payload}` envelope to `input/event.json`, `Run{TriggerKind, TriggerSubject}`.
- `internal/consume`: poison check on empty `Kind`, lookup by `ev.Key()`, fire
  threads kind + subject, `Subscriptions` filter becomes `"**"`. `eventPreamble`
  unchanged (grounded: it names no envelope field except `payload`).
- `internal/mcp`: `set_trigger`/`clear_trigger` take `(prompt_id, filter)`,
  `create`'s inline `triggers` is an array of filter strings, descriptions and
  the `describe` text state the canonical-key contract and the new envelope
  shape; domain errors keep the `isError` mapping.

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-6JDR-BVXT**, **R-6KLN-PNOI**, **R-6LTK-3FF7**, **R-6N1G-H75W**,
  **R-6O9C-UYWL**, **R-6PH9-8QNA**, **R-6QP5-MIDZ**, **R-6RX2-0A4O** — each
  covered by a clearly-named test asserting exactly the behavior its D24
  Verification line states, on the substrate that line names (real SQLite
  through the full embedded migration set, the real `eventplane/routing`
  matcher, the real `prompt.Service` + stubbed runner, the assembled
  `appkit/mcp` handler).
- `grep -rn "event_filter\|knownProducers\|cron\.\*" prompts --include=*.go`
  (from the repo root, or the equivalent from `prompts/`) returns no matches
  in Go source (hard cutover — the pair model and the cron carve-out are gone).
- `git diff --stat -- prompts/internal/db/migrations/006_prompt_redesign.sql`
  is empty (the frozen migration is untouched) and exactly one new
  `*_trigger_filters.sql` timestamped migration is present.
- R-DFV4-7W4Y and R-DH30-LNVN stay green on the conformed expectations
  (`"**"` in-edges; key-matched fan-out).
