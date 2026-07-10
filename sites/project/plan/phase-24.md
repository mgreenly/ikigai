# Phase 24 — Wire the control layer into the landing page: JSON island, sortable/hidden-until-JS controls, DOM controller

*Realizes design Decision 19 (the JSON data island + `createdAtSort` view-model field), Decision 6 (the control bar + sortable-header hooks, hidden-until-JS), and Decision 22's DOM wiring (the controller and the `landing.js` script reference). Depends on Phase 23 (the `landing.js` pure functions the controller calls) and Phase 20 (the current landing render).*

Make the landing page interactive by connecting Phase 23's tested pure functions
to the DOM: emit the site rows as a JSON island, render the (hidden-until-JS)
search / sort / pager / clear controls, add the `initController` DOM code to
`landing.js`, and reference the script from the page. The store, migrations, MCP
surface, and nginx are untouched; only the landing render and its client wiring
change.

- **`sites/cmd/sites/main.go`** (landing handler / view model) — add
  `CreatedAtSort string` (RFC3339 UTC) to `siteRow`, populated alongside the
  existing display `CreatedAt` from the same source time. No other handler change;
  the visible-table render is unchanged.
- **`sites/share/www/landing.html`** — (1) emit the `Sites` slice a second time as
  `<script type="application/json" id="sites-data">…</script>`, each element
  `{slug,url,public,createdBy,createdAt,createdAtSort}`, the `url` the same
  server-computed anchor `href`; empty slice → `[]`. (2) Add, between the lead and
  the table, the **search input**, **Clear** control, and **Prev/Next pager**
  region with a page readout — each marked hidden-until-JS (`hidden` / `js-only`
  class, with a root `no-js`→`js` flip). (3) Put a `data-sort-key` sort hook
  (`name`/`createdBy`/`createdAt`) on the Slug, Creator, and Created headers (not
  Visibility). (4) Reference the script: `<script src="static/landing.js" defer>`.
- **`sites/share/www/static/landing.js`** — fill in `initController` (the DOM half
  behind the `document` guard from Phase 23): parse `#sites-data`, hold `state`,
  wire input/header/pager/clear/Escape listeners through `reduce`, and on each
  change stamp `computeView(rows, state)` into the DOM (rebuild `<tbody>`, set the
  pager label/enabled state, toggle the no-match message, reveal/hide controls).
  It adds **no logic** — all decisions come from the pure functions.
- **Tests** — extend the `share/www` render tests (over the repo-real tree via
  `appkit/web`, and the `GET /{$}` handler + real store substrate) for the island
  and control markup below. The controller's runtime DOM behavior is not driven
  here (structural only), per D22.

**Done when:** the sites suite is green (`cd sites && go build ./...`, `go vet
./...`, `gofmt -l .` prints nothing, `go test ./...`, `bin/check-migrations sites`),
AND each id is covered:
- R-IDOL-PV70 — rendering `landing.html` with two sites emits a `<script type="application/json" id="sites-data">` parsing to two objects each carrying `slug`/`url`/`public`/`createdBy`/`createdAt`/`createdAtSort`; an empty slice emits an island parsing to `[]`.
- R-IEWI-3MXP — driving `GET /{$}` (fixed `baseURL`) over a store with public `X` and private `Y`, each island element's `url` is byte-identical to that row's visible anchor `href` (`<baseURL>public/X/`, `<baseURL>private/Y/`).
- R-IG4E-HEOE — the rendered page contains, between the lead and the table, a search input, a Clear control, and a Prev/Next pager with a page-readout; and the Slug/Creator/Created headers carry `data-sort-key` of `name`/`createdBy`/`createdAt` while Visibility carries none.
- R-IHCA-V6F3 — the search input, Clear control, and pager region are each rendered hidden-until-JS (a `hidden` attribute or `js-only` class), so the no-JS render exposes no active search/sort/pager/clear affordance.
- R-ICGP-C3GB — the shipped tree contains `share/www/static/landing.js` and the rendered `landing.html` loads it via `<script src="static/landing.js">` (deferred).
