# ralph build — Phase 0 recon manifest

Recon only. No source was modified. All paths are absolute and were observed
directly (grep/read), not guessed.

Sources:
- Chassis: `/mnt/projects/ikigai/ralph/ledger/` (Go module `ledger`)
- Engine:  `/home/mgreenly/projects/ikigai-cli/app-root/internal/` (Go module `github.com/ai4mgreenly/ikigai-cli`)
- Target:  `/mnt/projects/ikigai/ralph/ralph/` (Go module `ralph`), engine lands under `ralph/internal/engine/`

---

## A. Engine copy manifest

### A.1 Transitive package closure

Root set requested: `agent`, `tools` (+ 6 tool subpkgs), `provider` (+`provider/anthropic`), `model`, `wire`.

Walking non-test imports of `github.com/ai4mgreenly/ikigai-cli/internal/...`:

- `agent` imports → `model`, `provider`, `schema`, `tools`, `trace`, `wire`
- `tools` (dispatch.go, tools.go) imports → `tools/bash`, `tools/edit`, `tools/glob`, `tools/grep`, `tools/read`, `tools/write`, `wire`
- each `tools/<x>` imports → `wire` only (stdlib otherwise)
- `provider` imports → stdlib only
- `provider/anthropic` imports → `provider`, `trace`
- `model` imports → stdlib only
- `wire` imports → stdlib only
- `schema` imports → stdlib only
- `trace` imports → stdlib only

**Final closure (13 packages):**

```
agent
tools
tools/bash
tools/edit
tools/glob
tools/grep
tools/read
tools/write
provider
provider/anthropic
model
wire
schema
trace
```

**EXCLUDED** (verified not imported by the closure — only reachable from
CLI or other providers):
`provider/google`, `provider/openai`, `driver`, `startup`, `layout`,
`configless`, `build`, `scope`. (`scope`, `build`, `configless`, `layout` are
test-only/CLI-only packages; `driver`/`startup` are CLI wiring; google/openai
are dropped providers.)

The provider factory `buildClient` that imports google/openai lives in
`cmd/ikigai-cli/main.go` (CLI entrypoint) — **outside** the engine closure (see
Section C). The `model` registry names google/openai only as map keys/strings,
not imports, so it copies unchanged.

### A.2 File-by-file copy list

Format: `SRC  ->  DEST`. SRC root = `/home/mgreenly/projects/ikigai-cli/app-root/internal/`.
DEST root = `/mnt/projects/ikigai/ralph/ralph/internal/engine/`.
All `_test.go` files copy too — every test in the closure imports only closure
packages (verified, see A.4).

**agent/**
```
agent/loop.go        -> engine/agent/loop.go
agent/loop_test.go   -> engine/agent/loop_test.go
agent/prompt.go      -> engine/agent/prompt.go
```

**tools/**
```
tools/dispatch.go    -> engine/tools/dispatch.go
tools/tools.go       -> engine/tools/tools.go
tools/tools_test.go  -> engine/tools/tools_test.go
```

**tools/bash/**
```
tools/bash/bash.go        -> engine/tools/bash/bash.go
tools/bash/bash_test.go   -> engine/tools/bash/bash_test.go
tools/bash/export_test.go -> engine/tools/bash/export_test.go
tools/bash/schema_test.go -> engine/tools/bash/schema_test.go
```

**tools/edit/**
```
tools/edit/edit.go        -> engine/tools/edit/edit.go
tools/edit/edit_test.go   -> engine/tools/edit/edit_test.go
```

**tools/glob/**
```
tools/glob/glob.go        -> engine/tools/glob/glob.go
tools/glob/glob_test.go   -> engine/tools/glob/glob_test.go
```

**tools/grep/**
```
tools/grep/grep.go        -> engine/tools/grep/grep.go
tools/grep/grep_test.go   -> engine/tools/grep/grep_test.go
```

**tools/read/**
```
tools/read/read.go        -> engine/tools/read/read.go
tools/read/read_test.go   -> engine/tools/read/read_test.go
tools/read/schema_test.go -> engine/tools/read/schema_test.go
```

**tools/write/**
```
tools/write/write.go        -> engine/tools/write/write.go
tools/write/write_test.go   -> engine/tools/write/write_test.go
```

**provider/**
```
provider/provider.go            -> engine/provider/provider.go
provider/provider_test.go       -> engine/provider/provider_test.go
provider/clone_blocks_test.go   -> engine/provider/clone_blocks_test.go
provider/error_message_test.go  -> engine/provider/error_message_test.go
```

**provider/anthropic/**
```
provider/anthropic/anthropic.go      -> engine/provider/anthropic/anthropic.go
provider/anthropic/anthropic_test.go -> engine/provider/anthropic/anthropic_test.go
```

**model/**
```
model/model.go         -> engine/model/model.go
model/model_test.go    -> engine/model/model_test.go
model/registry.go      -> engine/model/registry.go
model/registry_test.go -> engine/model/registry_test.go
```

**wire/** (all .go files in the dir — none import outside `wire`):
```
wire/assistant_test.go            wire/correlation_test.go
wire/decode.go                    wire/decode_test.go
wire/event.go                     wire/event_test.go
wire/line_flush_test.go           wire/line_size_test.go
wire/replay_test.go               wire/result.go
wire/result_test.go               wire/session.go
wire/session_pending_test.go      wire/session_synchronous_test.go
wire/session_test.go              wire/stdin_reader.go
wire/stdin_reader_test.go         wire/text_block.go
wire/text_block_test.go           wire/thinking_block.go
wire/thinking_block_test.go       wire/tool_result_block.go
wire/tool_result_block_test.go    wire/tool_roundtrip_test.go
wire/tool_use_block.go            wire/tool_use_block_test.go
wire/user_test.go                 wire/utf8_test.go
wire/wire.go                      wire/wire_test.go
```
All -> `engine/wire/<same name>`.

**schema/**
```
schema/schema.go        -> engine/schema/schema.go
schema/schema_test.go   -> engine/schema/schema_test.go
```

**trace/**
```
trace/trace.go  -> engine/trace/trace.go
```

### A.3 Non-Go data files in closure

**NONE.** Verified: `find` over all 13 closure dirs returns zero non-`.go`
files; `grep go:embed` over the closure returns zero matches. The engine
closure is pure Go source, no embeds/json/txt/templates.

(For contrast, the *chassis* `ledger/internal/db/migrations/*.sql` IS a data
file with a `//go:embed` in `ledger/internal/db/db.go:19` — but that is the
chassis db package, not the engine, and is handled in Section E.)

### A.4 Test-file dependency check

Every `_test.go` in the closure imports only closure packages (none pull
google/openai/driver/scope/startup). Notable:
- `agent/loop_test.go` imports `provider`, `schema`, `wire` (uses a stub
  provider.Client, no real backend) → safe to copy.
- `provider/anthropic/anthropic_test.go` imports `provider`, `trace` → safe.
- `wire/utf8_test.go`, `schema/schema_test.go`, `model/*_test.go`,
  `tools/bash/export_test.go` import no engine packages → safe.

---

## B. Import-rewrite map

Single uniform prefix rewrite for every closure package:

```
github.com/ai4mgreenly/ikigai-cli/internal/  ->  ralph/internal/engine/
```

Distinct import paths that appear in the closure source and must be rewritten
(sed-style, longest-first to avoid partial-prefix issues):

```
github.com/ai4mgreenly/ikigai-cli/internal/provider/anthropic -> ralph/internal/engine/provider/anthropic
github.com/ai4mgreenly/ikigai-cli/internal/tools/bash         -> ralph/internal/engine/tools/bash
github.com/ai4mgreenly/ikigai-cli/internal/tools/edit         -> ralph/internal/engine/tools/edit
github.com/ai4mgreenly/ikigai-cli/internal/tools/glob         -> ralph/internal/engine/tools/glob
github.com/ai4mgreenly/ikigai-cli/internal/tools/grep         -> ralph/internal/engine/tools/grep
github.com/ai4mgreenly/ikigai-cli/internal/tools/read         -> ralph/internal/engine/tools/read
github.com/ai4mgreenly/ikigai-cli/internal/tools/write        -> ralph/internal/engine/tools/write
github.com/ai4mgreenly/ikigai-cli/internal/provider           -> ralph/internal/engine/provider
github.com/ai4mgreenly/ikigai-cli/internal/tools              -> ralph/internal/engine/tools
github.com/ai4mgreenly/ikigai-cli/internal/model              -> ralph/internal/engine/model
github.com/ai4mgreenly/ikigai-cli/internal/schema             -> ralph/internal/engine/schema
github.com/ai4mgreenly/ikigai-cli/internal/trace              -> ralph/internal/engine/trace
github.com/ai4mgreenly/ikigai-cli/internal/wire               -> ralph/internal/engine/wire
```

A single blanket sed of the common prefix
`github.com/ai4mgreenly/ikigai-cli/internal/` → `ralph/internal/engine/` over
all copied `.go` files is sufficient and unambiguous.

### B.1 External dependencies — VERIFIED STDLIB-ONLY

`/home/mgreenly/projects/ikigai-cli/app-root/go.mod`:
```
module github.com/ai4mgreenly/ikigai-cli
go 1.26
```
**No `require` block at all.** The entire ikigai-cli module is stdlib-only.

Cross-checked every non-test import line in the closure: the only non-stdlib
imports are the `ikigai-cli/internal/...` ones (rewritten above). All other
imports are stdlib: `context encoding/json fmt strings time bufio bytes errors
io net net/http net/url os os/exec sync syscall sort regexp io/fs path/filepath
crypto/rand encoding/base32`.

**FLAG: confirmed stdlib-only — the "stdlib-only engine" claim holds. No
`go get` / no new `require` is needed for the engine.** `ralph`'s `go.mod` keeps
exactly the chassis deps (`modernc.org/sqlite` + its indirects); the engine
adds nothing.

---

## C. Provider factory pin

**There is no provider factory inside the engine closure.** The provider
selection/factory lives in the CLI entrypoint, which is NOT being copied:

- `/home/mgreenly/projects/ikigai-cli/app-root/cmd/ikigai-cli/main.go`
  - lines 16–19: imports `provider`, `provider/anthropic`, `provider/google`,
    `provider/openai`.
  - `func buildClient(resolved model.Resolved) (provider.Client, string, error)`
    at **line 229**; the switch on `resolved.Provider`:
    - line ~233 `anthropicprovider.New(...)`
    - line ~240 `openaiprovider.New(...)`
    - line ~248 `googleprovider.New(...)`
    - line ~254 `unsupported provider` default.

Because `cmd/ikigai-cli/main.go` is the CLI and is not copied into ralph, **the
google/openai imports never enter the ralph build**. ralph will write its own
small factory in its `cmd`/agent-bring-up code that calls only
`engine/provider/anthropic.New(...)`.

**Minimal edit for ralph's new factory (Phase 2/3):** construct the client with
`anthropic.New(apiKey, bareID)` directly (single provider). No switch needed.

**Confirmation dropping google/ and openai/ won't break the closure:**
`grep -rn "provider/google|provider/openai|googlebackend|openaibackend"` over
all NON-test `.go` files in `internal/` returns **zero** hits. The only files
referencing them are `cmd/ikigai-cli/main.go` (not copied) and the two providers'
own `_test.go` files (not copied). The `model` registry references google/openai
as string map keys only (`internal/model/registry.go` lines 168, 186; `model.go`
lines 24–26, 63–66) — those are data, not imports, and compile fine without the
provider packages. **Safe to drop.**

Note: ralph may optionally prune the OpenAI/Google entries from
`engine/model/registry.go` / `engine/model/model.go` to anthropic-only, but it
is not required for compilation — leaving them is harmless (they just won't be
selectable since no backend exists).

---

## D. Engine adaptation points (locate only — Phase 3)

All refs below in `/home/mgreenly/projects/ikigai-cli/app-root/internal/`,
which becomes `ralph/internal/engine/`.

### D.1 `agent.Run` — signature + schema-terminal block (freeform mode goes here)

File: `agent/loop.go`.

Signature (line 63):
```go
func Run(ctx context.Context, client provider.Client, sess *wire.Session,
         req provider.Request, sch *schema.Schema, tracer *trace.Tracer) error
```

Main loop: `for {` at **line 76**.
- non-tool stop / terminal handling: **lines 102–146**.
  - `attempt++` line 102.
  - `value, perr := parseAndValidate(text, sch)` **line 103** — THE schema
    parse/validate call. This is the "freeform terminal mode" insertion point:
    when stop is not `tool_use`, freeform mode would bypass/relax
    `parseAndValidate` and emit the raw final text instead of validated JSON.
  - success branch builds `IterationStats` + `NewResultEventFull`, lines 104–139.
  - retry/exhaustion: `lastErr = perr` line 141; bound check
    `if attempt >= maxStructuredAttempts` **lines 143–145**.
  - `const maxStructuredAttempts = 3` at **line 31**.
- the JSON parse + schema validate helper:
  `func parseAndValidate(text string, sch *schema.Schema) (any, error)` at
  **lines 272–287** (json.Unmarshal line 278; `sch.Validate` line 282).

### D.2 Framing prompt (bare-JSON instruction)

File: `agent/prompt.go`.
`const FramingPrompt = ...` at **lines 11–16**. It instructs "output your final
answer as a single bare JSON value with no surrounding text, no markdown code
fences". Freeform mode will need an alternate/relaxed framing prompt here.

### D.3 `tools.Dispatch` — signature, dropped ctx, no sandbox root

File: `tools/dispatch.go`.
Signature **line 24**:
```go
func Dispatch(_ context.Context, block wire.ToolUseBlock) (wire.ToolResultBlock, any, error)
```
**ctx is accepted but dropped** (`_` parameter, line 24) — never threaded to any
tool. No sandbox-root parameter exists anywhere. Phase 3 must change this to
`Dispatch(ctx context.Context, sandboxRoot string, block ...)` and thread both.

Per-tool root/path resolution to thread `sandboxRoot` into:

- **bash** (`tools/bash/bash.go`):
  - `func Run(toolUseID, cmd string) (RunResult, error)` **line 102**.
  - `exec.CommandContext(ctx, "bash", "-c", cmd)` **line 105** — `c.Dir` is
    NEVER set (comment at lines 88–98 notes it defaults to launch dir). Phase 3:
    set `c.Dir = sandboxRoot` and use the passed `ctx` instead of the internal
    `context.WithTimeout(context.Background(), ...)` at line 103.
  - dispatch call site: `bash.Run(block.ID, in.Command)` `dispatch.go:34`.

- **glob** (`tools/glob/glob.go`):
  - `func Glob(toolUseID, pattern, searchPath string)` **line 56**;
    `root, err := resolveRoot(searchPath)` line 57.
  - `func resolveRoot` defaults to `os.Getwd()` **line 120** when path empty.
    Phase 3: default to `sandboxRoot` instead of `os.Getwd()`.
  - call site `glob.Glob(block.ID, in.Pattern, in.Path)` `dispatch.go:73`.

- **grep** (`tools/grep/grep.go`):
  - `func Grep(toolUseID string, in Input)` **line 141**;
    `resolvePath(in.Path)` **line 175**.
  - `func resolvePath` defaults empty path to `os.Getwd()` **line 240**.
    Phase 3: default to `sandboxRoot`.
  - `type Input struct` at **line 114**.
  - call site `grep.Grep(block.ID, in)` `dispatch.go:81`.

- **read** (`tools/read/read.go`):
  - `func Read(toolUseID, absPath string, offset, limit int)` **line 71**;
    rejects relative paths `if !filepath.IsAbs(absPath)` **line 72**. Phase 3:
    resolve relative paths against `sandboxRoot` (or enforce within it).
  - call site `read.Read(block.ID, in.FilePath, in.Offset, in.Limit)` `dispatch.go:51`.

- **write** (`tools/write/write.go`):
  - `func Write(toolUseID, filePath, content string)` **line 56**;
    `if !filepath.IsAbs(filePath)` **line 57**; `dir := filepath.Dir(filePath)`
    line 62. Phase 3: resolve against `sandboxRoot`.
  - call site `write.Write(block.ID, in.FilePath, in.Content)` `dispatch.go:62`.

- **edit** (`tools/edit/edit.go`):
  - `func Edit(toolUseID, filePath, oldString, newString string, replaceAll bool)`
    **line 61**; `if !filepath.IsAbs(filePath)` **line 73**;
    `dir := filepath.Dir(filePath)` line 122. Phase 3: resolve against
    `sandboxRoot`.
  - call site `edit.Edit(block.ID, in.FilePath, in.OldString, in.NewString, in.ReplaceAll)` `dispatch.go:94`.

### D.4 Preserve-list (do NOT break in Phase 3)

All in `agent/loop.go`:
- `drainTurn(events)` **lines 217–267** — flush logic, thinking-signature
  round-trip (lines 234–243, signature preserved in providerBlocks even when
  text empty per R-FPG8-RKEP), tool_use collection, usage capture (line 259–261).
- `dispatchTools(ctx, sess, req, wireBlocks, providerBlocks, tracer)`
  **lines 153–210** — emits one user event per tool_result, attaches
  per-tool sidecar (lines 182–187, `NewUserEventWithSidecar`), clones
  assistant blocks preserving thinking signatures (`provider.CloneBlocks`,
  line 204, R-ROBI-V64M).
- **signed-thinking round-trip:** `provider.ThinkingBlock{Text, Signature}`
  built in `drainTurn` line 243 and round-tripped via `CloneBlocks` in
  `dispatchTools` line 204.
- **usage accounting:** `cumUsage` accumulation **lines 84–87**; cost via
  `pricing.ComputeCost` lines 106–111; `IterationStats`/`ModelUsage` build
  lines 112–134; emitted by `NewResultEventFull` line 135.
- **per-tool sidecars:** produced in `tools/dispatch.go` (only bash returns one,
  `bash.BashSidecar` lines 35–39); consumed in `dispatchTools` lines 182–187.
- tracer hooks: `tracer.LogToolDispatch` (loop.go:162),
  `tracer.LogToolResult` (loop.go:178) — `tracer` may be nil.

---

## E. Ledger -> ralph rename map

Chassis tree: `/mnt/projects/ikigai/ralph/ledger/`. Module `ledger`.

### E.1 Global parameter changes

| item | ledger value | ralph value |
|---|---|---|
| go module name | `ledger` | `ralph` |
| cmd dir | `cmd/ledger/` | `cmd/ralph/` |
| env prefix | `LEDGER_` | `RALPH_` |
| loopback port | `3002` | `3004` |
| nginx mount | `/srv/ledger/` | `/srv/ralph/` |
| MCP tool prefix | `ledger_` | `ralph_` |
| MCP server name | `Ledger` | `Ralph` |
| sqlite db file | `ledger.db` | `ralph.db` |
| install root | `/opt/ledger` | `/opt/ralph` |
| systemd unit | `ledger` (`ledger.conf`, `ExecStart=ikigenba-launch ledger`) | `ralph` |
| Makefile APP | `ledger` | `ralph` |

Also: add `./ralph` to `/mnt/projects/ikigai/ralph/go.work` `use(...)` block —
**go.work currently lists crm/dashboard/eventplane/ledger/notify but NOT ralph.**

### E.2 go.mod

`/mnt/projects/ikigai/ralph/ledger/go.mod` line 1: `module ledger` ->
`module ralph`. The `require` block (`modernc.org/sqlite v1.50.1` + indirects)
stays as-is; copy `go.sum` verbatim. Engine adds no deps (Section B.1).

### E.3 Internal import paths (`ledger/internal/...` -> `ralph/internal/...`)

- `cmd/ledger/main.go:25` `"ledger/internal/db"`
- `cmd/ledger/main.go:26` `"ledger/internal/logging"`
- `cmd/ledger/main.go:27` `"ledger/internal/mcp"`
- `cmd/ledger/main.go:28` `"ledger/internal/server"`
- `internal/server/server.go:20` `"ledger/internal/logging"`

Rewrite prefix `ledger/internal/` -> `ralph/internal/`.

### E.4 Exhaustive literal occurrences (file:line) — Phase 1 checklist

**cmd/ledger/main.go** (rename dir to cmd/ralph/):
- 1, 6, 7, 9: doc comments "ledger"
- 25–28: internal imports (see E.3)
- 36: `fmt.Fprintln(os.Stderr, "ledger:", err)`
- 42: `envOrInt(getenv, "LEDGER_PORT", 3002)` -> `RALPH_PORT`, `3004`
- 47: `flag.NewFlagSet("ledger", ...)`
- 54: `"LEDGER_IP"` (and flag help text)
- 55: `"LEDGER_PORT"`
- 56: `"LEDGER_LOG_LEVEL"`
- 68, 70: comments `LEDGER_RESOURCE_ID` / `LEDGER_AUTH_SERVER`
- 73: `envOr(getenv, "LEDGER_RESOURCE_ID", "http://localhost:8080/srv/ledger/mcp")`
- 74: `"LEDGER_AUTH_SERVER"`
- 75, 77: `"LEDGER_DB_PATH"`, default `"./tmp/ledger.db"` -> `./tmp/ralph.db`
- 95: comment "ledger domain logic"
- 119: `logger.Info("starting ledger", ...)`

**internal/server/server.go**:
- 1, 5, 7, 8: package doc "ledger" / "/srv/ledger/"
- 20: import `"ledger/internal/logging"`
- 29: comment `"127.0.0.1:3002"` -> 3004
- 81, 82: comments "ledger service" / "/srv/ledger/"
- 91: comment "ledger_whoami proof"
- 95: comment "ledger_* tool surface"

**internal/server/handlers.go**:
- 20: comment "/srv/ledger/ prefix"
- 32: comment "ledger_whoami proof"

**internal/server/middleware.go**:
- 29: comment "ledger performs no token"

**internal/server/server_test.go**:
- 13: `testResourceID = "https://int.ikigenba.com/srv/ledger/mcp"` -> `/srv/ralph/mcp`

**internal/logging/logging.go**:
- 1: package doc "ledger service's structured logger"

**internal/mcp/mcp.go**:
- 2, 4, 5, 32: doc comments "ledger_*", "ledger service", "ledger domain"
- 58: `"serverInfo": {"name": "Ledger", "version": "1"}` -> `"Ralph"`

**internal/mcp/tools.go**:
- 9, 10: comments "ledger_* tool set" / "ledger_whoami"
- 15: `desc("ledger_whoami", "...", ...)` -> `ralph_whoami` (tool name + description)
- 56: `case "ledger_whoami":` -> `ralph_whoami`

**etc/manifest.env**:
- 1: `APP=ledger` -> `APP=ralph`
- 2: `MOUNT=/srv/ledger/` -> `/srv/ralph/`
- 4: `PORT=3002` -> `3004`
- (3 `DEFAULT=false`, 6 `MCP=true` unchanged)

**etc/nginx.conf** (location fragment):
- 1, 5: header comments + `locations/ledger.conf` path
- 8: comment `MOUNT=/srv/ledger/, PORT=3002`
- 13, 15: `/srv/ledger/` location
- 16: `proxy_pass http://127.0.0.1:3002/.well-known/...` -> 3004
- 23: `location /srv/ledger/ {`
- 25, 26: `$ledger_owner`, `$ledger_client` vars
- 34: comment
- 37: `error_page 500 = @ledger_authn_500;`
- 39, 45, 46: identity-header comment + `$ledger_owner`/`$ledger_client`
- 48: `proxy_pass http://127.0.0.1:3002/;` -> 3004; comment `/srv/ledger/`
- 57: `location @ledger_authn_500 {`
- (rename all `ledger_owner`/`ledger_client`/`ledger_authn_500` nginx vars to `ralph_*`)

**etc/deploy.env**:
- 2: comment "ledger is a path-routed service" (ACCOUNT/SSH unchanged)

**Makefile**:
- 1: comment "builds and runs ledger directly"
- 5: `APP := ledger` -> `ralph`
- 14: `go build ... ./cmd/ledger` -> `./cmd/ralph`

**bin/build**:
- 2, 5, 7, 9, 10, 14, 35: comments + paths (`build/ledger`, `/opt/ledger/bin/run`,
  `LEDGER_RESOURCE_ID/AUTH_SERVER`, `build/ledger.bin`, `/opt/ledger/bin/ledger.bin`,
  `/srv/ledger/`)
- 28: `-o "build/${APP}.bin" ./cmd/ledger` -> `./cmd/ralph`
- 38: `export LEDGER_RESOURCE_ID="https://${IKIGENBA_DOMAIN}/srv/ledger/mcp"`
- 39: `export LEDGER_AUTH_SERVER=...`
- 40: `export LEDGER_DB_PATH=/opt/ledger/data/ledger.db`
- 41: `exec /opt/ledger/bin/ledger.bin \`
  (rewrite `LEDGER_*` -> `RALPH_*`, `/srv/ledger/` -> `/srv/ralph/`,
  `/opt/ledger` -> `/opt/ralph`, `ledger.bin` -> `ralph.bin`)

**bin/deploy**:
- 8: `/opt/ledger/data/ledger.db`
- 10: `/opt/ledger tree`
- (26: `BIN="build/${APP}.bin"` — driven by APP, no literal change)

**bin/start**:
- 2: comment `build/ledger.pid`
- 5, 20, 24, 26: "ledger" text; line 24/26 `LEDGER_PORT`
- (12: `BIN="build/${APP}.bin"` — APP-driven)

**bin/stop**:
- 2, 12, 19, 21: "ledger" text

**bin/setup**:
- 2, 4, 8, 9: comments + `locations/ledger.conf`
- 56: `"sudo APP='$APP' ...` — APP-driven, no literal

Note `bin/setup:56`, `bin/deploy:26`, `bin/start:12` use `$APP`/`${APP}`, so
changing `APP` in the env/Makefile flows through; still verify.

### E.5 Existing tests (Phase 1 must keep green)

**internal/db/db_test.go** — chassis db migration tests (no `ledger` literal in
asserts; module-path rewrite only). Asserts:
- `TestOpenAndMigrate` (line 14): after first `Migrate`, `schema_migrations`
  count >= 1.
- `TestMigrate_IsIdempotent` (35): second migrate doesn't change row count.
- `TestMigrate_RefusesDowngrade` (62): injecting a future version makes migrate
  refuse (error expected).
- `TestLoadMigrations_Order` (87): migrations load in contiguous version order,
  no gaps; non-empty.
- Depends on embedded `internal/db/migrations/*.sql` (`db.go:19 //go:embed`).
  **Copy `internal/db/migrations/001_schema_migrations.sql` verbatim** — it is
  the one non-Go data file in the chassis and is required for these tests.

**internal/server/server_test.go** — asserts:
- `testResourceID = "https://int.ikigenba.com/srv/ledger/mcp"` (line 13) →
  **must change to `/srv/ralph/mcp`**.
- `TestNewRequiresConfig` (33): `New` errors when any of logger/auth-server/mcp
  missing.
- `TestPRMetadataUnauthenticated` (54): PRM well-known returns 200 +
  `application/json`; `resource == testResourceID`; `authorization_servers ==
  [testAuthServer]`; `bearer_methods_supported == ["header"]`.
- `TestWhoamiWithIdentity` (86): `/whoami` with injected
  `X-Owner-Email`/`X-Client-Id` → 200, `owner_email == owner@example.com`,
  `client_id == client-abc`.
- `TestWhoamiWithoutOwnerEmailIs401` (110): missing `X-Owner-Email` → 401 with
  `WWW-Authenticate` challenge.

### E.6 Note on `internal/ids/ids.go`

Pure stdlib (`crypto/rand`, `encoding/base32`, `time`) — no `ledger` literal, no
external dep. Copy unchanged (module-path rewrite only if imported; it is not
imported elsewhere in the grep set but ships with the chassis).

---

## Summary of critical findings

- **Closure (13 pkgs):** agent, tools(+bash/edit/glob/grep/read/write),
  provider(+anthropic), model, wire, schema, trace.
- **External deps: NONE — engine is verified stdlib-only** (ikigai-cli go.mod has
  no `require` block). ralph adds nothing beyond the chassis `modernc.org/sqlite`.
- **Provider factory** is in CLI `cmd/ikigai-cli/main.go:229 buildClient` —
  outside the closure; google/openai are referenced ONLY there and in their own
  tests. Dropping them is safe.
- **No data files / no embeds** in the engine closure.
