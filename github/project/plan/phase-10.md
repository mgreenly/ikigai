# Phase 10 — Clone the canonical crm landing verbatim and mirror crm's test set

*Realizes design Decision 6 (landing page), reworked: the new crm-clone
verification ids `R-XSOU-THYE`, `R-XTWR-79P3`, `R-XV4N-L1FS`, `R-XWCJ-YT6H`,
`R-XXKG-CKX6`, `R-XYSC-QCNV`, and the corrected implementation of the kept ids
`R-EVZ3-VXJZ`, `R-7NJI-UTHM`, `R-7PZB-MCZ0`, `R-EX70-9PAO`. Depends on Phase 9.*

## What gets built

Phases 6–9 iterated the landing page toward "canonical" but each pass pinned a
**github-only** variant with github-bespoke guards (a golden fixture, a
structural-contract check, a token-integrity check, an escaping test). The golden
was regenerated from the drifted template every time, so all guards stayed green
while the shipped page still rendered unlike every sibling: a different `<main>`
width and centring, a flex/`--layout-*` `<style>` block the other services do not
use. This phase abandons the bespoke approach: `github`'s landing becomes a
**byte-exact clone of the canonical `crm` landing**, and `github`'s web tests
become a **verbatim clone of `crm`'s web test set**, so github is verified exactly
the way the rest of the suite is.

**`internal/web/landing.html`** — replace the entire file with a byte-for-byte copy
of `crm/internal/web/landing.html` (the canonical reference of record; `gmail`,
`ledger`, and the rest share it identically). Change **only** the three text
fields that carry github's own copy, leaving the `<style>` block and all markup
structure identical to crm's:

- `<title>{{.Service}} · github</title>` (crm's `· crm` suffix → `· github`);
- the eyebrow `<div class="eyebrow">GitHub connector</div>` (crm's `Contacts CRM`);
- the description `<p>Github connects the suite to GitHub through one shared GitHub
  App and exposes repository, pull request, and issue actions as MCP tools.</p>`
  (crm's contacts sentence).

The heading stays crm's canonical `<h1 id="page-title">{{.Service}}</h1>` and the
Version cell keeps `class="version"`. No `--layout-gutter`, `--layout-max-width`,
`position: fixed`, `var(--color-accent)`, `.detail`, `.description`, or
`<p class="eyebrow">` survives — the `<style>` and markup are crm's verbatim.
`static/tokens.css` and the fonts are already byte-identical to crm's and are **not
touched**.

**`internal/web/web_test.go`** — replace the entire file with a verbatim clone of
`crm/internal/web/web_test.go`, applying exactly these substitutions:

- Service-name literals: crm's `LandingHandler("crm-test", "v9.8.7")` →
  `LandingHandler("github-test", "v9.8.7")` (the count-of-3 test); crm's other
  `LandingHandler("crm", "dev")` calls → `LandingHandler("github", "dev")`.
- Content literals: `Contacts CRM` → `GitHub connector`; `<h1 id="page-title">crm</h1>`
  → `<h1 id="page-title">github</h1>`; crm's contacts description →
  github's description above.
- **Re-tag every test's `// R-…` comment with github's minted id**, not crm's:
  Renders→`R-EVZ3-VXJZ`, LinksOnlyAppLocalStaticAssets→`R-XSOU-THYE`,
  PreloadsSelfServedFontFiles→`R-XTWR-79P3`, UsesCanonicalServiceLayout→`R-7NJI-UTHM`,
  RendersHomeLink→`R-7PZB-MCZ0`, StaticHandlerServesTokensAndFonts→`R-EX70-9PAO`,
  TokensCSSDeclaresEmbeddedFontFaces→`R-XV4N-L1FS`,
  TokensCSSUsesOptionalFontDisplay→`R-XWCJ-YT6H`,
  ExactRootRoute…→`R-XXKG-CKX6`, CompositionRootMounts…→`R-XYSC-QCNV`.
- Adapt the two composition-coupled tests to github's reality (github is a
  **non-producer** with no `/feed`, and its Spec lives in
  `internal/githubapp/spec.go`, not inline in `cmd/github/main.go`):
  - `TestExactRootRouteDoesNotShadowMCPOrUnknownPaths`: drop the `/feed` stub route
    and its assertion; keep `/health`, the PRM well-known, `POST /mcp`, and the
    unknown-path `404`, each asserted not to return the landing `<h1>`.
  - `TestCompositionRootMountsLandingUngatedAndKeepsMCPWiring`: read
    `../githubapp/spec.go` (not `../../cmd/crm/main.go`) and assert github's actual
    wiring — `rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))`,
    `rt.Handle("POST /mcp", rt.RequireIdentity(`, and
    `mcp.NewHandler(client, rt.Version(), rt.Service(), health, rt.Logger())` — with
    the `rt.Events()` / `rt.Subscriptions()` assertions dropped; keep the negative
    check that the landing line is not `RequireIdentity`-wrapped.

Carry crm's `htmlHead` and `lineContaining` helpers along. This clone **removes**
github's four bespoke tests (`TestLandingHandlerMatchesGoldenCanonicalLayout`,
`TestLandingHandlerSatisfiesCanonicalStructuralContract`,
`TestLandingHandlerEscapesInjectedServiceAndVersion`,
`TestLandingHandlerReferencesOnlyDefinedTokens`) and their ids
`R-X00O-1HBS`, `R-31CG-6FPW`, `R-7ORF-8L8B`, `R-WYSR-NPL3` (design already dropped
them).

**`internal/web/testdata/landing.golden.html`** — **delete** it (and the now-empty
`internal/web/testdata/` directory); no sibling service ships a golden fixture and
no remaining test reads one.

Left untouched because they are already canonical: `web.go`, `embed.go`, the
embedded font/CSS assets, and `etc/nginx.conf` / `internal/web/nginx_test.go`
(`R-EYEW-NH1D`).

Observable end state: a logged-in browser at `/srv/github/` sees a page **visually
indistinguishable** from `/srv/crm/` — same `<main>` width and centring, same muted
eyebrow, same top-left `Home` link, same grid, spacing, and fonts — because the
shipped `landing.html` is crm's file with three words changed, and github's web
tests are crm's tests.

## Done when

All hold on identical repo state, from `github/`:

- `GOWORK=off go build ./...` and `GOWORK=off go test ./...` exit 0; `gofmt -l .`
  empty; `go vet ./...` clean.
- The `<style>` block of the shipped landing is the canonical one (no legacy
  layout tokens or drift markers) — all six return `0`:
  `grep -c -- '--layout-gutter' github/internal/web/landing.html`,
  `grep -c -- '--layout-max-width' github/internal/web/landing.html`,
  `grep -c 'position: fixed' github/internal/web/landing.html`,
  `grep -c 'var(--color-accent)' github/internal/web/landing.html`,
  `grep -c 'class="detail"' github/internal/web/landing.html`,
  `grep -c 'class="description"' github/internal/web/landing.html`.
- The canonical markers are present — all four return `1`:
  `grep -c '<section aria-labelledby="page-title">' github/internal/web/landing.html`,
  `grep -c '<div class="eyebrow">GitHub connector</div>' github/internal/web/landing.html`,
  `grep -c '<h1 id="page-title">{{.Service}}</h1>' github/internal/web/landing.html`,
  `grep -c '<dl aria-label="Service details">' github/internal/web/landing.html`.
- The shipped `<style>` block is byte-identical to crm's canonical one — this is
  empty:
  `diff <(sed -n '/<style>/,/<\/style>/p' crm/internal/web/landing.html) <(sed -n '/<style>/,/<\/style>/p' github/internal/web/landing.html)`.
- The bespoke fidelity harness is gone: `github/internal/web/testdata/landing.golden.html`
  does not exist, and all three return `0`:
  `grep -rc 'landing.golden.html' github/internal/web/`,
  `grep -c 'TestLandingHandlerMatchesGoldenCanonicalLayout\|TestLandingHandlerSatisfiesCanonicalStructuralContract\|TestLandingHandlerEscapesInjectedServiceAndVersion\|TestLandingHandlerReferencesOnlyDefinedTokens' github/internal/web/web_test.go`,
  `grep -c 'R-X00O-1HBS\|R-31CG-6FPW\|R-7ORF-8L8B\|R-WYSR-NPL3' github/internal/web/web_test.go`.
- The cloned crm test set is present with github's ids — this returns `6`:
  `grep -c 'func TestLandingHandlerLinksOnlyAppLocalStaticAssets\|func TestLandingHandlerPreloadsSelfServedFontFiles\|func TestTokensCSSDeclaresEmbeddedFontFaces\|func TestTokensCSSUsesOptionalFontDisplayForEveryFontFace\|func TestExactRootRouteDoesNotShadowMCPOrUnknownPaths\|func TestCompositionRootMountsLandingUngatedAndKeepsMCPWiring' github/internal/web/web_test.go`,
  and each of `R-XSOU-THYE`, `R-XTWR-79P3`, `R-XV4N-L1FS`, `R-XWCJ-YT6H`,
  `R-XXKG-CKX6`, `R-XYSC-QCNV` appears in `github/internal/web/web_test.go`.
