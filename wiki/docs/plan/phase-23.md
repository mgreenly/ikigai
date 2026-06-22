# Phase 23 — MCP surface: public paths out, no internal ULIDs

*Realizes design Decision 10 (the MCP tool surface). Depends on Phase 19 (D11 `Path`/`GetByPath`), Phase 20 (D12 link footer), and Phase 22 (page path input).*

Phase 22 converted the `page` *input* and (with Phase 21) the `ask` citations to
`type/slug` paths, but the read tools still serialize raw domain structs — so
`subjects`, `claims`, `page`, and `status` leak internal ULIDs and `claims` is
still keyed by `subject_id`. This phase finishes D10's **"paths in, paths out —
the internal ULID never crosses the MCP boundary"** across the whole surface.

Observable end state — every `tools/call` result carries only its
design-specified public shape, and no internal subject ULID appears anywhere:

- **`subjects`** → each row is `{path, type, name, has_page}`; no internal id
  (no ULID, no `norm_name`).
- **`claims`** → input `{subject}` is a **path**, resolved via `GetByPath`
  (`ErrSubjectNotFound` → clean not-found result; `ErrAmbiguousPath` → clean tool
  error). Result is `[{id, text, job}]` — claim id and job id only; no
  `subject_id` / subject ULID. The `subject_id` input is gone.
- **`page`** → result is `{subject, title, body}` — `subject` is the requested
  path, `body` carries the D12 footer; no page or subject ULID.
- **`status`** → a finished job's produced subjects are `type/slug` paths, not
  internal ULIDs.
- **One path-based wiring** → `wiki.Spec()` (the non-`serve` verbs) and the
  `serve` composition root advertise and serve the **identical** path surface;
  the legacy `subject_id` options (`WithPageService`, the subject-id
  `WithClaimsService`) are removed in favor of path-based wiring.

The surface stays exactly eight verbs (D10). Result shaping and path projection
live in the `cmd/wiki/main.go` composition-root adapters (the established
`pathPageService` pattern); `status` subjects are projected ULID→path at the
adapter, so `internal/wiki` is unchanged. The package built is `internal/mcp`
plus its `cmd/wiki` wiring.

**Done when:** R-01OQ-Y5YV, R-02WN-BXPK, R-044J-PPG9, R-03GW-PX5K, and
R-04HB-QM7T are each covered by a clearly-named test that drives `tools/call` and
asserts the serialized JSON field names **and** the absence of any internal ULID
in the `subjects`, `claims`, `page`, and `status` results; both MCP wirings
expose an identical tool surface; and the suite is green (per design
*Conventions*: `go build ./...`, `go vet ./...`, `gofmt -l .`, `go test ./...`,
`bin/check-migrations wiki`).
