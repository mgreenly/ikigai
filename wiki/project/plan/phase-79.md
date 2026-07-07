# Phase 79 — Serve the read surface from `share/www` through the chassis

*Realizes design Decision 52. Depends on Phase 78 (one inline composition root to
edit) and on the appkit chassis providing D5–D7 (`Spec.WWW`, `appkit/web`,
`Router.WWW()`), consumed through the committed replace as a fixed external
contract. Retained D42–D50 read-surface ids are re-proven over the on-disk tree.*

Observable end state:

- The web assets ship on disk (same bytes): `internal/web/layout.tmpl`,
  `home.tmpl`, `subject.tmpl` → `wiki/share/www/`; `internal/web/static/tokens.css`
  and `internal/web/static/fonts/*.woff2` (the four fonts) →
  `wiki/share/www/static/`.
- The templates load into **one** `appkit/web.Load` set with **no** duplicate
  `{{define …}}` name: the shared `{{block "main"}}` in `layout.tmpl` is replaced
  by unique-named chrome partials (`head`/`foot`), and `home.tmpl`/`subject.tmpl`
  invoke them, so rendering `"home"` and `"subject"` from the one set each yields
  its **own** body. The rendered DOM (elements, classes, aria labels, `.prose`
  container, `<base href>`, the `tokens.css` link, the D50 font preloads) is
  preserved.
- The composition root sets `WWW: true`; `internal/web.NewHandler` takes the
  `*appkit/web.Site` from `rt.WWW()` and renders via `site.Render(w, "home"|
  "subject", data)`. The `//go:embed`, the `template.Must(ParseFS(...))` sets,
  the `StaticHandler`/`LandingHandler` funcs, and the `GET /static/` mux mount
  are deleted (the chassis auto-mounts `/static/`). The styled-404 path keeps its
  `w.WriteHeader(http.StatusNotFound)` before rendering.
- `internal/web` retains its router role (route table, injected seams, `?q=` ask
  dispatch, markdown rendering).

**Boundary-crossing line (flag it):** `bin/start`'s `launch_wiki` gains
`export WIKI_WWW_PATH="$repo/wiki/share/www"` beside its existing
`WIKI_DB_PATH`. This one line lives outside `wiki/`; it is **not** covered by the
Go suite and is verified by the live `bin/start` smoke (bring the suite up, load
`GET /srv/wiki/` and `GET /srv/wiki/static/tokens.css` through the front door).

**Done when:** the suite is green (`cd wiki && go build ./...`, `go vet ./...`,
`gofmt -l .` empty, `go test ./...`) and these ids are covered by clearly-named
tests driving a `Site` loaded from the repo-real `wiki/share/www` tree:

- **R-JGZ2-0BMY** — a `Site` loaded from `wiki/share/www` renders `"home"`
  through the web handler with the injected service and version in the footer and
  the search `<form>` present.
- **R-JI6Y-E3DN** — the same `Site` renders `"subject"` producing the
  subject-page `<article>` (title + `.prose` body + mention footers), distinct
  from the home body — the collision-free single-set proof.
- **R-JJEU-RV4C** — the chassis static mount over `wiki/share/www` serves
  `GET /static/tokens.css` (`200`, `text/css`) and
  `GET /static/fonts/space-grotesk.woff2` (`200`, `font/woff2`) with no wiki-side
  `/static/` handler registered.

and these scoped greps hold:

- `grep -rn "go:embed" wiki/internal/web/` returns nothing;
- `grep -rn "StaticHandler\|ParseFS\|homeTemplates\|subjectTemplates" wiki/internal/web/*.go`
  returns nothing;
- the retained D42–D50 read-surface tests pass over the on-disk tree with no
  assertion changes.
