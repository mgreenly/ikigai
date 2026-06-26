# Phase 5 — Add the top-left Home link to the dashboard landing page

*Realizes design Decision 7 (the top-left Home link). Depends on the
existing landing page (the `internal/web` handler + template already serve it).
Covers `R-HOME-8R2V`.*

Scripts's landing page gains a quiet **top-left Home link** that returns the
viewer to the dashboard landing page — the apex root `/`, which nginx routes to
the dashboard. This is scripts's own control, built entirely within scripts; nothing
is shared with another service. **Read D7 for the exact markup and
rationale.**

**What gets changed (markup only — no Go logic, no nginx, no handler signature):**

- **`scripts/internal/web/landing.html`** — add, as the **first child of `<body>`** (before
  `<main>`), the anchor:

  ```html
  <a class="home" href="/">Home</a>
  ```

  and add the `.home` rule to the inline `<style>` (with the other label-style
  selectors), exactly as D7 specifies. The link text is `Home` (not
  `Dashboard`) so the page's external-asset guard stays green. Everything else on
  the page — the eyebrow, `h1`, description, `dl`, and asset links — is unchanged.
- **`scripts/internal/web/web_test.go`** — add a genuinely-asserting test tagged
  `// R-HOME-8R2V` that drives `GET /` through the landing handler and asserts the
  rendered body contains the home link `href="/"`. Keep the existing landing
  assertions green (the new anchor must not trip the embedded-assets guard — `Home`
  text, `/` href, no `dashboard`/`http` substring).
- Touch nothing else. No schema change — no migration.

**Done when:**

- R-HOME-8R2V — a test tagged `// R-HOME-8R2V` drives `GET /` through the landing handler and
  asserts the rendered body contains an anchor to the dashboard apex (`href="/"`).
- The rendered page shows a top-left `<a class="home" href="/">Home</a>` with its
  `.home` style rule in the inline `<style>`.
- The suite is green: `cd scripts && go build ./...`, `cd scripts && go vet ./...`,
  `cd scripts && gofmt -l .` (prints nothing), `cd scripts && go test ./...`, and
  `bin/check-migrations scripts`.
