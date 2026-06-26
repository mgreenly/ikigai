# Phase 14 — Conform the landing page to the cron canonical template

*Realizes design Decision 11 (conform to cron). Structural / markup-only — covers
no `R-XXXX-XXXX` ids. Depends on Phases 11–13 (D10 — the landing page, its route,
and its nginx fragment already exist; this phase only re-skins the page).*

prompts's landing page (D10) has its own markup and tokens; this phase makes them
identical to the suite-canonical cron page, preserving only prompts's data (the
service name and version stay dynamic via `{{.Service}}` / `{{.Version}}`).
**Read D11 for the canonical source files and the exact substitution table.**

**What gets changed (markup + tokens only — no Go logic, no nginx, no handler
signature):**

- **`prompts/internal/web/landing.tmpl`** — make it identical to
  `cron/internal/web/landing.html` except the three substitutions in D11's table:
  the `<title>` suffix (`· prompts`), the `.eyebrow` text (`Agent sessions`), and
  the lead `<p>` (`Prompts runs sandboxed Claude agent sessions on the owner's
  behalf and exposes them as MCP tools.`). Everything else — the inline `<style>`,
  the `<h1>{{.Service}}</h1>`, the `Service / Version / API` `<dl>` (API value
  `POST /mcp`), and the `/static/tokens.css` link — is copied from cron verbatim.
  Keep the `{{.Service}}` / `{{.Version}}` field references (they match
  `landingData{Service, Version}`).
- **`prompts/internal/web/static/tokens.css`** — replace with a byte-for-byte copy
  of `cron/internal/web/static/tokens.css`. Leave `static/fonts/` as is.
- **`prompts/internal/web/web_test.go`** — the D10 landing tests
  (`R-LAND-PG01/NMVR/CARB/ROOT/UNGT`) must stay green. Keep their behavioral
  assertions (renders the injected service name + version; `200`;
  `Content-Type: text/html; charset=utf-8`; exact-root `{$}` only; ungated
  in-process). If any of them pins the prior markup's literal text (old eyebrow /
  title / card structure / asset path), update that literal to cron's canonical
  text — do not weaken the behavioral assertion.
- Touch nothing else: the handler, the routes, `cmd/prompts/main.go`, and the nginx
  fragment are unchanged. No schema change — no migration.

**Done when (structural content check — no id-tagged test):**

- `diff prompts/internal/web/static/tokens.css cron/internal/web/static/tokens.css`
  (run from the repo root) prints nothing.
- `prompts/internal/web/landing.tmpl` differs from `cron/internal/web/landing.html`
  on only the three lines in D11's table (the `· prompts` title suffix, the
  `Agent sessions` eyebrow, and the prompts lead paragraph); all other lines are
  identical.
- The suite is green: `cd prompts && go build ./...`, `cd prompts && go vet ./...`,
  `cd prompts && gofmt -l .` (prints nothing), `cd prompts && go test ./...`, and
  `bin/check-migrations prompts`.
