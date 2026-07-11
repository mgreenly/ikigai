# Phase 89 — nginx fragment: the session-gated web locations opt into the apex `@login_bounce`

*Realizes design Decision 60 (opt into `@login_bounce`). **Structural / config
phase — no R-ids.** Edits only `wiki/etc/nginx.conf`; touches no Go code, adds
no migration. Depends on no earlier phase (a purely additive change to the
fragment).*

Adds one line — `error_page 401 = @login_bounce;` — to each of the three
session-gated locations in `wiki/etc/nginx.conf`: the exact-match web root
`= /srv/wiki/`, the subject-page tier `/srv/wiki/subject/`, and the asset tier
`/srv/wiki/static/`. Each retains its `auth_request /_session-authn;` and
`proxy_pass`; nothing else in the fragment changes. The bearer prefix `location
/srv/wiki/` (fronting `/srv/wiki/mcp`, `/health`, `/feed`), the PRM bootstrap,
and the 429 re-emit deliberately do **not** get the line. `@login_bounce` is a
dashboard-owned apex external contract (defined by the dashboard's own
`project/`, D20) that wiki only references — like `/_session-authn`.

**Done when:** the Go suite stays green (this phase changes no Go — `cd wiki &&
go build ./... && go vet ./... && gofmt -l . && go test ./...` and
`bin/check-migrations wiki` all unaffected) **and** the named structural check
passes:

- Each of the three session-gated locations — `location = /srv/wiki/ {`,
  `location /srv/wiki/subject/ {`, and `location /srv/wiki/static/ {` — contains
  **both** `auth_request /_session-authn;` and `error_page 401 = @login_bounce;`.
- The bearer prefix `location /srv/wiki/ {` (with `auth_request /_authn`) does
  **not** contain `error_page 401 = @login_bounce;`, and it, the PRM exact-match
  location, and `location @wiki_authn_500` are still present and unchanged.
- The change is additive: every pre-existing location still appears and each
  session-gated location keeps its `auth_request /_session-authn;` and its
  `proxy_pass` — nothing removed or rewritten.

(Structural phase: "Ids to cover" is **(none — structural phase)**; the build
loop verifies the green suite plus the fragment check above, not an `R-id` test.)
