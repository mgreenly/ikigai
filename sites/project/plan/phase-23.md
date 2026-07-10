# Phase 23 — `landing.js` pure logic (fuzzy filter, sort, paginate, state) + goja tests

*Realizes design Decision 22 (the pure-function control layer for the landing listing — filter/sort/paginate/state — and its goja test strategy). Depends on Phase 20 (the landing listing this script will drive).*

Add the landing page's client JavaScript as a self-contained module of **pure
functions over data**, and prove every one of them by loading the real shipped
file into `goja` from a Go test. This phase touches **no** `.html`, no handler,
no DB, no nginx — it only adds the JS file, the test-only `goja` dependency, and
the goja test. The JSON data island the controller will consume, and the markup
that hosts the controls, arrive in Phase 24.

- **`sites/share/www/static/landing.js`** — the module described in D22. Define,
  at top level, the pure functions `filterSites(rows, query)`,
  `sortRows(rows, key, dir)`, `paginate(rows, page, size)`, `nextSort(state, key)`,
  `defaultState()`, `reduce(state, action)`, and `computeView(rows, state)`,
  and attach them to `globalThis.SitesLanding`. Place the DOM bootstrap
  (`initController`, event wiring) behind an `if (typeof document !== 'undefined')`
  guard so the file loads inertly under a non-browser runtime. Semantics per D22:
  subsequence-fuzzy case-insensitive predicate that preserves input order; sort
  by `name`/`createdAt`(via `createdAtSort`)/`createdBy` with a slug tie-break and
  a two-state direction toggle; page size 10; `defaultState` = created-at-desc /
  page 1; `reduce` resets page to 1 on `setQuery`/`setSort`; `computeView` derives
  `showControls`/`empty`/`noMatch`/`showPager`/`pageCount`/range/label and clamps
  the page.
- **`sites/go.mod` / `sites/go.sum`** — add `github.com/dop251/goja` as a
  (test-only) require; it is pure Go, needs no `replace`, and keeps
  `go test ./...` the whole green bar.
- **`sites/internal/web/landing_js_test.go`** (or a `cmd/sites` test package) —
  a Go test that reads `share/www/static/landing.js`, evaluates it in a fresh
  `goja.Runtime`, and calls the exposed `SitesLanding.*` functions with fixed
  inputs, one assertion block per id below. No DOM is provided; the controller
  bootstrap stays inert.

**Done when:** the sites suite is green (`cd sites && go build ./...`, `go vet
./...`, `gofmt -l .` prints nothing, `go test ./...`, `bin/check-migrations sites`),
AND each id is covered by a goja assertion over the real `landing.js`:
- R-HU67-LJBW — `filterSites(["docs","dashboard","blog"], "dsb")` returns exactly `["dashboard"]` (subsequence, not substring).
- R-HVE3-ZB2L — `filterSites` is case-insensitive: `DOCS`→`docs`, `DsB`→`dashboard`.
- R-HWM0-D2TA — `filterSites(rows, "")` returns all rows in input order.
- R-HXTW-QUJZ — `filterSites(["zebra-app","able-app"], "app")` returns them in input order (no re-rank).
- R-HZ1T-4MAO — `sortRows(rows,'name','desc')` is the exact reverse of `sortRows(rows,'name','asc')` for distinct slugs.
- R-I1HL-W5S2 — `sortRows(rows,'createdAt','desc')` orders by `createdAtSort` (real chronology) even when the display strings do not sort lexically the same way.
- R-I2PI-9XIR — `sortRows(rows,'createdBy','asc')` orders by creator ascending.
- R-I3XE-NP9G — rows sharing a `createdAtSort` come back in slug order deterministically (stable tie-break).
- R-I55B-1H05 — `nextSort` flips direction on the active key and switches to a new key at `asc`.
- R-I6D7-F8QU — `paginate(34rows, p, 10)`: page 1 → first 10, page 4 → last 4, page 5 → `[]`.
- R-I7L3-T0HJ — `computeView` sets `showPager` true iff the filtered set > 10 (10 → false, 11 → true).
- R-I8T0-6S88 — `computeView` derives `pageCount`/label/range and clamps: 34 rows `page:4` → `pageCount:4`, range `31–34 of 34`; `page:9` clamps to page 4.
- R-IA0W-KJYX — `computeView` returns `{showControls:false,empty:true}` for zero rows and `{showControls:true,noMatch:true,showPager:false}` for a query matching none.
- R-IB8S-YBPM — `defaultState()` equals `{query:'',sortKey:'createdAt',dir:'desc',page:1}` and `reduce(any,{type:'clear'})` returns that same default.
- R-7V8B-GA0T — `reduce` resets `page` to 1 on `setQuery` and `setSort`, but not on `setPage`.
