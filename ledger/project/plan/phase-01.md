# Phase 1 — Landing handler + embedded Carbon assets, wired at `GET /{$}`

*Realizes design Decision 1 (the landing handler + v1 content), 2 (route wiring
through `Spec.Handlers`), and 3 (embedded Carbon design assets). Depends on no
earlier phase.*

Stand up ledger's human landing page: a new `ledger/internal/web` package with a
Carbon-styled handler showing the service name + running version, its own
embedded `tokens.css` + woff2 fonts, mounted ungated in-process at the exact root
`GET /{$}` through the existing `cmd/ledger/main.go` `Handlers` hook (nginx is the
gate — Phase 2).

**What gets built (the observable end state):**

- **`ledger/internal/web/`** — a new package:
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
- **`ledger/cmd/ledger/main.go`** — one line added to the **existing** `Handlers`
  hook, beside the `POST /mcp` mount, ungated in-process:

  ```go
  rt.HandleFunc("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
  ```

  This is incremental growth of the wiring file, not a rewrite. The `POST /mcp`
  mount and the Service/Producer wiring are untouched. (If `rt.HandleFunc` is not
  the exact appkit Router method name, use the Router's equivalent route-register
  call for `GET /{$}` — the contract is: the bare root, exact-match, served by the
  landing handler.)

**No schema change** — this phase adds **no** migration; `internal/db` is
untouched.

**Done when:**

- R-LAND-3C9D — a test constructs `web.LandingHandler("ledger", v)` and drives a
  `GET /` through it (httptest), asserting status `200`.
- R-LAND-5E1F — that test asserts the rendered body contains the service name
  passed in (`"ledger"`).
- R-LAND-7G2H — a test passes a distinctive version (e.g. `"9.9.9-test"`) and
  asserts it appears verbatim in the rendered body — proving the page reflects the
  injected running version, not a hardcoded string.
- R-LAND-9J4K — a test asserts the landing response's `Content-Type` is
  `text/html; charset=utf-8`.
- R-ROUT-2M6N — a test registers the handler on an `http.ServeMux` at
  `GET /{$}` (as the composition root wires it) and asserts `GET /` is dispatched
  to the landing handler (`200`, name+version body).
- R-ROUT-4P8Q — a test registers a sibling `POST /mcp` on the same mux and asserts
  `{$}` does not shadow it (`POST /mcp` reaches the mcp/stub handler, not the
  landing handler).
- R-ROUT-6R1S — a test asserts a non-root unregistered path (e.g. `GET /nope`) is
  **not** captured by the `{$}` landing pattern (it does not return the landing
  page) — proving exact-match, not subtree.
- R-ASST-3T7V — a test drives `GET /static/tokens.css` through the handler and
  asserts `200` with a CSS content type (`text/css`).
- R-ASST-5W9X — a test asserts the rendered landing HTML references ledger's own
  embedded `/static/` asset path and contains **no** cross-service/dashboard asset
  URL.
- R-ASST-7Y2Z — a test drives `GET` for an embedded font (e.g.
  `/static/fonts/space-grotesk.woff2`) and asserts `200` with a `font/woff2`
  content type.
- The suite is green: `cd ledger && go build ./...`, `cd ledger && go vet ./...`,
  `cd ledger && gofmt -l .` (prints nothing), `cd ledger && go test ./...`, and
  `bin/check-migrations ledger`.
