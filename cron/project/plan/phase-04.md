# Phase 4 — Add the top-left Home link to the dashboard landing page

*Realizes design Decision 6 (the top-left Home link). Depends on the
existing landing page (the `internal/web` handler + template already serve it).
Covers `R-HOME-2K4P`.*

Cron's landing page gains a quiet **top-left Home link** that returns the
viewer to the dashboard landing page — the apex root `/`, which nginx routes to
the dashboard. This is cron's own control, built entirely within cron; nothing
is shared with another service. **Read D6 for the exact markup and
rationale.**

**What gets changed (markup only — no Go logic, no nginx, no handler signature):**

- **`cron/internal/web/landing.html`** — add, as the **first child of `<main>`** (inside
  the centered column), the anchor:

  ```html
  <a class="home" href="/">Home</a>
  ```

  and add the `.home` rule to the inline `<style>` (with the other label-style
  selectors), exactly as D6 specifies. The link text is `Home` (not
  `Dashboard`) so the page's external-asset guard stays green. Everything else on
  the page — the eyebrow, `h1`, description, `dl`, and asset links — is unchanged.
- **`cron/internal/web/web_test.go`** — add a genuinely-asserting test tagged
  `// R-HOME-2K4P` that drives `GET /` through the landing handler and asserts the
  rendered body contains the home link `href="/"`. Keep the existing landing
  assertions green (the new anchor must not trip the embedded-assets guard — `Home`
  text, `/` href, no `dashboard`/`http` substring).
- Touch nothing else. No schema change — no migration.

**Done when:**

- R-HOME-2K4P — a test tagged `// R-HOME-2K4P` drives `GET /` through the landing handler and
  asserts the rendered body contains an anchor to the dashboard apex (`href="/"`).
- The rendered page shows a top-left `<a class="home" href="/">Home</a>` with its
  `.home` style rule in the inline `<style>`.
- The suite is green: `cd cron && go build ./...`, `cd cron && go vet ./...`,
  `cd cron && gofmt -l .` (prints nothing), `cd cron && go test ./...`, and
  `bin/check-migrations cron`.
