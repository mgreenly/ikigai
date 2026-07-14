# Phase 13 — Structured MCP adoption for cron's domain surface

*Realizes design Decision 15 (Structured MCP adoption). Depends on Phase 08
(the `appkit/mcp` tool-table surface) and Phase 12 (event-routing conformance),
and on the appkit structured-MCP conformance (appkit phases 12–14) being built —
operator-sequenced: cron does not currently compile against that appkit, and this
phase makes it green again.*

This phase adopts the suite's structured-MCP result contract
(`docs/structured-mcp-design.md`) across cron's five crontab domain tools in
`cron/internal/mcp/` (`tools.go` + `mcp.go`), reshaping **no** result:

- Every domain success site returns `appkitmcp.StructuredResult(v)` in place of
  the deleted `appkitmcp.JSONResult(v)`, propagating the helper's new error
  return honestly (the handlers already return `(map[string]any, error)`).
- `errorEnvelope` and its `{error:{code,message,field?}}` text marshalling are
  removed; `toolErr(err)` maps each domain sentinel to one closed-vocabulary code
  and calls `appkitmcp.ErrorResult(code, msg)` — `*parseError`→`ErrValidation`,
  `crontab.ErrExists`→`ErrConflict`, `crontab.ErrNotFound`→`ErrNotFound`,
  `crontab.ErrInvalid`→`ErrValidation`, default→`ErrInternal` (the D15 table).
- Each domain tool declares a hand-authored `outputSchema` mirroring the JSON it
  already emits (a shared `entrySchema()` for `create`/`get`/`update`; `items`
  array for `list`; `{ok:boolean}` for `delete`), surfaced by `tools/list`.

The observable end state: cron compiles and its suite is green against the new
appkit; `tools/list` shows an `outputSchema` per domain tool; every success
result carries `structuredContent` + mirrored text; every tool error carries a
closed-vocabulary `structuredContent.code`; the seven-tool partition (D10) and
the tick routing (D14) are untouched. cron declares no prose-exception tool and
changes no loopback guard (D15 §4–5).

**Done when** — cron's suite is green per the design Conventions
(`cd cron && go build ./...`, `go vet ./...`, `gofmt -l .` empty, and
`go test ./...` all pass with zero failures), the source-scan guard confirms the
old token is gone, and every id below is covered by a clearly-named test:

- R-6TVE-C4AC — table-driven test over `create`/`list`/`get`/`update`/`delete`:
  each success result's `structuredContent` equals the emitted JSON object and
  `content[0].text` unmarshals to the identical object (`create.last_slot==null`,
  `delete.ok==true`, `list.items` an array).
- R-6V3A-PW11 — `tools/list` returns a non-nil `outputSchema` for each of the
  five domain tools, matching the emitted shape (entry schema requires
  `name,expr,created_at,updated_at,last_slot` with `last_slot` typed
  `["string","null"]`; `list` requires `items:array`; `delete` requires
  `ok:boolean`).
- R-6WB7-3NRQ — `create`/`update` with an invalid `expr` returns `isError:true`
  and `structuredContent.code == "validation"`, message naming the bad field.
- R-6XJ3-HFIF — `create` of an existing `name` returns `isError:true` and
  `structuredContent.code == "conflict"` (never the retired `"duplicate"`).
- R-6YQZ-V794 — `get`/`update`/`delete` of an unknown schedule returns
  `isError:true` and `structuredContent.code == "not_found"`.
- R-6ZYW-8YZT — `create` with a CHECK-violating `name` (`crontab.ErrInvalid`)
  returns `isError:true` and `structuredContent.code == "validation"`.
- R-716S-MQQI — a source-scan test (walking the module tree, excluding
  `*_test.go`) asserts the token `JSONResult` appears nowhere in cron's
  `internal/` and `cmd/` Go source. Reinforced by the deterministic grep
  `grep -rn 'JSONResult' cron/internal cron/cmd --include='*.go' | grep -v _test.go`
  returning empty.
