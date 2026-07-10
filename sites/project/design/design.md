# sites — Design

**Authority: shape and its proof.** This document and the `project/design/`
directory it heads own *how* sites is built and *how each behavior is proven*.
The product (`project/product/product.md`) owns the *why*, *for whom*, and the
user-facing promises; design states the **exact, checkable form** of those
promises and never re-declares the why. Design *uses* the product's contractual
constants by value (a site is public-or-private; a site that exists is served;
sites serves every byte under its mount; the visibility gate is nginx's; the
landing page is session-gated and shows version + site list; the visual system is
Carbon) but does **not** own them. This is the single, current statement of the
architecture — it is rewritten in place to stay true (stale decisions are
removed, not stacked); the history of how it got here lives in the plan.

> **Scope.** This design covers sites' whole current surface: the slug/visibility
> domain (`internal/sites`), the in-process static server (`internal/serve`), the
> confined file tools (`internal/files`), the MCP tool table (`internal/mcp`), the
> embedded landing page (`share/www`), the migration set (`internal/db`), and the
> nginx fragment (`sites/etc/nginx.conf`). All of these live under `sites/`;
> nothing outside `sites/` is named or changed. Cross-service facts (the dashboard
> session validator `/_session-authn`, the dropbox mirror, the shared `registry`)
> are fixed external contracts this design consumes.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is
  **no separate requirements document**.
- Design's responsibility for ids ends at minting them into this doc. How
  coverage is measured, what counts as a covered id, and when the work is "done"
  are **not** design's concern — downstream phases own that.

## Conventions

Shared facts every Decision leans on:

- **Language / toolchain:** Go **1.26**, single module `module sites` rooted at
  `sites/`. Pure-Go SQLite driver `modernc.org/sqlite` (no cgo).
- **Build / typecheck command:** `cd sites && go build ./...` and
  `cd sites && go vet ./...`. The production build adds
  `CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off -buildvcs=false` (driven by
  `bin/ship sites`).
- **Test command:** `cd sites && go test ./...`. **"The suite is green"** means:
  `cd sites && go build ./...`, `cd sites && go vet ./...`, `cd sites && gofmt -l .`
  (no output), and `cd sites && go test ./...` all succeed with zero failures.
  **Green includes the browser wiring test (D23) and therefore hard-requires a
  `google-chrome` binary on `PATH`** of the box running the suite (present:
  `/usr/bin/google-chrome`). No Chrome → the suite is red, never skipped. The
  harness may retry the browser *launch* once; scenario assertions are never
  retried.
- **Formatting:** `gofmt`-clean; `gofmt -l .` must print nothing.
- **Migrations are timestamped and immutable.** Schema lives under
  `sites/internal/db/migrations/`, applied forward-only by the appkit runner and
  keyed per file. A committed migration is **frozen** — a schema change is a
  **new** migration created with `bin/create-migration sites <name>` (which stamps
  a UTC `YYYYMMDDHHMMSS_<slug>.sql` version); never hand-name or edit one. The
  hosting-model change adds new migrations; `002_sites.sql` and
  `20260609135943_add_source_path.sql` stay frozen.
- **Module wiring:** `appkit`, `eventplane`, and `registry` are committed in-repo
  replace-siblings. sites resolves its own port and the dropbox mirror address by
  name through `registry` (D9). No `agentkit` dependency (D10/D11): confined
  file-tool logic lives in the native `internal/files` package. Two **test-only**
  dependencies (pure Go, no cgo, imported only from `*_test.go`, linked into no
  shipped binary — enforced mechanically by D23's import-graph id):
  `github.com/dop251/goja` (an ES engine: the landing page's client JavaScript
  `share/www/static/landing.js`, D22, is written as pure functions and exercised
  by loading the real shipped file into goja from a Go test) and
  `github.com/chromedp/chromedp` (drives the headless Chrome for D23's single
  browser wiring test over the DevTools protocol — no node/npm toolchain; see
  `project/research/research.md`).
- **The chassis owns the server.** sites is `appkit.Main(appkit.Spec{…})`:
  `App:"sites"`, `Mount:"/srv/sites/"`, `Port:registry.MustPort("sites")` (== 3004),
  `MCP:true`, `WWW:true` (chassis loads/serves the `share/www` landing template and
  `/static/` assets), `Migrations:db.FS`. sites is **not** an event-plane producer
  (no `/feed`); its MCP `reflection` reports an empty event graph (D13). The fixed
  verbs, config-from-env, the loopback server + PRM + identity gate, the
  `appkit/mcp` transport, and the `appkit/web` render/static mechanism are
  appkit's. main.go declares sites's identity (the Spec) and wires its surface
  through the `Spec.Handlers` hook: the landing route (`GET /{$}`), the site-serving
  routes (`GET /public/`, `GET /private/`), and the `POST /mcp` mount.
- **nginx is the sole trust boundary.** sites runs no token/session logic and
  binds `127.0.0.1` only. Every `/srv/sites/` request is gated (or not) at nginx,
  which forwards to the loopback service. **nginx serves no site bytes off disk** —
  it `proxy_pass`es both the public and private site paths to the sites process
  (there is no `alias`); this is the core change from the earlier disk-served
  design. The site-serving Go routes are therefore mounted **ungated in-process**,
  exactly as `POST /mcp` relies on nginx for its bearer gate.
- **Two front doors, two audiences.** Humans in a browser are gated by the
  dashboard login-session cookie (`auth_request /_session-authn`); agents/MCP
  clients by an opaque bearer (`auth_request /_authn`). The landing page and the
  **private** site tier are cookie-gated; the **public** site tier is
  unauthenticated; the `/mcp` endpoint is bearer-gated.

## Data model

`sites` is one row per hosted site, keyed by slug `name`. After the hosting-model
change the row is: `name` (slug PK), `public` (INTEGER 0/1 — the visibility
boolean), `created_by` (TEXT — the owner email that created it), `source_path`
(TEXT, nullable — dropbox-sync provenance, unchanged), `created_at`, `updated_at`.
The retired columns `tier`, `published`, and `published_at` are **gone** (a site's
visibility is the `public` boolean; there is no lifecycle flag). The database is
the single source of truth for which sites exist and their visibility; the
on-disk folder location mirrors it in lockstep (the MCP tools are the only
writer). See D15.

## Filesystem layout

A site's files live **directly** at its served location — there is no working
tree and no symlink indirection:

- `SITES_ROOT/public/<slug>/**` — a public site's files.
- `SITES_ROOT/private/<slug>/**` — a private site's files.

`SITES_ROOT` defaults to `/opt/sites/state/www`. `Layout.SiteDir(public, slug)`
is the single path helper. Flipping visibility renames the directory between the
two parents in lockstep with the `public` flag. See D16.

## In-process static serving

`internal/serve` is a sites-owned `http.Handler` that serves the two site trees
from `SITES_ROOT` over the loopback server, mounted at `GET /public/` and
`GET /private/`. It serves real files (no symlink layer), maps a directory to its
`index.html`, returns `404` (never a listing, never `403`) for a directory with
no index or a missing path, confines every path under the site dir via
`internal/files.ConfinePath` (an escape is `404`), and 301-redirects a directory
request that lacks a trailing slash. It is distinct from the chassis `/static/`
mount (which serves the service's *own* Carbon UI assets from `share/www`). See
D17.

## Testing strategy

Testing is part of the architecture. The cross-cutting approach:

- **The static server is tested over a temp `SITES_ROOT` with
  `net/http/httptest`.** Tests build a real directory tree under a `t.TempDir()`
  root, construct the `internal/serve` handler over it, and drive it with
  `httptest` requests, asserting status, body, `Content-Type`, the index.html
  mapping, the missing-index `404`, the traversal `404`, and the trailing-slash
  redirect. No network, no running suite.
- **The domain store is tested over a real migrated SQLite DB.** `internal/sites`
  tests open an in-memory/temp DB via `appkit/db`, run the migration set, and
  assert `Create` persists `created_by` and defaults `public` false, `SetVisibility`
  flips the flag and moves the directory, and the final schema has `public` +
  `created_by` and lacks `tier`/`published`/`published_at` (via `pragma
  table_info`). The migration assertions run against the **real** SQLite the
  runner uses, not a fake — the substrate that actually enforces the column set.
- **The MCP tool table is tested at the handler boundary.** Tests assert the tool
  set contains no `publish`/`unpublish`, that `create` records the request
  Identity's owner as `created_by`, that `set_visibility` moves the folder and the
  returned `url` reflects the new tier, and that the file tools/`sync`/`delete`
  operate on `SiteDir(site.Public, slug)`.
- **The landing surface is tested over the repo-real `share/www` tree.** Tests
  load the shipped tree with `appkit/web.Load`, render `landing.html` with a fixed
  version and a fixed slice of sites, and assert the version card plus one row per
  site (slug, public/private, creator, created-at), and that an empty slice still
  renders. The same substrate proves the D22 additions structurally: the JSON
  data island's shape and URL-parity (D19), and the control layout — filter bar
  above the table, pager below it, hidden-until-JS with a stylesheet that makes
  `hidden` actually hide, sort hooks and `aria-sort` affordance CSS (D6).
- **The landing page's client JavaScript is tested in two tiers, each covering
  the other's blind spot.** **goja owns the logic (broad, cheap):** a Go test
  reads `share/www/static/landing.js`, evaluates it in `github.com/dop251/goja`
  (which has no `document`, so only the pure definitions run and the DOM
  controller stays inert), and calls the exposed
  `SitesLanding.{filterSites,sortRows,paginate,nextSort,defaultState,reduce,computeView}`
  against fixed inputs — proving fuzzy-filter semantics, sort order and the
  toggle rule, pagination arithmetic, the state reducer, and the view-model
  derivations against the code that actually ships (D22). **A single headless
  browser proves the wiring (narrow, minimal — D23):** one chromedp-driven
  Chrome session loads a seeded, auth-free `httptest` render of the real landing
  page and touches each interactive control exactly once — boot/unhide, type a
  fuzzy query, click a sort header, Clear, page Next/Prev — proving
  `initController` connects the goja-tested logic to a live DOM. Logic
  boundaries are never re-proven in the browser; wiring is never "proven" by a
  structural assert or a DOM mock.
- **The nginx fragment is proven by content assertion.** A test reads
  `sites/etc/nginx.conf` and asserts the public tier `proxy_pass`es to
  `…/public/` with no `auth_request`, the private tier gates with
  `auth_request /_session-authn` and `proxy_pass`es to `…/private/`, neither
  contains `alias` nor references the on-disk state path, and the pre-existing
  landing/PRM/mcp/`@sites_authn_500` locations remain (D4's ids).
- **Determinism.** Handlers take their inputs explicitly (name/version strings,
  the site slice, the `SITES_ROOT`), so output is determined by inputs — no clock,
  no network.

## Layout

The design is split for addressability so a build phase reads only the one
Decision it realizes:

- `project/design/design.md` — this spine: static cross-cutting facts only.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded;
  referenced in prose and the plan as `D<N>`).
- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map; the grep target for resolving an id.

**Service packages.** `internal/sites` (slug/visibility store + `Layout.SiteDir`),
`internal/serve` (the in-process static server, D17), `internal/files` (confined
filesystem ops, D10), `internal/mcp` (the domain tool table over the `appkit/mcp`
transport, D13/D20), `internal/db` (the embedded migration set + load guard). The
landing page and Carbon assets live on disk in `sites/share/www/` served by the
chassis, including the landing page's client script `share/www/static/landing.js`
(D22, filter/sort/paginate). There is **no** working tree, no served-symlink
tree, and no `internal/web` package.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a new
Decision adds a `DNN.md` and an INDEX entry. Existing `R-XXXX-XXXX` ids are stable
handles — never renumbered; a newly added behavior gets a freshly minted id, and a
removed behavior's id is deleted with it.
