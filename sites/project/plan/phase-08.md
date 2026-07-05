# Phase 08 — `internal/files`: confined filesystem operations as native Go

*Realizes design Decision 10. Adds one new package `sites/internal/files/` (plus
its `_test.go`). Depends on no earlier phase — it is a standalone, pure-Go package
with its own unit tests; the existing `agentkit` bridge in `internal/mcp` stays
in place and untouched this phase (Phase 09 removes it). No schema change, no MCP
wiring change.*

This phase creates the native home for every confined filesystem operation sites
performs. The function bodies are the same substance as `prompts`'
`internal/tools` workers (path confinement, `os.ReadFile` + line slicing,
`strings.Replace`, `filepath.Glob`, the grep walk, md5 hashing), reshaped as plain
Go functions over Go types — **no `agentkit`, no JSON, no agent framing**. Nothing
calls the package yet; it stands on its own unit tests. The package is the single
trust boundary for file access once Phase 09 wires it in.

Add `sites/internal/files/` with:

- `var ErrEscapes error` — the sentinel every confinement rejection wraps.
- `func ConfinePath(root, p string) (string, error)` — ported from `prompts`'
  symlink-resolving confinement (resolve `p` under `root`, follow symlinks, permit
  an absolute path that still lands under `root`, permit a not-yet-existing path
  under `root`; any escape returns an error wrapping `ErrEscapes`), plus its
  helpers (`resolveExisting`, `searchPath`, `sandboxRoot`).
- `func Read(root, path string, offset, limit int) (string, error)` (+ `sliceLines`).
- `func Write(root, path, content string, append bool) error` — truncate by
  default, append when `append`, create parent dirs.
- `func Edit(root, path, oldStr, newStr string, replaceAll bool) (int, error)` —
  returns the replacement count; missing `oldStr` is an error with no write.
- `func Glob(root, pattern, path string) ([]string, error)` — relative matches,
  confined.
- `func Grep(root, pattern, path, glob string) ([]Match, error)` with
  `type Match struct { Path string; Line int; Text string }`.
- `func List(root, scope string) ([]FileInfo, error)` with
  `type FileInfo struct { Path string; Size int64; Md5 string }` — walk + size +
  md5, scoped to a confined `scope` subdir when given.
- `func Mkdir(root, path string) error`.

The package marshals no JSON and imports only the standard library. It uses the
minimal Read/Grep form (no `output_mode`/`-i`/`-A`/`-B`/`type`/`cat -n`).

**Done when:** the suite is green (per design *Conventions*: `cd sites && go build
./...`, `cd sites && go vet ./...`, `cd sites && gofmt -l .` prints nothing,
`cd sites && go test ./...`, and `bin/check-migrations sites` all succeed with zero
failures) and these ids are covered by clearly-named tests in `internal/files`
(each using `t.TempDir()` as the sandbox root; no network, no running suite):

- **R-027Y-BQ1I** — `ConfinePath` rejects a relative `../…` escape and an
  absolute-outside path, each returning an error for which
  `errors.Is(err, files.ErrEscapes)` holds. *(unit test)*
- **R-03FU-PHS7** — `ConfinePath` accepts a relative in-root path, an absolute
  path resolving under root, and a not-yet-existing path under root (create case),
  returning a confined absolute path and no error. *(unit test)*
- **R-04NR-39IW** — `ConfinePath` rejects a symlink placed under root but pointing
  outside it (error wraps `ErrEscapes`), proving symlink resolution. *(unit test)*
- **R-05VN-H19L** — `Read` returns the whole file for `offset=0,limit=0` and
  exactly the requested 1-based line window otherwise. *(unit test)*
- **R-073J-UT0A** — `Edit` replaces the first occurrence by default and all when
  `replaceAll`, returns the count, and errors without writing on a missing
  `oldStr`. *(unit test)*
- **R-08BG-8KQZ** — `Glob` returns matching paths relative to the search base and
  never a path outside root. *(unit test)*
- **R-09JC-MCHO** — `Grep` returns `[]Match` with the relative path, 1-based line
  number, and matching text, confined under root. *(unit test)*
- **R-0AR9-048D** — `Write` truncates by default, appends when `append=true`
  (creating the file if missing), and creates missing parent directories.
  *(unit test)*
- **R-0D71-RNPR** — `List` returns one `FileInfo` per regular file with its
  working-root-relative path, byte size, and md5 hex, scoped to a confined
  `scope`. *(unit test)*
- **R-0EEY-5FGG** — `Mkdir` creates the (nested) directory under root and rejects
  an escaping path with an error wrapping `ErrEscapes`. *(unit test)*
