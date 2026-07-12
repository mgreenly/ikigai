# Phase 3 — Producer families: registry, reflection, validation

*Realizes design Decision 3 (producer families). Depends on Phase 2.*

Revise the producer registry in package `outbox` from
`EventType{Type, Description, Sample}` to `Family{Kind, Subject, Description,
Sample}`: `Index()` renders `{kind, subject, description}` per family in
declared order; `Detail(kind)` keeps the no-drift schema-from-type /
example-from-value property and returns `*UnknownKindError` (replacing
`UnknownEventTypeError`) for an undeclared kind; `Append` gates on declared
kinds when the registry is non-empty (subjects ungated); and
`Registry.CouldMatch(source, filter)` answers the filter-vs-families question
via the `routing` matcher — kind intersected, subject existentially open (the
intersection primitive may live in `routing`). The reflection machinery
(`schema`, `exampleOf`) is reused, not rewritten. End state: a producer
declares families, reflection describes them, and a selector-validating
service has its one helper.

**Done when:**

- R-3O28-8XN2 — `Append` rejects an undeclared kind (error names declared
  kinds), accepts a declared one; empty registry stays ungated.
- R-3QI1-0H4G — `Index()` yields `{kind, subject, description}` per family,
  in order.
- R-3RPX-E8V5 — `Detail` schema and example agree; unknown kind yields
  `*UnknownKindError` with the declared kinds.
- R-3SXT-S0LU — `CouldMatch` true/false on kind intersection; malformed
  filter returns an error.
- R-3U5Q-5SCJ — subject patterns never cause `CouldMatch` rejection.

Each id is covered by a test citing it; `go test ./...` and `go vet ./...`
from `eventplane/` exit 0; `grep -rn 'EventType' outbox/*.go` (run from
`eventplane/`) prints nothing.
