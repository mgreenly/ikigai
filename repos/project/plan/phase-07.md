# Phase 7 — The MCP tool surface

*Realizes design Decision 7 (the nine structured verbs). Depends on Phases 4
and 6.*

`internal/mcp/mcp.go` + `internal/mcp/tools.go` over the shared `appkit/mcp`
transport: `clone`, `list`, `get`, `delete`, `session_start`,
`session_list`, `session_get`, `session_output`, `session_cancel` — bare
names, both schemas per tool, `StructuredResult` successes, closed-vocabulary
`ErrorResult`s, owner threading from `server.Identity`, the repo/session
views D7 pins, `ikibot/*` branch-namespace validation on `session_start`,
and transcript line-slicing for `session_output`. Tests exercise the
assembled handler over `httptest` with real SQLite and fixture remotes,
including the no-side-effect validation-failure assertion and the
exactly-nine-tools registration check.

**Done when:** R-FN1M-P1FW, R-FO9J-2T6L, R-FPHF-GKXA, R-FQPB-UCNZ, and
R-FRX8-84EO are each covered by a clearly-named test, and the suite is green
per design Conventions.
