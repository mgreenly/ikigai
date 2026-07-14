# Phase 32 — Structured MCP surface: schemas, `StructuredResult`, coded errors, and the chassis loopback guard

*Realizes design Decision 27 (Structured MCP adoption) — slice: the MCP-layer
conversion and the content-holder guard swap, using the two error sentinels that
already exist (`prompt.ErrNotFound`, `prompt.ErrValidation`). Depends on Phase 20
(the `appkit/mcp` tool table) and Phase 27 (the `GET /run-content` holder).*

This phase makes prompts compile and pass against the new appkit contract
(`JSONResult` deleted, `ErrorResult` re-signed) by converting its MCP surface:

- **`internal/mcp/tools.go`** — the thirteen structured domain tools (`create`,
  `import`, `list`, `get`, `update`, `delete`, `set_trigger`, `clear_trigger`,
  `run`, `run_list`, `run_get`, `run_cancel`, `run_fs_list`) each return
  `appkitmcp.StructuredResult(v)` over the **same** value they marshal today
  (propagating its `(map, error)` second return honestly), and each declares an
  `OutputSchema` literal mirroring that emitted JSON (the `objSchema`/`typ`
  house helpers). The three prose tools (`describe`, `run_output`,
  `run_fs_read`) keep `TextResult` and declare no `OutputSchema`.
- **Coded errors via one classifier.** A `fail(err)` helper wrapping
  `appkitmcp.ErrorResult(codeFor(err), err.Error())` replaces every bare
  `ErrorResult(err.Error())` site; `codeFor` maps `prompt.ErrNotFound →
  not_found`, `prompt.ErrValidation → validation`, and every other error →
  `internal` (the `too_large`/`source_unavailable`/sandbox arms arrive in Phase
  33). `paramError` stays on the `(nil, err)` → `-32602` path, unwrapped.
- **Guard swap.** `internal/prompt/content.go`'s `RunContentHandler` drops its
  in-handler `X-Owner-Email`/`X-Forwarded-Proto` predicate (it reads no request
  header); `cmd/prompts/main.go` mounts it with
  `rt.HandleLoopback("GET /run-content", …)` so the shared chassis
  `server.LoopbackOnly` (keyed on `X-Forwarded-Proto` only) is the guard.

The retired R-6DA9-F18C's content-guard test (Phase 27) is replaced by
R-BI5J-4GM6's; no domain result body changes shape.

**Done when:**
- The suite is green — `go test ./...` from `prompts/` passes with no race
  violations (design Conventions) — and non-test source references no deleted
  token: `grep -rn 'JSONResult' internal cmd --include='*.go' | grep -v _test.go`
  is empty, and `grep -nE 'X-Owner-Email|X-Forwarded-Proto' internal/prompt/content.go`
  is empty (the guard predicate left the handler).
- These Verification ids are each covered by a clearly-named test through the
  assembled `appkit/mcp` handler (real `prompt.Service` + temp SQLite via
  `httptest`) or the mounted `RunContentHandler` (real temp sandbox):
  - R-B4QM-WZGJ — `tools/list` declares an object `outputSchema` for each of
    the 13 structured tools plus chassis `health`/`reflection`, and none for the
    three prose tools (`describe`, `run_output`, `run_fs_read`).
  - R-B5YJ-AR78 — each of the 13 structured tools' success result has a text
    block whose JSON parse is deep-equal to its `structuredContent`.
  - R-B76F-OIXX — `describe`, `run_output`, `run_fs_read` each return a text
    block and carry no `structuredContent`.
  - R-B8EC-2AOM — `get`/`run_get` of an unknown or foreign-owned id →
    `isError` with `structuredContent.code == "not_found"`.
  - R-B9M8-G2FB — `set_trigger` with an unknown source →
    `structuredContent.code == "validation"`.
  - R-BGXM-QOVH — a forced unclassified (store/DB) error →
    `structuredContent.code == "internal"` (default arm is `internal`, not
    `validation`).
  - R-BI5J-4GM6 — `GET /run-content` for an existing file: `X-Forwarded-Proto:
    https` → bare 404 serving none of the bytes; the same request without
    `X-Forwarded-Proto` but with `X-Owner-Email` set → 200 with the file bytes.
