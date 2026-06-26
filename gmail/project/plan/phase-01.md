# Phase 1 — Landing handler + embedded Carbon assets, wired at `GET /{$}`

*Realizes design Decision 1 (the landing handler + v1 content), 2 (route wiring
through `Spec.Handlers`), and 3 (embedded Carbon design assets). Depends on no
earlier phase.*

Stand up gmail's human landing page: a new `gmail/internal/web` package with a
Carbon-styled handler showing the service name + running version, its own
embedded `tokens.css` + woff2 fonts, mounted ungated in-process at the exact root
`GET /{$}` through the existing `cmd/gmail/main.go` `Handlers` hook (nginx is the
gate — Phase 2).

**What gets built (the observable end state):**

- **`gmail/internal/web/`** — a new package:
  - `web.go` — `LandingHandler(service, version string) http.Handler`. It returns
    an `http.Handler` (typically an `http.ServeMux` or a small struct) that:
    - serves the landing page at its root, rendering an embedded `landing.html`
      template with the two substitutions (service name, version), setting
      `Content-Type: text/html; charset=utf-8` and status `200`;
    - serves the embedded static assets under `/static/…` with correct content
      types (`text/css; charset=utf-8` for `tokens.css`, `font/woff2` for fonts).
  - `landing.html` — embedded via `//go:embed`; a single centered Carbon card:
    service name in Display type (Space Grotesk), version as a Mono · Label (IBM
    Plex Mono). Links the app's **own** `/static/tokens.css`. No data beyond the
    two values; no cross-service asset URL.
  - `static/tokens.css` — a **vendored copy** of `design/tokens.css`, with
    `@font-face` rules pointing at the embedded `/static/fonts/…` paths.
  - `static/fonts/` — vendored woff2 copies: `space-grotesk.woff2`,
    `ibm-plex-sans.woff2`, `ibm-plex-mono-400.woff2`, `ibm-plex-mono-500.woff2`
    (the same set `dashboard/ui/static/fonts/` carries).
  - `embed.go` (or a `//go:embed` block in `web.go`) — `//go:embed landing.html`
    and `//go:embed static` into `embed.FS`.
- **`gmail/cmd/gmail/main.go`** — one line added to the **existing** `Handlers`
  hook, beside the `POST /mcp` mount, ungated in-process:

  ```go
  rt.HandleFunc("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
  ```

  This is incremental growth of the wiring file, not a rewrite. The Gmail client +
  producer Engine construction, the `POST /mcp` mount, and the sibling
  `Producer`/`Workers` hooks (the outbox sink and the poll daemon) are untouched.
  (If `rt.HandleFunc` is not the exact appkit Router method name, use the Router's
  equivalent route-register call for `GET /{$}` — the contract is: the bare root,
  exact-match, served by the landing handler.)

**No schema change** — this phase adds **no** migration; `internal/db` is
untouched.

**Done when:**

- R-LAND-3F7K — a test constructs `web.LandingHandler("gmail", v)` and drives a
  `GET /` through it (httptest), asserting status `200`.
- R-LAND-5H9M — that test asserts the rendered body contains the service name
  passed in (`"gmail"`).
- R-LAND-7J2N — a test passes a distinctive version (e.g. `"9.9.9-test"`) and
  asserts it appears verbatim in the rendered body — proving the page reflects the
  injected running version, not a hardcoded string.
- R-LAND-9K4P — a test asserts the landing response's `Content-Type` is
  `text/html; charset=utf-8`.
- R-ROUT-4M6Q — a test registers the handler on an `http.ServeMux` at
  `GET /{$}` (as the composition root wires it) and asserts `GET /` is dispatched
  to the landing handler (`200`, name+version body).
- R-ROUT-6N8R — a test registers a sibling `POST /mcp` on the same mux and asserts
  `{$}` does not shadow it (`POST /mcp` reaches the mcp/stub handler, not the
  landing handler).
- R-ROUT-8P1S — a test asserts a non-root unregistered path (e.g. `GET /nope`) is
  **not** captured by the `{$}` landing pattern (it does not return the landing
  page) — proving exact-match, not subtree.
- R-ASST-3T5V — a test drives `GET /static/tokens.css` through the handler and
  asserts `200` with a CSS content type (`text/css`).
- R-ASST-5W7X — a test asserts the rendered landing HTML references gmail's own
  embedded `/static/` asset path and contains **no** cross-service/dashboard asset
  URL.
- R-ASST-7Y9Z — a test drives `GET` for an embedded font (e.g.
  `/static/fonts/space-grotesk.woff2`) and asserts `200` with a `font/woff2`
  content type.
- The suite is green: `cd gmail && go build ./...`, `cd gmail && go vet ./...`,
  `cd gmail && gofmt -l .` (prints nothing), `cd gmail && go test ./...`, and
  `bin/check-migrations gmail`.
