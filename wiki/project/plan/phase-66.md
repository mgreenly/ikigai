# Phase 66 — Conform the landing page to the cron canonical template

*Realizes design Decision 40 (conform to cron). Structural / markup-only — covers
no `R-XXXX-XXXX` ids. Depends on Phases 63–65 (D39 — the landing page, its route,
and its nginx fragment already exist; this phase only re-skins the page).*

wiki's landing page (D39) has its own markup and tokens; this phase makes them
identical to the suite-canonical cron page, preserving only wiki's data (the
service name and version stay dynamic via `{{.Service}}` / `{{.Version}}`).
**Read D40 for the canonical source files and the exact substitution table.**

**What gets changed (markup + tokens only — no Go logic, no nginx, no handler
signature):**

- **`wiki/internal/web/landing.tmpl`** — make it identical to
  `cron/internal/web/landing.html` except the three substitutions in D40's table:
  the `<title>` suffix (`· wiki`), the `.eyebrow` text (`Knowledge base`), and the
  lead `<p>` (`Wiki ingests, searches, and answers over a durable knowledge base
  using retrieval-augmented generation.`). Everything else — the inline `<style>`,
  the `<h1>{{.Service}}</h1>`, the `Service / Version / API` `<dl>` (API value
  `POST /mcp`), and the `/static/tokens.css` link — is copied from cron verbatim.
  Keep the `{{.Service}}` / `{{.Version}}` field references (they match
  `landingData{Service, Version}`).
- **`wiki/internal/web/static/tokens.css`** — replace with a byte-for-byte copy of
  `cron/internal/web/static/tokens.css`. Leave `static/fonts/` as is.
- **`wiki/internal/web/web_test.go`** — the D39 landing tests
  (`R-LAND-PG01/NMVR/CARB/ROOT/UNGT`) must stay green. Keep their behavioral
  assertions (renders the injected service name + version; `200`;
  `Content-Type: text/html; charset=utf-8`; exact-root `{$}` only; ungated
  in-process). If any of them pins the prior markup's literal text (old eyebrow /
  title / card structure / asset path), update that literal to cron's canonical
  text — do not weaken the behavioral assertion.
- Touch nothing else: the handler, the routes, `cmd/wiki/main.go`, and the nginx
  fragment are unchanged. No schema change — no migration.

**Done when (structural content check — no id-tagged test):**

- `diff wiki/internal/web/static/tokens.css cron/internal/web/static/tokens.css`
  (run from the repo root) prints nothing.
- `wiki/internal/web/landing.tmpl` differs from `cron/internal/web/landing.html`
  on only the three lines in D40's table (the `· wiki` title suffix, the
  `Knowledge base` eyebrow, and the wiki lead paragraph); all other lines are
  identical.
- The suite is green: `cd wiki && go build ./...`, `cd wiki && go vet ./...`,
  `cd wiki && gofmt -l .` (prints nothing), `cd wiki && go test ./...`, and
  `bin/check-migrations wiki`.
