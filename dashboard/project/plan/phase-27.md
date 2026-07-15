# Phase 27 — Purge legacy auth state and enforce `owner_id NOT NULL`

*Realizes design Decision 23 (purge + enforce the `owner_id` invariant). Depends
on Phase 22 (which introduced the nullable `owner_id` column this phase makes
`NOT NULL`).*

One new forward-only migration (`bin/create-migration dashboard
purge_auth_and_enforce_owner_id`) under
`dashboard/internal/db/migrations/`, applied by the appkit runner. It is
self-contained and atomic, in two ordered parts:

- **Purge** — delete every row from the auth/identity graph: `oauth_tokens`,
  `oauth_authcodes`, `oauth_chains`, `personal_tokens`, `web_sessions`,
  `oauth_state`, `dcr_clients`, `identities`. `audit_log` and `schema_migrations`
  are left untouched.
- **Enforce** — rebuild `web_sessions`, `oauth_authcodes`, `oauth_chains`,
  `personal_tokens` with `owner_id TEXT NOT NULL` via the SQLite table-rebuild
  (create new-shape table, drop old, rename), trivial because the purge left them
  empty.

No Go source changes: every creation path already stamps a non-empty `owner_id`
(D18) and every read scans it into a plain `string`, which becomes correct by
construction once the column is `NOT NULL` with no legacy rows. The observable
end state: a migrated DB has an empty, `NOT NULL`-constrained auth schema, and
the freshly-minted-then-read PAT and OAuth-chain paths no longer produce the
`converting NULL to string` scan failure.

**Done when:** every Verification id below is covered by a clearly-named test
that genuinely asserts the behavior, and the suite is green per design's
Conventions (`cd dashboard && go build ./...`, `go vet ./...`, `gofmt -l .`
empty, `go test ./...` all pass):

- R-6QJD-1MUY — migration against a real temp SQLite DB seeded with rows in all
  eight auth tables (including legacy `owner_id = NULL` rows) completes without
  error and leaves those eight tables empty, while a seeded `audit_log` row
  survives.
- R-6RR9-FELN — after migration, each of the four carrier tables rejects an
  `INSERT` with `owner_id = NULL` (real SQLite `NOT NULL` constraint error).
- R-6SZ5-T6CC — after migration, `pat.Store.Create` then `ListByOwner` returns
  the token and `ValidatePAT` accepts it, with no `NULL`-scan error.
- R-6U72-6Y31 — after migration, `TokenStore.IssueChainAndTokens` then
  `ValidateAccess` returns the valid token bound to a chain carrying the stamped
  `owner_id`, with no `NULL`-scan error.
