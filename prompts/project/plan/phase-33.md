# Phase 33 — Error taxonomy: `too_large`, `source_unavailable`, and sandbox not-found/validation codes

*Realizes design Decision 27 (Structured MCP adoption) — slice: the failure
modes the closed vocabulary names but today's domain error surface cannot yet
distinguish. Depends on Phase 32 (the `codeFor` classifier and the coded
`ErrorResult` sites it extends).*

This phase widens prompts' domain error surface so the MCP mapping stops
collapsing distinct failures into `validation`/`internal`:

- **`internal/prompt/model.go`** — two new sentinels, `prompt.ErrTooLarge` and
  `prompt.ErrSourceUnavailable`.
- **`internal/prompt/service.go`** — `Import`'s over-1-MiB branch wraps
  `ErrTooLarge` (was `ErrValidation`); `RunFsList`/`RunFsRead` translate the
  sandbox sentinels below into `prompt.ErrNotFound` / `prompt.ErrValidation`.
- **`internal/prompt/dropbox.go`** — `httpFetcher.Fetch` wraps
  `ErrSourceUnavailable` for a transport error and for any non-200/404 status
  (the 404 → `ErrNotFound` branch is unchanged).
- **`internal/sandbox`** — exported `sandbox.ErrNotFound` (absent path / not a
  file) and `sandbox.ErrPathEscape` (confinement escape / invalid path), wrapped
  by the existing `%w` sites, so callers classify without string matching. The
  `GET /run-content` holder keeps collapsing every resolution failure to a bare
  404 (D22 R-6EI5-SSZ1) and is untouched.
- **`internal/mcp`** — `codeFor` gains the `prompt.ErrTooLarge → too_large` and
  `prompt.ErrSourceUnavailable → source_unavailable` arms.

**Done when:**
- The suite is green — `go test ./...` from `prompts/` passes with no race
  violations (design Conventions).
- These Verification ids are each covered by a clearly-named test through the
  assembled `appkit/mcp` handler (real `prompt.Service`, a fetcher fake for the
  import paths, real temp sandbox):
  - R-BC21-7LWP — `import` of a >1 MiB mirror body → `structuredContent.code ==
    "too_large"` (not `"validation"`).
  - R-BD9X-LDNE — `import` when the mirror is unreachable (transport error or a
    non-200/404 status) → `structuredContent.code == "source_unavailable"`.
  - R-BEHT-Z5E3 — `run_fs_read` of an absent sandbox path →
    `structuredContent.code == "not_found"`.
  - R-BFPQ-CX4S — `run_fs_read` of an escaping sandbox path (`../../secrets`) →
    `structuredContent.code == "validation"`.
