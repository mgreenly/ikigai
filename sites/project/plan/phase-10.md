# Phase 10 — Fix `Glob`'s recursive `**` matching in `internal/files`

*Realizes design Decision 10 (the newly-added Glob-matching ids R-3ZP8-T0GP and
R-40X5-6S7E). Depends on Phase 08 (the `internal/files` package and its existing
Glob tests). Touches only `sites/internal/files/` (`files.go` + `files_test.go`);
no MCP-layer, `go.mod`, or cross-package change.*

`internal/files.Glob` currently delegates to `filepath.Glob`, whose `*` cannot
cross `/` and whose `**` collapses to a single-segment match — so recursive
patterns (`**/*.css`, `**/*.html`) return `[]` even when matching files exist at
the base or nested below it, while single-segment and explicit-path patterns
(`*.html`, `assets/css/*.css`) work. This phase makes `Glob` match a path
segment at a time with true `**` recursion.

Rework **`internal/files/files.go`** so `Glob`:
- walks the confined search base with `filepath.WalkDir` (which does not follow
  symlinks, keeping matches inside the sandbox) instead of delegating to
  `filepath.Glob`;
- tests each entry's **base-relative** path against the pattern one
  `/`-separated segment at a time — `*`/`?`/`[…]` via `filepath.Match` (never
  crossing `/`), and `**` matching any run of segments including none;
- keeps every existing guarantee: results are base-relative, `filepath.ToSlash`,
  sorted; the empty result is `[]string{}` not `nil`; and a `path`/pattern that
  escapes the search base still returns an error wrapping `files.ErrEscapes`.

The three existing Glob tests (`TestGlobReturnsSearchBaseRelativeMatches`,
`TestGlobReturnsTypedStrings`, and the `Glob` arm of
`TestOperationsShareErrEscapesConfinement`) must stay green unchanged — they pin
the base-relative, typed-`[]string`, and confinement behavior the rework
preserves.

Add id-tagged tests to **`internal/files/files_test.go`** over a `t.TempDir()`
tree containing a base-level `a.css` and `index.html`, a nested
`assets/css/style.css`, `assets/js/app.js`, and a deeper `deep/a/b/c.css`:

- **R-3ZP8-T0GP** — `Glob(root, "**/*.css", "")` returns exactly the base-relative
  paths of *all* `.css` files at every depth (`a.css`, `assets/css/style.css`,
  `deep/a/b/c.css`); `Glob(root, "**/*.html", "")` returns `["index.html"]`
  (proving `**` matches zero intermediate segments); and `Glob(root, "assets/**",
  "")` returns every file beneath `assets/`.
- **R-40X5-6S7E** — `Glob(root, "*.css", "")` returns only `["a.css"]` (never a
  nested match), and `Glob(root, "**/*.css", "assets")` returns base-relative-to-
  `assets` paths (`["css/style.css"]`), confirming single-segment `*` does not
  cross `/` and `path` scoping is preserved. Include a no-match case
  (`Glob(root, "**/*.md", "")`) asserting `[]string{}` (empty, not nil).

**Done when:** the suite is green — `cd sites && go build ./...`, `cd sites && go
vet ./...`, `cd sites && gofmt -l .` prints nothing, `cd sites && go test ./...`,
and `bin/check-migrations sites` all succeed with zero failures — and the two new
ids are covered by the clearly-named tests above:

- **R-3ZP8-T0GP** — `**/*.css` returns the base-level, mid-depth, and deeply
  nested `.css` files; `**/*.html` returns the base-level `index.html`;
  `assets/**` returns every file under `assets/`. A `[]` result, or one omitting
  the base-level or a nested match, fails it.
- **R-40X5-6S7E** — `*.css` matches only the base-level `a.css` and never a nested
  `.css`; `**/*.css` scoped to `path:"assets"` returns `assets`-relative paths;
  `**/*.md` returns `[]string{}`. A single-segment pattern returning a nested
  match fails it.
