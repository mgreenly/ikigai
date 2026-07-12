# Phase 2 — Outbox envelope and wire cutover

*Realizes design Decision 1 (envelope and wire cutover). Depends on Phase 1.*

Revise package `outbox` to the new addressing: `Event{Kind, Subject, Payload}`
replaces `Event{Type, Payload}`; `Append` validates the address via
`routing.ValidKind`/`routing.ValidSubject` and inserts into the revised
`SchemaSQL` table (`kind` + `subject` columns, `type` gone); the wire envelope
becomes `{id, source, time, kind, subject, payload}`; the SSE `event:` line
carries `routing.Key(source, kind, subject)`. Update existing outbox tests to
the new shapes (the baseline behaviors they pin — atomicity, ordering,
backpressure, retention, resync — must stay green, unchanged). Note: the
in-repo `consumer` package compiles against the wire format; if its tests
cannot pass until Phase 4's surface change, land the minimal consumer
compile-fix in this phase without claiming Phase 4's ids. End state: the
producer half speaks only the new format.

**Done when:**

- R-39FF-NOQQ — `Append` rejects invalid kinds, accepts conforming ones.
- R-3ANC-1GHF — `Append` rejects non-rooted subjects, accepts empty and
  `/`-rooted.
- R-3BV8-F884 — revised `SchemaSQL` applies on real SQLite; `kind`/`subject`
  columns present, no `type` column; appended values read back by SQL.
- R-3D34-SZYT — a real-FeedHandler frame's `data:` JSON has exactly the keys
  `id, source, time, kind, subject, payload` and no `type` key.
- R-3EB1-6RPI — the frame's `event:` line is the canonical key (subject-ful
  and subjectless cases).
- R-42P0-U6JE — a replayed event's frame is byte-identical across two real
  feed connections.

Each id is covered by a test citing it; `go test ./...` and `go vet ./...`
from `eventplane/` exit 0; `grep -n 'type TEXT' outbox/schema.go` and
`grep -n 'json:"type"' outbox/*.go` (run from `eventplane/`) both print
nothing.
