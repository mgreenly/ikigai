# Plan — Importing Dropbox-authored artifacts into scripts, prompts, and sites

Status: **ready for execution**. Date: 2026-06-09.
Pairs with `docs/adr-dropbox-import-sync.md` (the decisions; read it first). This
file is the *sequencing and file-level detail* — it does not re-decide anything
the ADR settled.

## How to read this plan

The work is split into **7 phases**, each sized for a **single subagent** that
will run **sequentially** (no parallelism — later phases may depend on earlier
ones). Every phase lists: its goal, the exact files to touch, step-by-step
changes, the MCP descriptor text to use verbatim, the migration command, the
test strategy, and the done-criteria the subagent must satisfy before it returns.

Each phase is self-contained: a subagent given *only this plan + the ADR* and
told "do Phase N" has everything it needs. A subagent should read the ADR and
the specific files named in its phase, make the change, run that service's tests,
and stop.

Conventions every phase obeys (from the repo CLAUDE.md):

- **Never hand-number a migration.** Run `bin/new-migration <service> <name>`;
  it stamps `YYYYMMDDHHMMSS_<name>.sql`. Fill the generated file's body; never
  edit a committed migration. `bin/check-migrations` enforces this.
- Stay in the worktree root; use relative paths. Do not `cd`.
- Match surrounding code style (the services are heavily commented — keep parity).

## Cross-cutting decisions (resolved here, beyond the ADR)

These were left open in the ADR/handoff; this plan fixes them so the subagents
do not each re-litigate. Confirm before execution if any is contentious.

1. **Loopback HTTP client lives per-service, field-injected — no new shared
   module.** scripts and prompts each get a ~25-line `dropbox` client that does
   `GET /content`; sites gets one that also does `GET /list` with cursor
   pagination. The duplication (scripts ≈ prompts) is intentional for v1:
   scripts is *not* an agent-backed service, so `agentkit` is the wrong home,
   and `appkit` is the chassis for a service's *own* server, not for calling
   peers (notify's peer-call to a `/feed` is likewise inline). If a 4th consumer
   appears, extract then. The client is wired through an **interface field** set
   at the composition root (the dropbox `svc.Mirror = …; svc.Client = …`
   pattern), *not* a new `NewService` parameter — this keeps every existing
   `NewService` call site and test untouched.

2. **`DROPBOX_BASE_URL` is env-only (not in `ManifestExtras`).** Read at each
   consumer's composition root with `config.EnvOr(os.Getenv, "DROPBOX_BASE_URL",
   "http://127.0.0.1:3005")`, exactly the shape notify uses for `CRM_FEED_URL`
   (`notify/cmd/notify/main.go:191`). The client derives `<base>/content` and
   `<base>/list`.

3. **Upsert is enforced by a partial unique index, not just app logic.** The
   `source_path` migration for scripts and prompts adds
   `CREATE UNIQUE INDEX … ON <table>(owner_email, source_path) WHERE source_path
   IS NOT NULL`. Import then uses `INSERT … ON CONFLICT(owner_email, source_path)
   DO UPDATE …`. NULL `source_path` (hand-authored rows) is exempt and may
   repeat. This makes idempotency a schema invariant (fail-loud), not a
   convention.

4. **The loopback `GET /list` returns the FULL `content_hash`**, unlike the MCP
   `list` tool which abbreviates to 8 chars. The consumer is code, not an LLM, so
   there is no brevity concern, and the full hash keeps the route useful for the
   deferred per-file sync manifest. This is the one intentional divergence from
   "mirrors the MCP list tool" — the *shape* (`{files:[{path,size,hash,rev,
   updated_at}], next_cursor}`) is identical; only the hash value is full.

5. **Validation lives on the consumer, behind an interface seam.** Each
   `Import`/`Sync` method takes an injected fetcher interface so it is unit-
   testable with a fake (no network, no live dropbox). UTF-8 + 1 MiB checks
   (import) happen in the service method, at the boundary.

---

## Phase 1 — dropbox: add the loopback `GET /list` route

**Goal:** give peers a loopback way to enumerate a mirror subtree, the twin of
the existing `GET /content`. This is the foundation sites (Phase 6) depends on;
scripts/prompts do not need it (they fetch a known path via `/content`).

**Files:**
- `dropbox/internal/dropbox/list.go` *(new)* — the `ListHandler` method, modeled
  exactly on `content.go`'s `ContentHandler`.
- `dropbox/cmd/dropbox/main.go` — register the route in the `Handlers` hook.
- `dropbox/internal/dropbox/list_test.go` *(new)* — handler tests.

**Steps:**
1. Read `dropbox/internal/dropbox/content.go` (the self-guard pattern) and
   `dropbox/internal/dropbox/service.go:95` (`Service.List(path, after, limit)
   ([]FileRow, error)`) and `FileRow` (fields: `Path`, `PathLower`, `Size`,
   `ContentHash`, `Rev`, `UpdatedAt`).
2. Add `func (s *Service) ListHandler() http.Handler`. Copy `ContentHandler`'s
   **primary guard verbatim**: 404 if `X-Owner-Email` or `X-Forwarded-Proto` is
   present (loopback-only). Then:
   - read query params `path` (prefix; empty/"/" = everything), `cursor`
     (opaque, = a prior `next_cursor`), `limit` (parse int; default 1000, clamp
     to `[1,1000]` — same bounds as `mcp/tools.go` `toolList`).
   - call `rows, err := s.List(path, cursor, limit)`; on error → `500` plain
     (this is a machine route; an internal error is not a path-confinement leak).
   - render JSON `{"files":[{"path","size","hash","rev","updated_at"}],
     "next_cursor"?}` where `hash` is the **full** `r.ContentHash` (decision 4),
     and `next_cursor` is `rows[len-1].PathLower` included **only** when
     `len(rows)==limit` (same "full page ⇒ more" rule as `toolList`).
   - set `Content-Type: application/json`.
3. In `main.go` `Handlers`, register **unauthenticated, not behind
   `RequireIdentity`**, next to the `/content` registration:
   `rt.Handle("GET /list", svc.ListHandler())`. Update the `Handlers` doc comment
   to mention `/list` alongside `/content`.
4. Tests (`list_test.go`), mirroring any existing `content` handler test:
   - 404 when `X-Owner-Email` set; 404 when `X-Forwarded-Proto` set.
   - happy path over a seeded test DB: files returned ordered by path, full hash
     present, prefix scoping works, `limit` clamps, pagination via `next_cursor`
     returns the next page and terminates (no `next_cursor` on a short page).

**Done-criteria:** `go test ./...` green in `dropbox/`; route returns the
documented shape; guard rejects proxied headers. No MCP surface change (the
`list` tool stays as-is).

**Context budget:** small — one new ~70-line file + one route line + one test
file. Read only `content.go`, `service.go` (List + FileRow), and `main.go`.

---

## Phase 2 — scripts: `source_path` schema

**Goal:** add the nullable `source_path` column + partial unique index that
makes import idempotent. Schema-only phase (kept separate so the migration is a
clean, reviewable unit and `bin/check-migrations` runs against it alone).

**Files:**
- new migration via `bin/new-migration scripts add_source_path` (timestamped;
  coexists with the legacy `001`–`004` numbered files).
- `scripts/internal/script/model.go` — add `SourcePath` to the `Script` struct.
- `scripts/internal/script/store.go` — thread the new column through
  `InsertScript`, `GetScript`, `ScriptForRun`, `ListScripts`, `UpdateScript`
  SELECT/INSERT/UPDATE column lists and the row scans.

**Steps:**
1. `bin/new-migration scripts add_source_path`. In the generated file:
   ```sql
   ALTER TABLE scripts ADD COLUMN source_path TEXT;
   CREATE UNIQUE INDEX idx_scripts_source
       ON scripts(owner_email, source_path) WHERE source_path IS NOT NULL;
   ```
2. `model.go`: add `SourcePath string` to `Script` (a plain string; "" ⇒ SQL
   NULL, written via a `nullStr`-style helper — check whether scripts' store
   already has one; prompts' store uses `nullStr` at `store.go:64`, add the
   equivalent if absent).
3. `store.go`: add `source_path` to every `scripts` column list and bind/scan
   site. Read it back as `sql.NullString` → `""` when NULL. INSERT binds
   `nullStr(sc.SourcePath)`. `UpdateScript` (`store.go:92`) does **not** touch
   `source_path` (hand-edits via the normal `update` tool must not clobber the
   import binding) — leave its SET list unchanged.

**Done-criteria:** `go test ./...` green in `scripts/`; `bin/check-migrations`
passes; existing CRUD round-trips `source_path` (add one store-test assertion).
**No new verb yet.**

**Context budget:** small — one migration, two files, mechanical column
threading. Read `model.go` and `store.go` only.

---

## Phase 3 — scripts: the `import` verb + loopback client

**Goal:** the `import(source_path)` MCP verb, end to end.

**Files:**
- `scripts/internal/script/dropbox.go` *(new)* — the `ContentFetcher` interface
  + an HTTP impl that does `GET <base>/content?path=…`.
- `scripts/internal/script/service.go` — add a `Fetcher ContentFetcher` field to
  `Service` and an `Import` method; add `Store` upsert support.
- `scripts/internal/script/store.go` — `UpsertScriptBySource` method.
- `scripts/internal/mcp/tools.go` — descriptor + dispatch case.
- `scripts/cmd/scripts/main.go` — read `DROPBOX_BASE_URL`, construct the client,
  set `svc.Fetcher`.
- tests: `scripts/internal/script/service_test.go` (Import with a fake fetcher),
  `scripts/internal/mcp/tools_test.go` (dispatch).

**Steps:**
1. `dropbox.go`:
   ```go
   type ContentFetcher interface {
       Fetch(ctx context.Context, path string) ([]byte, error)
   }
   ```
   plus `type httpFetcher struct{ base string; hc *http.Client }` whose `Fetch`
   GETs `base + "/content?path=" + url.QueryEscape(path)`, returns the body on
   200, a typed error on 404 (→ map to `ErrNotFound`) / other.
2. `service.go`: add field `Fetcher ContentFetcher` to `Service` (injected in
   main, like dropbox's `svc.Mirror`). Add:
   ```go
   func (s *Service) Import(ctx context.Context, owner, sourcePath, name string) (Script, error)
   ```
   - validate `sourcePath` non-empty (`ErrValidation`).
   - `data, err := s.Fetcher.Fetch(ctx, sourcePath)`.
   - reject non-UTF-8: `if !utf8.Valid(data) { return …ErrValidation }`.
   - reject `len(data) > 1<<20` (1 MiB) with `ErrValidation`.
   - derive name: `if name == "" { name = path.Base(sourcePath) }`.
   - default config `Interpreter: "python3"` (mirror `Create`).
   - call `s.store.UpsertScriptBySource(ctx, owner, sourcePath, name,
     string(data), cfg, s.nowStr())` returning the row.
3. `store.go` `UpsertScriptBySource`:
   ```sql
   INSERT INTO scripts (id, owner_email, name, body, config_json, source_path, created_at, updated_at)
   VALUES (?, ?, ?, ?, ?, ?, ?, ?)
   ON CONFLICT(owner_email, source_path) DO UPDATE SET
       name = excluded.name, body = excluded.body, updated_at = excluded.updated_at
   RETURNING id;
   ```
   (config left as-is on update — re-import refreshes body+name only). Generate
   the ULID for the INSERT arm with `ids.NewULID()`.
4. `mcp/tools.go` descriptor (append to the slice; add the dispatch case):
   ```
   desc(tool("import"), "Import a Dropbox-mirrored file as a script. 'source_path' is the file's path in the dropbox mirror (e.g. \"/scripts/nightly.py\"). Fetches the current mirror bytes over loopback, requires valid UTF-8 text under 1 MiB, and upserts on source_path: re-importing the same path updates the same script instead of creating a duplicate. 'name' defaults to the file's basename. Returns {script_id, name}.", obj(map[string]any{
       "source_path": typ("string"),
       "name":        typ("string"),
   }, "source_path")),
   ```
   Dispatch case `tool("import")`: parse `{SourcePath, Name}`, call
   `svc.Import`, return `{"script_id": sc.ID, "name": sc.Name}`.
5. `main.go`: in `buildService`/`Handlers` (around `scripts/cmd/scripts/main.go:188`),
   add `base := config.EnvOr(os.Getenv, "DROPBOX_BASE_URL", "http://127.0.0.1:3005")`
   and `svc.Fetcher = script.NewHTTPFetcher(base)` after `svc` is built.
6. Tests: `Import` happy path with a fake `ContentFetcher` returning known bytes
   → asserts row written, name derived; re-import same path → same `script_id`,
   body updated (no duplicate); non-UTF-8 → `ErrValidation`; >1 MiB →
   `ErrValidation`. MCP test: dispatch `import` maps to the service and returns
   `script_id`.

**Done-criteria:** `go test ./...` green in `scripts/`; the verb works against a
fake fetcher; re-import is idempotent.

**Context budget:** medium — ~6 files but each change is small and localized.
Read `service.go` (Create + NewService), `store.go` (InsertScript), `mcp/tools.go`
(create descriptor + dispatch), `main.go` (Handlers).

---

## Phase 4 — prompts: `source_path` schema

**Goal:** the prompts twin of Phase 2.

**Files:**
- migration via `bin/new-migration prompts add_source_path`.
- `prompts/internal/prompt/model.go` — add `SourcePath` to `Prompt`.
- `prompts/internal/prompt/store.go` — thread `source_path` through
  `InsertPrompt` (`store.go:55`), the `GetPrompt` SELECT (`store.go:77`), list,
  and update column lists + scans.

**Steps:**
1. `bin/new-migration prompts add_source_path`:
   ```sql
   ALTER TABLE prompts ADD COLUMN source_path TEXT;
   CREATE UNIQUE INDEX idx_prompts_source
       ON prompts(owner_email, source_path) WHERE source_path IS NOT NULL;
   ```
   (The live `prompts` table is from migration `006_prompt_redesign.sql`; columns
   today: id, owner_email, name, user_prompt, system_prompt, config_json,
   created_at, updated_at.)
2. `model.go`: add `SourcePath string` to `Prompt`.
3. `store.go`: add `source_path` to the insert/select/scan sites; bind via the
   existing `nullStr` helper (`store.go:64`); read as `sql.NullString`. The
   `Update` path must **not** overwrite `source_path`.

**Done-criteria:** `go test ./...` green in `prompts/`; `bin/check-migrations`
passes. No new verb yet.

**Context budget:** small — same shape as Phase 2.

---

## Phase 5 — prompts: the `import` verb + loopback client

**Goal:** `prompts.import(source_path)` mapping body → `user_prompt`.

**Files:**
- `prompts/internal/prompt/dropbox.go` *(new)* — `ContentFetcher` + HTTP impl
  (identical shape to scripts' Phase 3 client; intentional duplication per
  cross-cutting decision 1).
- `prompts/internal/prompt/service.go` — `Fetcher` field + `Import` method.
- `prompts/internal/prompt/store.go` — `UpsertPromptBySource`.
- `prompts/internal/mcp/tools.go` — descriptor + dispatch.
- `prompts/cmd/prompts/main.go` — `DROPBOX_BASE_URL` → `svc.Fetcher`.
- tests: `service_test.go`, `mcp/mcp_test.go`.

**Steps:**
1. Client: same as Phase 3 step 1, in package `prompt`.
2. `service.go` `Import(ctx, owner, sourcePath, name string) (Prompt, error)`:
   - fetch + UTF-8 + 1 MiB validation (same as scripts).
   - **default Config:** prompts' `Create` validates that `Config.Model` resolves
     to an Anthropic model — an empty config would fail. The import MUST supply a
     valid default. **Reuse prompts' existing default-model resolution** (inspect
     `service.go:99` `Create` and the config validation it calls; if there is a
     documented default model use it, otherwise set `Config{Model:"sonnet"}` —
     confirm the canonical default while implementing). Map file body →
     `UserPrompt`; `name` defaults to `path.Base(sourcePath)`; leave
     `SystemPrompt` empty.
   - upsert via `UpsertPromptBySource`.
3. `store.go` `UpsertPromptBySource`: `INSERT … ON CONFLICT(owner_email,
   source_path) DO UPDATE SET name=excluded.name, user_prompt=excluded.user_prompt,
   updated_at=excluded.updated_at RETURNING id`. Config/system_prompt untouched
   on update.
4. `mcp/tools.go` descriptor + dispatch:
   ```
   desc(tool("import"), "Import a Dropbox-mirrored file as a prompt. 'source_path' is the file's path in the dropbox mirror. Fetches the current mirror bytes over loopback (valid UTF-8 under 1 MiB) and maps the file body to the prompt's user_prompt; 'name' defaults to the basename. Re-importing the same source_path updates the same prompt (upsert); system_prompt and config keep their defaults. Returns {prompt_id, name}.", obj(map[string]any{
       "source_path": typ("string"),
       "name":        typ("string"),
   }, "source_path")),
   ```
5. `main.go`: `DROPBOX_BASE_URL` → `svc.Fetcher` (around
   `prompts/cmd/prompts/main.go:223`).
6. Tests: same matrix as scripts (happy/idempotent/non-UTF-8/too-large) plus an
   assertion that the resulting prompt has a valid (non-empty, validating)
   Config so `run` would not later reject it.

**Done-criteria:** `go test ./...` green in `prompts/`; imported prompts are
runnable (valid config); re-import idempotent.

**Context budget:** medium — mirrors Phase 3. The one novel concern is the
default-config resolution; read `service.go` `Create` + its config validation.

---

## Phase 6 — sites: `source_path` schema + the loopback client (with `/list`)

**Goal:** the sites-side foundations for `sync` — schema column, a client that
enumerates a subtree via `/list` and fetches bytes via `/content`, and a pure
reconcile routine — all unit-tested without the MCP wiring. (Split from the verb
wiring, Phase 7, to keep each subagent's context bounded; sites' files are the
largest in the repo.)

**Files:**
- migration via `bin/new-migration sites add_source_path`.
- `sites/internal/sites/store.go` — add `SourcePath` to `Site` + thread through
  `Create`/`Get`/`List`/`scanSite`; add a `SetSourcePath` (or fold into a
  `CreateOrReuse`) helper.
- `sites/internal/sites/sync.go` *(new)* — the dropbox client
  (`Fetch`+`List`) **and** the pure reconcile routine.
- `sites/internal/sites/sync_test.go` *(new)*.

**Steps:**
1. `bin/new-migration sites add_source_path`:
   ```sql
   ALTER TABLE sites ADD COLUMN source_path TEXT;
   ```
   (No unique index — the slug PK is already the key; `source_path` is recorded
   for symmetry/provenance per ADR Decision 2. The `sites` table is `STRICT`;
   `ADD COLUMN … TEXT` nullable is fine.)
2. `store.go`: add `SourcePath string` to `Site`; add `source_path` to the three
   SELECT column lists and `scanSite` (scan into `sql.NullString`). `Create`
   inserts `NULL`. Add `func (s *Store) SetSourcePath(ctx, name, sourcePath
   string) error` (UPDATE `source_path`, `updated_at`).
3. `sync.go` — the client:
   ```go
   type MirrorClient interface {
       List(ctx context.Context, prefix string) ([]MirrorFile, error) // follows next_cursor to completion
       Fetch(ctx context.Context, path string) ([]byte, error)
   }
   type MirrorFile struct { Path string; Size int64; Hash, Rev, UpdatedAt string }
   ```
   HTTP impl: `List` loops `GET <base>/list?path=<prefix>&cursor=<c>` following
   `next_cursor` until absent, accumulating `files`; `Fetch` is the same
   `GET /content` as the other consumers.
4. `sync.go` — pure reconcile (no HTTP, no DB):
   ```go
   // Reconcile mutates workingDir in place to match `desired` (the subtree files,
   // keyed by their path RELATIVE to source_path): overwrite every desired file
   // (fetched bytes), delete every working file absent from desired.
   func Reconcile(workingDir string, desired map[string][]byte, existingRel []string) (written, deleted int, err error)
   ```
   - For delete-absent: `existingRel` is the working tree's current relative file
     set (the caller obtains it by walking `workingDir` — reuse the
     `filepath.WalkDir` + `filepath.Rel`/`ToSlash` pattern from
     `sites/internal/mcp/files.go` `toolFileList`; **md5 is NOT needed** —
     overwrite-all means only the path set matters, confirming the handoff's open
     question).
   - confine every relative path under `workingDir` before writing/deleting
     (reuse the `confinePath` helper used in `files.go`).
   - write with `os.MkdirAll(dir)` + `os.WriteFile(…, 0o644)`; delete with
     `os.Remove`. `sync` is binary-safe — no UTF-8/size validation (ADR
     "Asserted defaults").
   - prune empty parent dirs left by deletes is optional v1 (note if skipped).
5. Tests (`sync_test.go`, temp dir, fake `MirrorClient`):
   - reconcile writes new files, overwrites changed ones, deletes working files
     absent upstream, leaves matching paths correct.
   - path-escape attempt in a desired key → error, nothing written outside root.
   - `List` pagination: a fake HTTP server returning two pages stitches into one
     set (test the HTTP client's cursor loop).

**Done-criteria:** `go test ./...` green in `sites/`; `bin/check-migrations`
passes; reconcile + client unit-tested. **No MCP verb yet.**

**Context budget:** medium-large. To stay bounded, the subagent reads
`store.go`, `layout.go`, and *only* `files.go` (for the walk + `confinePath`
patterns) — it does **not** need `tools.go` or `publish.go` in this phase.

---

## Phase 7 — sites: the `sync` verb wiring + composition root

**Goal:** the `sync(slug, source_path)` MCP verb tying Phase 6's pieces together.

**Files:**
- `sites/internal/mcp/tools.go` — descriptor + dispatch case.
- `sites/internal/mcp/sync.go` *(new, or in `files.go`)* — the `toolSync` handler.
- `sites/internal/mcp/mcp.go` — add a `client sites.MirrorClient` field to
  `Handler` + `NewHandler` param (or a settable field).
- `sites/cmd/sites/main.go` — `DROPBOX_BASE_URL` → construct the client → pass to
  `NewHandler`.
- tests: `sites/internal/mcp/sync_test.go`.

**Steps:**
1. `mcp.go`: add `client sites.MirrorClient` to `Handler`. Prefer a settable
   field over changing `NewHandler`'s signature if many callers/tests construct
   it; otherwise add a trailing param. Mirror whichever the file already favors.
2. `toolSync(ctx, raw)`:
   - parse `{SourcePath, Slug}`. Require `source_path`.
   - derive slug: `if slug == "" { slug = path.Base(sourcePath) }`; validate it
     against the slug grammar (reuse `sites` validation — `Store.Create` already
     enforces it; a pre-check gives a clean `validation` error rather than a
     create attempt). If derived slug is invalid and none given → validation
     error telling the caller to pass `slug` explicitly (ADR "Slug derivation").
   - **create-or-reuse:** `_, err := h.store.Get(ctx, slug)`; on `ErrNotFound`
     call `h.store.Create(ctx, slug)` and `os.MkdirAll(h.layout.WorkingDir(slug))`
     (match `toolCreate`'s row-then-dir order). Then
     `h.store.SetSourcePath(ctx, slug, sourcePath)`.
   - enumerate upstream: `files, err := h.client.List(ctx, sourcePath)`. Build
     `desired map[string][]byte` keyed by each file's path **relative to
     `sourcePath`**, fetching bytes via `h.client.Fetch(ctx, f.Path)`.
   - walk the working dir for `existingRel` (the path set).
   - `written, deleted, err := sites.Reconcile(h.layout.WorkingDir(slug),
     desired, existingRel)`.
   - **do NOT publish** (ADR Decision 7). Return
     `{"slug": slug, "written": written, "deleted": deleted}`.
3. `tools.go` descriptor (append; add dispatch `case tool("sync")`):
   ```
   desc(tool("sync"), "Sync a Dropbox-mirrored subtree into a static site's working tree. 'source_path' is the mirror folder to sync from (e.g. \"/sites/marketing\"); 'slug' names the target site and defaults to the source_path basename when that is a valid slug, else it is required. Creates the site if absent, then reconciles its working tree to match the subtree: every upstream file is (over)written and every working file absent upstream is deleted (the subtree owns the tree). Does NOT publish — call publish(tier) once to expose it; an already-published site updates live. Returns {slug, written, deleted}.", obj(map[string]any{
       "source_path": descTyp("string", "the mirror folder path to sync from"),
       "slug":        descTyp("string", "target site slug; defaults to the source_path basename"),
   }, "source_path")),
   ```
4. `main.go`: `base := config.EnvOr(os.Getenv, "DROPBOX_BASE_URL",
   "http://127.0.0.1:3005")`; build `sites.NewMirrorClient(base)`; pass/set on
   the handler (`sites/cmd/sites/main.go:43` is where store+layout are wired).
5. Tests (`sync_test.go`): with a fake `MirrorClient` and a temp `SITES_ROOT`:
   - sync to a **new** slug creates row + working tree + writes files; counts
     correct; site is **not** published.
   - sync to an **existing** slug reconciles: new files written, removed-upstream
     files deleted from working.
   - slug derivation: valid basename auto-derives; invalid basename with no
     `slug` → validation error.
   - re-running an already-published site updates `working/` (the served symlink
     reflects it) and still does not republish.

**Done-criteria:** `go test ./...` green in `sites/`; the verb is end-to-end over
a fake client; no publish side effect.

**Context budget:** medium-large — this is the heaviest verb. The subagent reads
`tools.go` (descriptor/dispatch + `toolCreate`), `mcp.go` (Handler/NewHandler),
`files.go` (walk/confine), `main.go`, and Phase 6's `sync.go`. Phase 6 having
already built reconcile + client keeps this phase to wiring + handler logic.

---

## Phase 8 — workspace verification

**Goal:** prove the whole suite still builds and the migration set is clean
before handing back. (Not a code phase; a gate.)

**Steps:**
1. `bin/check-migrations` — no duplicate versions, no edits to committed files.
2. Build every affected module with `GOWORK` as in local dev: `go build ./...`
   and `go vet ./...` in `dropbox/`, `scripts/`, `prompts/`, `sites/` (or at the
   workspace root if `go.work` covers them).
3. `go test ./...` across the four services once more.
4. Report a summary: new routes, new verbs, new migrations (by filename), and
   the new `DROPBOX_BASE_URL` env var for ops to set on the box.

**Do not git-commit** unless the user asks. The ADR + this plan are currently
untracked; committing the doc pair and the implementation is a separate,
user-authorized step.

---

## Sequencing summary

| Phase | Service | Deliverable | Depends on |
|---|---|---|---|
| 1 | dropbox | `GET /list` loopback route | — |
| 2 | scripts | `source_path` schema | — |
| 3 | scripts | `import` verb + client | 2 |
| 4 | prompts | `source_path` schema | — |
| 5 | prompts | `import` verb + client | 4 |
| 6 | sites | schema + client(`/list`) + reconcile | 1 |
| 7 | sites | `sync` verb wiring | 6 |
| 8 | all | build/vet/test/check-migrations gate | 1–7 |

Phases 2–3 (scripts) and 4–5 (prompts) are independent of Phase 1; only sites
(6–7) consumes `/list`. The order above front-loads `/list` so sites is
unblocked, then does the simpler import pair, then the heavier sync pair. A
subagent may be given a *pair* (e.g. "Phases 2+3") if its context allows, but the
default is one phase per subagent.

## Test strategy (summary)

- **Pure/unit first, behind interface seams.** Every `Import`/`Sync`/`Reconcile`
  takes an injected fetcher/client, so the core logic is tested with fakes — no
  live dropbox, no network. This is where the validation matrix (UTF-8, 1 MiB,
  idempotent upsert, delete-absent) is exercised.
- **HTTP clients** get a focused test against `httptest.Server` (cursor
  pagination for `/list`; 200/404 mapping for `/content`).
- **dropbox `/list` handler**: guard (404 on identity headers), shape, prefix,
  pagination — over a seeded test DB, mirroring the existing `/content` test.
- **MCP dispatch**: one test per new verb confirming the descriptor name routes
  to the service method and the result shape is returned.
- Each phase runs only *its* service's `go test ./...`; Phase 8 runs all four.

## Open decisions to confirm before execution

These are the cross-cutting calls this plan made on the ADR/handoff's behalf. If
any is wrong, it is cheaper to flip now than mid-execution:

1. **Per-service HTTP client (no shared module).** scripts ≈ prompts client code
   is duplicated by design (decision 1). Confirm you do not want a shared
   `dropboxclient` package up front.
2. **Partial unique index enforces upsert** (decision 3) rather than app-only
   logic. Confirm you want the DB-level invariant.
3. **`/list` returns the full `content_hash`** (decision 4), diverging from the
   MCP tool's 8-char abbreviation.
4. **prompts import default model** — Phase 5 reuses prompts' existing default;
   if there is none, it sets a default (`sonnet` proposed). Confirm the canonical
   default model for an imported prompt.
5. **`DROPBOX_BASE_URL` is env-only** (not in `ManifestExtras`), matching
   notify's `*_FEED_URL`. Confirm ops will set it on the box (default works for
   the standard loopback layout, so likely no action needed).
