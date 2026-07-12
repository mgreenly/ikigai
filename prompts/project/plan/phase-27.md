# Phase 27 — Content-plane holder: `GET /run-content` + `content_url` on `run_fs_list`

*Realizes design Decision 22 (content-plane holder), revising D17's `Tools`/`NewHandler` signatures in place (the `contentBase` seam). Depends on Phase 26 (mechanically independent, ordered for one-writer discipline over `internal/mcp`). Covers R-6C2D-19HN, R-6DA9-F18C, R-6EI5-SSZ1, R-6FQ2-6KPQ.*

Observable end state:

- `prompts/internal/sandbox/` gains the byte-level `Open(id, relPath) (*os.File, os.FileInfo, error)` using the existing `validateID` + `confine` machinery.
- `prompts/internal/prompt/content.go` adds `Service.RunContentHandler() http.Handler`: the identity-header 404 guard (`X-Owner-Email` / `X-Forwarded-Proto`), `run_id` + `path` query resolution through the sandbox manager, `http.ServeContent` streaming, and the never-leak error mapping (unknown/absent/dir/escape → 404, no `rev` parameter). `cmd/prompts/main.go`'s `registerRoutes` mounts it ungated at `GET /run-content` and resolves `contentBase := registry.BaseURL("prompts")`.
- `internal/mcp`'s `Tools(svc, contentBase)` / `NewHandler(svc, contentBase, rt)` thread the base; `run_fs_list` stamps each non-directory entry with a complete URL-encoded `content_url` (run_id + joined sandbox-relative path) and its description states the reference contract; directory entries carry none. Tool count stays eighteen (R-DKQP-QZ3Q green).

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-6C2D-19HN**, **R-6DA9-F18C**, **R-6EI5-SSZ1** — each covered by a clearly-named test driving the real `RunContentHandler` over a real temp sandbox tree via `httptest`, asserting exactly the behavior its D22 Verification line states (byte-identical streaming with Content-Type/Length; the identity-header 404 guard; the 404-never-500 mapping for unknown run / absent path / directory / escape).
- **R-6FQ2-6KPQ** — a clearly-named test through the assembled MCP handler asserts non-directory `run_fs_list` entries carry a `content_url` built from `registry.MustPort("prompts")` whose query round-trips run_id + joined path, directory entries carry none, and a returned URL fetched against the mounted handler yields the file's bytes.
- R-RG04-NLIT stays green (no `127.0.0.1:30` literal introduced in non-test source; test expectations derive from the registry).
