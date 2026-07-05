# Phase 09 — Rewire the MCP file tools onto `internal/files` and drop `agentkit`

*Realizes design Decision 11. Depends on Phase 08 (the `internal/files` package
must exist). Rewrites `internal/mcp/files.go`, edits `internal/mcp/tools.go` (the
four hand-written schemas + confinement helper), points the native
`file_write`/`file_list`/`mkdir` handlers and `internal/sites/sync.go` at
`files.*`, and removes `agentkit` from `sites/go.mod`. No new dependency; the
15-tool surface is unchanged.*

This phase severs sites's tie to the local `agentkit` module (sites is its only
consumer) by moving the MCP file tools onto the native package built in Phase 08.
`internal/mcp` becomes a pure wire layer: decode `arguments` → validate site →
resolve `layout.WorkingDir(site)` → call `files.*` → format with the existing
result helpers.

In **`internal/mcp/files.go`**:
- Remove the `agentkit/tools` and `agentkit/wire` imports and delete the
  `agentkitSchemas`, `toolFile`, `renderToolResultBlock`, and `contentText`
  machinery.
- `file_read`/`file_edit`/`file_glob`/`file_grep` become ordinary handlers calling
  `files.Read`/`files.Edit`/`files.Glob`/`files.Grep`, formatting native returns:
  `file_read` → text block of the content; `file_edit` → `{edited, site,
  replaced}`; `file_glob` → `{site, matches:[…]}`; `file_grep` → `{site,
  matches:[{path,line,text},…]}` (via `toolResultText`/`toolResultJSON`).
- The native `file_write`/`file_list` handlers call `files.Write`/`files.List`.

In **`internal/mcp/tools.go`**:
- Replace `fileToolDescriptor("file_read"/"file_edit"/"file_glob"/"file_grep", …)`
  (which reflected agentkit schemas) with hand-written `desc(tool(…), …,
  obj(…))` descriptors using `descTyp`, each keeping the required `site` property —
  the same style every other tool already uses.
- Point `mkdir` at `files.Mkdir`/`files.ConfinePath` and **delete the in-package
  `confinePath`** (and any now-unused helpers) so `files.ConfinePath` is the one
  confinement in the module.
- Map a confinement rejection (`errors.Is(err, files.ErrEscapes)`) from any file
  tool to `errResultMsg("path_escapes_working_dir", …)`.

In **`internal/sites/sync.go`**: route the reconcile's writes through
`files.Write`/`files.ConfinePath` rather than its own `os.WriteFile`, so
confinement has one implementation.

In **`sites/go.mod`**: remove `require agentkit v0.0.0` and
`replace agentkit => ../agentkit`, then `go mod tidy`. Add no new dependency.

> **Leave the repo-root `agentkit/` tree on disk.** Deleting the now-orphaned
> local `agentkit` module is suite-level work outside this `project/`'s scope
> (D11). This phase runs from `sites/` and touches only `sites/`.

The pre-existing `internal/mcp` tests (`files_test.go`, `tools_test.go`,
`sync_test.go`) must stay green; their assertions are `Contains`/`IsError`-based
and survive the cleaner JSON-in-text result shapes and the sentinel→
`path_escapes_working_dir` mapping. Update only where a test pinned behavior that
D11 intentionally changed (e.g. an inert `output_mode` grep argument).

**Done when:** the suite is green (per design *Conventions*: `cd sites && go build
./...`, `cd sites && go vet ./...`, `cd sites && gofmt -l .` prints nothing,
`cd sites && go test ./...`, and `bin/check-migrations sites` all succeed with zero
failures) and these ids are covered by clearly-named tests:

- **R-0FMU-J775** — a scan of every `*.go` in the module finds no import path equal
  to or prefixed by `agentkit` (neither `agentkit/…` nor
  `github.com/ikigenba/agentkit`). Reintroducing any agentkit import fails it.
  *(source-tree scan test)*
- **R-0GUQ-WYXU** — a content assertion over `go.mod` finds no `require agentkit`
  line and no `replace agentkit =>` directive. Either remaining fails it.
  *(go.mod content-assertion test)*
- **R-0I2N-AQOJ** — through `tools/call`: `file_glob` returns a result whose JSON
  carries a `matches` array (not a stringified array in a text block), `file_grep`
  returns `matches` of `{path,line,text}`, `file_edit` returns
  `{edited,site,replaced}`, and `file_read` returns the content as text. A
  stringified-array-in-text result or a missing `matches`/`replaced` field fails
  it. *(handler-driven test in `internal/mcp`)*
- **R-0JAJ-OIF8** — a `tools/call` to a file tool with a `..`-escaping or
  outside-root path returns `isError` with the envelope `code`
  `path_escapes_working_dir` (driven for at least `file_read` and `file_list`). A
  generic code or a non-error result fails it. *(handler-driven test)*
- **R-0KIG-2A5X** — `tools/list` returns exactly `{health, describe, create, list,
  delete, mkdir, publish, unpublish, sync, file_write, file_read, file_edit,
  file_glob, file_grep, file_list}` with every `inputSchema` an object schema. A
  missing, added, or renamed tool fails it. *(handler-driven test)*
