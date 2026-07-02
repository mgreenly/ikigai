# Phase 13 — Service list adopts the shared list chrome + copyable URLs; drop section intros

*Realizes design Decision 5 (`R-OF1Q-VEDC`, `R-OG9N-9641`, `R-OHHJ-MXUQ`, all new;
refines the existing R-DB12-LINK markup). Touches `ui/html/index.html` (signed-in
`{{if .Owner}}` branch: remove two `.section-intro` paragraphs, replace the services
`<table>` with the shared `.list`/`.row` markup + a per-row copy button),
`ui/static/app.css` (delete the one-off `.services-table` rules, add the small
`.service-row` copy-button/URL treatment), `ui/static/app.js` (generalize the
`.copy-btn` source lookup to also fire inside a service row), and the landing server
tests. No route, no view-model change (`serviceRows` already yields `.Name`, `.Href`,
`.URL`), no migration, no schema.*

**1. Remove both section intro paragraphs (D5 / R-OHHJ-MXUQ).** In
`ui/html/index.html`, in the **logged-in `{{if .Owner}}` branch only**, delete these
two lines verbatim:
- the connect-agent intro `<p class="section-intro">Paste the line for your agent into its terminal to wire this box's MCP services into your install.</p>`
- the services intro `<p class="section-intro">The raw resource URLs, for wiring any other MCP client by hand.</p>`

Leave both `.section-head` blocks (the `<h2>` + `.rule`) untouched.

**2. Replace the services `<table>` with the shared list chrome + copy buttons
(D5 / R-OF1Q-VEDC, R-OG9N-9641; refines R-DB12-LINK).** In `ui/html/index.html`,
inside the `{{if .Services}}` section, replace the entire `<table class="services-table">…</table>`
with the shared `.list`/`.row` idiom (the same chrome the profile PAT/grant lists use).
Each row: the linked **name** as `.name` (left), the raw MCP **URL** as a copyable
`<code class="meta service-url">` (middle), and a reused `.copy-btn` (right) — filling
the standard `.row` `1fr auto auto` grid. There is **no** `<thead>`/header row:

```html
<ul class="list services-list">
  {{range .Services}}
  <li class="row service-row">
    <a href="{{.Href}}" class="name">{{.Name}}</a>
    <code class="meta service-url">{{.URL}}</code>
    <button type="button" class="copy-btn" aria-label="Copy {{.Name}} MCP URL">
      <svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor"
           stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
        <rect x="9" y="9" width="11" height="11" rx="2"/><path d="M5 15V5a2 2 0 0 1 2-2h10"/></svg>
      <span class="copy-label">Copy</span>
    </button>
  </li>
  {{end}}
</ul>
```

The name link **must** carry `class="name"` (so the `.row .name` weight applies and to
keep the accent link color) and the URL **must** be inside a `<code>` (the copy JS reads
`code.textContent`). Keep the SVG copy icon identical to the connect-snippet buttons.

**3. Delete the one-off table CSS; add the service-row treatment (D5 / R-OF1Q-VEDC,
R-OG9N-9641).** In `ui/static/app.css`:
- **Delete** the entire `/* ---- Services table --- */` block (the `.services-table`,
  `.services-table th, .services-table td`, `.services-table tbody tr:last-child td`,
  `.services-table th`, `.services-table td`, and `.services-table tbody tr:hover`
  rules). It is now unused.
- Add a small **services-list** block reusing the shared `.list`/`.row` chrome (do not
  re-declare the surface/border/grid — those come from `.list`/`.row`). Only the
  copyable URL and the in-row copy button need adjusting from their defaults:
  - `.service-url { min-width: 0; overflow-x: auto; white-space: nowrap; }` so a long
    MCP URL never blows out the row.
  - The base `.copy-btn` is styled for the *snippet* (a `border-left` divider + a
    `--color-surface` fill, appropriate flush inside a `.snippet` box). Inside a list
    row that chrome is wrong, so override it: `.service-row .copy-btn { border-left: none;
    background: transparent; border-radius: var(--radius); padding: var(--space-1) var(--space-2); }`
    and `.service-row .copy-btn:hover { background: var(--color-surface-2); }`. The
    `.is-copied` accent state and icon sizing carry over from the base `.copy-btn` rules
    unchanged.

**4. Generalize the copy-to-clipboard source lookup (D5 / R-OG9N-9641).** In
`ui/static/app.js`, the copy handler currently finds its text via
`btn.closest(".snippet")`. Widen that one selector so the same handler also works for
the in-row copy buttons, whose copyable `<code>` lives in a `.service-row` rather than a
`.snippet`:

```js
const scope = btn.closest(".snippet, .service-row");
const code = scope && scope.querySelector("code");
```

(Rename the local `snippet` → `scope`; everything else in the handler — the label swap,
the `.is-copied` toggle, the fallback `copyText` — is unchanged. `querySelector("code")`
inside a `.service-row` matches the URL `<code>`, not the name `<a>`.)

**5. Update the landing tests (D5).** Co-locate in `internal/server/*_test.go`,
`package server`, driving the real route table via the existing `httptest` harness with
a live session, mirroring the current landing test setup:
- In `internal/server/landing_composition_test.go`, `TestLandingServiceNameLinksToMount`
  (**R-DB12-LINK**): update the expected name-link string to the new markup
  `<a href="/srv/crm/" class="name">ikigenba_crm</a>` (the `class="name"` is now present).
  The raw-URL assertion (`https://int.ikigenba.com/srv/crm/mcp`) stays as-is.
- Add a new test `TestLandingServiceListChrome` in the same file, on the signed-in
  landing:
  - **R-OF1Q-VEDC** — assert the body contains `<ul class="list services-list"` and
    `<li class="row service-row">`, and contains **no** `services-table`, **no**
    `<thead`, and **no** `<th>` (the table markup is gone).
  - **R-OG9N-9641** — assert the crm row exposes a copy control and holds the URL in a
    `<code>`: the body contains `<code class="meta service-url">https://int.ikigenba.com/srv/crm/mcp</code>`
    and `aria-label="Copy ikigenba_crm MCP URL"`.
  - **R-OHHJ-MXUQ** — assert the signed-in landing body contains **no**
    `class="section-intro"`.
- In `internal/server/connect_section_test.go`, `TestIndexConnectSectionLoggedOut`:
  change the stale `services-table` guard marker to `services-list` (so the negative
  "not shown to logged-out visitor" assertion still tests a real marker); the
  `ikigenba_crm` half of that check is unchanged.

**Done when:** the suite is green — `cd dashboard && go build ./...`, `go vet ./...`,
`gofmt -l .` (no output), `go test ./...`, and `bin/check-migrations dashboard` (run
from the repo root) all succeed with zero failures (per design *Conventions*) — and
these ids are covered:

- **R-OF1Q-VEDC** — the signed-in landing service list is rendered as
  `<ul class="list services-list">` of `<li class="row service-row">` (shared list
  chrome), with no `<table>`/`<thead>`/`<th>` header markup.
- **R-OG9N-9641** — each service row shows its MCP URL in a `<code>` alongside a reused
  `.copy-btn` (with a service-naming `aria-label`) that copies that URL.
- **R-OHHJ-MXUQ** — the signed-in landing renders no `.section-intro` paragraph in
  either the connect-agent or the service-list section.
