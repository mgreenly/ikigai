# Phase 55 — The keyword lane: bring back `pages_fts` and wrap it as a `Retriever`

*Realizes design Decision 31 (the keyword lane: full-text search returns). Depends on Phase 52 (the D08 seam) and the D3 page-write path / D17 read handle in `internal/wiki`. This phase also retires the two D08 checks that recorded the now-departed "no full-text table" world.*

The "match the words" lane returns: a SQLite full-text index over page titles and bodies, kept correctly in sync on every write, wrapped behind the search seam.

In `internal/wiki`:

- A new **forward create migration** (generated with `bin/new-migration wiki create_pages_fts`; **never** hand-name the version) recreating `pages_fts` as an **external-content** FTS5 table over `pages(title, body)` (`content='pages'`, `content_rowid='rowid'`), and backfilling existing rows in one shot with `INSERT INTO pages_fts(pages_fts) VALUES('rebuild');` — pure SQL, no network. The old `drop_pages_fts` migration stays **frozen** on disk (immutable-migrations rule); this is a brand-new create.
- The external-content **sync in the page-write path**, inside the same transaction as the page write: a **new** page inserts its title/body into `pages_fts`; a **rewritten** page issues the FTS5 `'delete'` command with the page's **previous** title/body (read before the row is overwritten) and then inserts the new — the read-old → delete → insert dance that keeps an external-content index from rotting. The prior build's reference implementation (`ftsPhrase` + this sync) is reused as-is.
- `ftsPhrase` — the query sanitizer: wrap each term as a quoted literal (doubling internal quotes) and OR-join them, so quotes/parentheses/operators in a user's question run as literal terms (no FTS5 syntax error, no operator injection) and a page matching **any** term is returned.

In `internal/retrieve`:

- `keywordRetriever` (over the D17 read handle) satisfying `retrieve.Retriever`: an FTS5 `MATCH` ranked by the built-in `bm25()` (best first), capped at `k`, each match returned as a `Hit` whose `PageID` is the page's subject id, plus title and a matched snippet.

**Retire the old-world D08 checks.** Delete the two tests that asserted the full-text table was gone and that ingest never touched it — they describe the world this phase leaves: `R-PH8Z-YHNX` (`internal/db/db_test.go`) and `R-PIGW-C9EM` (`internal/wiki/service_test.go`). Their ids are not re-minted; the history of the removal-then-return lives here in the plan.

**Done when:** the suite is green (per design *Conventions*, including `bin/check-migrations wiki`), the two retired tests above are gone, and these ids are covered by clearly-named tests against a **real** temp SQLite migrated by the appkit runner (`ftsPhrase` may be table-tested pure):

- **R-203P-F1ET** — after migrations, `pages_fts` exists and a `MATCH` for a word in a page's body returns that page (built, queryable, rebuild backfilled existing rows).
- **R-22JI-6KW7** — the rewrite regression guard: after a page is rewritten so one word is removed and another added, a `MATCH` for the removed word no longer returns the page and a `MATCH` for the added word does (proves the read-old→delete→insert sync, not a stale index).
- **R-23RE-KCMW** — `ftsPhrase` neutralizes FTS5 syntax: a query with quotes/parentheses/operators runs as literal terms without error, and multiple terms OR-join so a page matching **any** term is returned.
- **R-24ZA-Y4DL** — `keywordRetriever.Search` returns `Hit`s whose `PageID` is the matched subject id, ordered by bm25 relevance, capped at the requested `k`.
