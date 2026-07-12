# Phase 1 — The `routing` package: key, validity, matcher

*Realizes design Decision 2 (routing package). Depends on nothing — first
phase of the revision.*

Create the new package `eventplane/routing` with the four exported functions
of D2 — `Key`, `Match`, `ValidKind`, `ValidSubject` — the hand-rolled matcher
implementing the pinned glob dialect (`*` segment-local, `**` cross-segment,
`?`, character classes, braces literal, whole-key anchoring, error on
malformed pattern), and the exhaustive table-driven matcher test that defines
the dialect. No third-party dependency: `go.mod` gains no new `require`. End
state: any package in the module (and any suite service) can render a
canonical key and evaluate a filter through this one seam.

**Done when:**

- R-3FIX-KJG7 — `Key` renders the subjectless and subject-ful exact strings.
- R-3GQT-YB6W — `*` never crosses `/` (positive and negative cases).
- R-3HYQ-C2XL — `**` crosses segments (positive and negative cases).
- R-3J6M-PUOA — matching is anchored to the whole key (no prefix/suffix
  elision).
- R-3KEJ-3MEZ — `?` single non-`/` char; character classes; braces literal.
- R-3LMF-HE5O — malformed pattern returns a non-nil error.
- R-3MUB-V5WD — literal precision; `dropbox:*` matches the subjectless key
  only.
- R-41H4-GESP — `ValidKind` / `ValidSubject` truth tables.

Each id is covered by a test citing it by name or adjacent comment;
`go test ./...` and `go vet ./...` from `eventplane/` exit 0; `git diff
--stat` shows no change to `go.mod`'s `require` set.
