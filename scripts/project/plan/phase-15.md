# Phase 15 — Event-routing conformance: triggers become canonical filter strings (trigger surface + consumer)

*Realizes design Decision 17 (trigger surface + consumer conformance); D11's and D13's in-place truth-ups (`"**"` in-edges, key-matched fan-out, filter-shaped trigger tools) land here. Depends on Phase 12. Covers R-7TR5-QSY4, R-7UZ2-4KOT, R-7W6Y-ICFI, R-7XEU-W467, R-7YMR-9VWW, R-7ZUN-NNNL, R-812K-1FEA.*

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

- One **new timestamped migration**, minted with `bin/create-migration scripts
  trigger_filters` (never a hand-picked number), drops and recreates
  `script_triggers` as `(script_id, source, filter, created_at)` (PK
  `(script_id, filter)`, index on `source`, FK/cascade preserved) and reshapes
  `runs`' trigger context to `trigger_kind` + `trigger_subject` (no
  `trigger_type`). No rows carried (migration window). `002_scripts.sql` is
  byte-untouched.
- `internal/script/trigger.go`: `knownProducers` + the cron special case +
  `globMatch`/`path.Match` are replaced by the static five-source
  `knownFamilies` kinds table (no `"scripts"` entry — self-chaining stays
  out), well-formedness validation (literal non-glob source segment before
  `:`, one of the five sources), and family validation via
  `outbox.Registry.CouldMatch`.
- `internal/script` model/store/service: `Trigger{ScriptID, Source, Filter,
  CreatedAt}` (Source derived from the filter), `SetTrigger`/`ClearTrigger`
  keyed by `(script_id, filter)` (owner scope and the silent no-op clear of an
  unheld filter preserved), `ScriptsForEvent(ctx, source, key)` matching
  stored filters against the canonical key with `routing.Match`,
  `RunForEvent(…, kind, subject, …)`, `Run{TriggerKind, TriggerSubject}` (JSON
  `trigger_kind`/`trigger_subject`), `FinishRunInput.TriggerKind`/
  `TriggerSubject` threaded by the runner's `finish` closure.
- `internal/consume`: poison check on empty `Kind`, lookup by `ev.Key()`, fire
  threads kind + subject, `Subscriptions` filter becomes `"**"`. The run input
  contract is unchanged (raw event payload on stdin/`$EVENT_JSON`; no envelope
  file exists in scripts).
- `internal/mcp`: `set_trigger`/`clear_trigger` take `(script_id, filter)`;
  their descriptions and the `describe` text state the canonical-key contract;
  domain errors keep the `isError` mapping.

## Done when

The suite is green (design *Conventions* commands, from `scripts/`) and:

- **R-7TR5-QSY4**, **R-7UZ2-4KOT**, **R-7W6Y-ICFI**, **R-7XEU-W467**,
  **R-7YMR-9VWW**, **R-7ZUN-NNNL**, **R-812K-1FEA** — each covered by a
  clearly-named test asserting exactly the behavior its D17 Verification line
  states, on the substrate that line names (real SQLite through the full
  embedded migration set, the real `eventplane/routing` matcher, the
  `consume.Handler` recording substrate, the assembled `appkit/mcp` handler).
- `grep -rn "event_filter\|knownProducers\|cron\.\*" scripts --include=*.go`
  (from the repo root, or the equivalent from `scripts/`) returns no matches
  in Go source (hard cutover — the pair model and the cron carve-out are gone).
- `git diff --stat -- scripts/internal/db/migrations/002_scripts.sql` is empty
  (the frozen migration is untouched) and exactly one new
  `*_trigger_filters.sql` timestamped migration is present.
- R-8WN1-0VQI and R-8XUX-ENH7 stay green on the conformed expectations
  (`"**"` in-edges; key-matched fan-out through the handler factory).
