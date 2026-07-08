# Phase 14 — In-process static server + `SiteDir` layout helper

*Realizes design Decision 17 (in-process serving) and 16 (SiteDir/SiteBase path helpers). Depends on Phase 08 (`internal/files` confinement).*

Add the code that lets the sites process serve site bytes, without yet removing
anything old (additive — the suite stays green).

- New package `internal/serve` with `Handler(root, urlPrefix string) http.Handler`
  serving files under one visibility root: file → `200` + extension `Content-Type`;
  directory with `index.html` → `200` serving it; directory without `index.html` →
  `404` (no listing, no `403`); path escaping the root via `..`/absolute → `404`
  (confined with `internal/files.ConfinePath`); missing path → `404`; directory
  without trailing slash → `301` to the slash form.
- New `Layout.SiteDir(public bool, slug string) string`
  (`<root>/public/<slug>` | `<root>/private/<slug>`) and
  `Layout.SiteBase(public bool) string`, added **alongside** the existing
  `WorkingDir`/`ServedDir` helpers (not removed here).
- Wire two ungated routes in `cmd/sites/main.go`'s `Handlers`:
  `GET /public/` → `serve.Handler(layout.SiteBase(true), "/public/")` and
  `GET /private/` → `serve.Handler(layout.SiteBase(false), "/private/")`.

**Done when:** `cd sites` and `go build ./...`, `go vet ./...`, `gofmt -l .`
(no output), `go test ./...` all pass, with named tests over a `t.TempDir()` root
covering R-QZX3-2WYW, R-R14Z-GOPL, R-R2CV-UGGA, R-R3KS-886Z, R-R4SO-LZXO,
R-R60K-ZROD (the six serve behaviors) and R-QV1H-JU04 (the `SiteDir` path formula
for both visibilities).
