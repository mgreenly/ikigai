# Phase 46 — Identity == path token: Path/GetByPath direct, delete slug + ErrAmbiguousPath

*Realizes design Decision 11 (subject addressing), with the surface fallout in Decision 10 (MCP not-found) and Decision 27 (merge errors). Depends on Phase 45.*

`NormName` is now the canonical `[a-z0-9-]` path token (D3/Phase 45), so the
separate slug projection — and the ambiguity it created — are removed. Because
the changes below straddle `internal/wiki` and `internal/mcp` (the latter won't
compile while it still references a deleted `ErrAmbiguousPath`), they land as one
phase so the suite ends green.

- In `internal/wiki` (`data_model.go`): `Path(s)` returns `s.Type + "/" + s.NormName`
  with no further transform; the `slug()` function is deleted; `GetByPath` parses
  `type/normName`, does a direct indexed `GetByNormName` exact match, and returns
  the subject only if its `Type` matches the path's type (else
  `ErrSubjectNotFound`). The former scan-over-all-subjects-of-a-type comparing
  `slug(NormName)`, the `ErrAmbiguousPath` sentinel, and its `errors.New`
  declaration are all deleted — `norm_name` is `UNIQUE`, so a path names at most
  one subject and ambiguity is impossible.
- In `internal/mcp` (`mcp.go`): `pathField` builds the path as `typ + "/" + normName`
  directly, and the duplicate `slug()` function is deleted. (This package builds
  paths by reflection and shares no import with `internal/wiki`; the change is a
  one-call-site edit plus the function deletion.)
- In `internal/mcp` — remove the now-dead `ErrAmbiguousPath` handling from the
  `page`/`claims` resolver (D10) and from the `merge` handler (D27): the only
  resolution miss is `ErrSubjectNotFound` → a clean not-found result. `merge`
  keeps exactly two error channels (not-found, same-id); its ambiguous-path
  branch and the corresponding assertion in the R-E01B test are deleted.

Display continues to use the original `Name`; nothing reconstructs a name from
the path. No schema migration (the `norm_name` column/UNIQUE already exist; only
the values change, and dev data is disposable per design Conventions).

**Done when:** R-DRX6-PWSW (`Path` joins `Type` and `NormName` with one `/`, no
transform), R-DT53-3OJL (`GetByPath` direct `norm_name` match with type check;
wrong-type token → `ErrSubjectNotFound`), and R-DUCZ-HGAA (`GetByPath` →
`ErrSubjectNotFound` for an unknown or empty token) each have a clearly-named
test; no `slug` identifier and no `ErrAmbiguousPath` reference remains in either
package; the updated R-E01B (D27) asserts the two surviving merge error channels;
and the suite is green.
