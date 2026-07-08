# Phase 18 — Landing page lists the sites that exist

*Realizes design Decision 19 (landing lists sites). Depends on Phase 15 (`Site.Public`/`CreatedBy`).*

Grow the `GET /{$}` landing from a version-only card into the owner-facing index.

- The `GET /{$}` handler in `cmd/sites/main.go` queries `store.List(ctx)` and
  builds a `landingView{Service, Version, Sites []siteRow}` where
  `siteRow{Slug, Public, CreatedBy, CreatedAt}` (CreatedAt pre-formatted UTC), and
  renders it through `rt.WWW().Render(w, "landing.html", view)`.
- `share/www/landing.html` gains a sites section rendering one row per `siteRow`
  (slug, public/private label, creator, created-at), and renders cleanly when
  `Sites` is empty (version card, no rows / explicit empty state). Carbon styling,
  the session gate, and the version card are unchanged.

**Done when:** the sites suite is green with render tests over the repo-real
`share/www` tree (`appkit/web.Load`) covering R-RAW6-IUN5 (two sites of differing
visibility each render slug + public/private + creator + created-at) and
R-RC42-WMDU (an empty `Sites` slice renders without error and still shows the
version).
