# Phase 12 — `occurred_at` is an optional, format-validated field on every subject type

*Realizes design Decision 6 (the extract stage `internal/extract`), covering the new ids R-XJBY-H8JZ and R-XKJU-V0AO and retiring the reversed behavior R-W01W-PH1M. Depends on Phase 04 (the extract stage and its `validate` hook).*

Phase 04 built the extract stage with an `occurred_at` rule of "events only":
the validator (`validateSubject`) rejects any non-empty `OccurredAt` carried by a
non-`event` subject with `subjects[i].occurred_at must be empty unless type is
event`. But the model correctly and consistently extracts a subject's defining
date regardless of type — a person's birth year, a work's publication year, an
organization's founding year — so this rule fails whole ingests
**deterministically**: the "Gary Gygax" document, where every `entity` subject
carries a year, dies in extract on `subjects[0]` and no number of re-prompts can
save it (the model will emit the same correct shape every time). D6 has been
re-decided: `occurred_at` is now an **optional, format-validated ISO-8601 prefix
on every subject type**, required only for events, retained as extracted and
never coerced or rejected for being present on a non-event. This phase realigns
`internal/extract` to that contract.

**What gets built (the observable end state):**

- `validateSubject` no longer rejects a non-empty `OccurredAt` on an `entity` or
  `concept` subject. Instead, **whenever `OccurredAt` is non-empty (any type)**,
  it must be a valid ISO-8601 prefix (`YYYY` / `YYYY-MM` / `YYYY-MM-DD`); a
  malformed value is a parse-level failure → bounded re-prompt.
- For `Type=="event"`, `OccurredAt` remains **required** (non-empty, valid
  prefix) — unchanged from Phase 04.
- A valid extraction **retains** `OccurredAt` exactly as extracted on every
  subject type; nothing is coerced to `""` and nothing is dropped. A stated date
  on a non-event subject survives into the durable record.
- The extract prompt description reflects the new contract — `occurred_at` is
  welcome on any subject for which the document states a defining date — so the
  model is not steered to omit those dates.
- The reversed-behavior test for R-W01W-PH1M (asserting `OccurredAt` is emptied
  for non-events) is **removed** along with its id; it no longer describes the
  design.
- End-to-end, the "Gary Gygax" document (every entity carrying a year) extracts
  successfully rather than failing the job.

**Done when:**

- R-XJBY-H8JZ — a test asserts a valid extraction retains a non-empty
  `OccurredAt` on a non-`event` subject (`entity`/`concept`) exactly as
  extracted — the date is never emptied, dropped, or rejected for being present
  on a non-event.
- R-XKJU-V0AO — a test asserts `validate` rejects a non-empty `OccurredAt` that
  is not a valid ISO-8601 prefix on any type, rejects an `event` whose
  `OccurredAt` is empty, and accepts a valid prefix on `entity`, `concept`, and
  `event` (parse-level failures trigger a re-prompt).
- The remaining D6 ids stay green — R-VYU0-BPAX (closed-set type rejection),
  R-W19T-38SB (honest-empty), and R-W2HP-H0J0 (deterministic call site) — so the
  change narrows the `occurred_at` rule without disturbing the rest of D6.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki`).
