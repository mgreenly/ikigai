# Phase 25 — Make the landing controls real: rearranged layout, honest hiding, sort affordances, and the implemented DOM controller

*Realizes design Decision 6 (rewritten: filter bar above the table, pager below it, `aria-sort` caret/pointer CSS, `[hidden]` guaranteed to hide, the no-match node) and Decision 22's rewritten `initController` (the branchless DOM transcription — runtime proof deferred to Phase 26/D23). Depends on Phase 24 (the island, controls markup, and script reference this phase rearranges and animates).*

Phase 24 shipped the control layer structurally wired but functionally dead:
`initController` was an empty stub, so v0.13.0 renders inert controls (and,
because `.controls`/`.pager` declare `display: flex`, renders them *visibly*
inert — the `hidden` attribute never won). This phase makes the page actually
work and fixes the layout to the operator's directive. HTML, CSS, and JS only —
the handler, view model, island, store, migrations, MCP surface, and nginx are
untouched.

- **`sites/share/www/landing.html`** — (1) keep the search input + Clear control
  between the lead and the table; **move the pager region to directly after the
  table**. (2) Add a hidden no-match message element adjacent to the table
  (e.g. `<p class="no-match" hidden>No sites match.</p>`). (3) CSS: add
  `[hidden] { display: none !important; }` so the attribute beats the
  `.controls`/`.pager` `display: flex`; add `cursor: pointer` (+ hover
  affordance) on `th[data-sort-key]`; add the caret rules
  `th[aria-sort="ascending"]::after` → `" ▲"` and
  `th[aria-sort="descending"]::after` → `" ▼"`. The server markup carries no
  `aria-sort`.
- **`sites/share/www/static/landing.js`** — implement `initController` per D22:
  parse `#sites-data` once, `state = defaultState()`, reveal controls/pager per
  the view model (`showControls`/`showPager`; zero sites → everything stays
  hidden), wire the listeners (search `input` → `setQuery`; Escape →
  `setQuery('')`; `th[data-sort-key]` click → `setSort`; Prev/Next → `setPage`;
  Clear → `clear`), and on every action dispatch `reduce` → `computeView` →
  stamp the DOM: rebuild `<tbody>` from `view.rows` (slug anchor from each
  row's `url`, visibility, creator, `createdAt` display), set the `Page X of Y`
  readout, toggle pager/no-match visibility, and stamp `aria-sort` on the
  active header (removing it from the others). **No branching logic of its
  own** — every decision comes from `computeView`. The pure functions and their
  goja tests are untouched.
- **Tests** — update/extend the structural render tests (repo-real `share/www`
  via `appkit/web`, and the `GET /{$}` handler + real store substrate where
  document order matters) for the three ids below. Phase 24's tests asserting
  the old pager-above placement are updated to the new order (their retired ids
  R-IG4E-HEOE / R-IHCA-V6F3 have left the design; the frozen phase-24 record
  keeps them). The controller's runtime behavior is deliberately **not** proven
  here — that is Phase 26's browser gate; this phase's bar is structural truth
  plus a green suite (the goja tests must still pass with the controller code
  present, proving the `document` guard still keeps the module loadable outside
  a browser).

**Done when:** the sites suite is green (`cd sites && go build ./...`, `go vet
./...`, `gofmt -l .` prints nothing, `go test ./...`), AND each id is covered:
- R-83NK-DUW1 — rendering `landing.html` (non-empty `Sites`): the search input and Clear sit **between the lead and the table**, the Prev/Next pager region (with page readout) sits **after the table** (document-order assert), a hidden no-match element is present, and the Slug/Creator/Created headers carry `data-sort-key` `name`/`createdBy`/`createdAt` while Visibility carries none.
- R-84VG-RMMQ — the interactive controls carry `hidden` in the server render **and** the page styles contain a `[hidden]` rule forcing `display: none` with precedence over the controls' own `display` declarations (the v0.13.0 visible-dead-controls defect is structurally excluded).
- R-863D-5EDF — the page styles contain the `th[aria-sort="ascending"]`/`th[aria-sort="descending"]` caret rules and a `cursor: pointer` rule for `th[data-sort-key]`; the server markup contains no `aria-sort` attribute.
