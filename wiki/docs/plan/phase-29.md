# Phase 29 — DB concurrency: single-writer handle + concurrent read pool

*Realizes design Decision 17 (DB concurrency: split write/read handles + routing
seam). Depends on Phase 01 (the D2 composition root and `appkit` `rt.DB()`
single-writer handle), Phase 02 (the D3 domain stores), Phase 24 (the D13
recorder write path), Phase 25 (the D14 integrate transaction), and Phase 26
(the D15 `LLMCallStore.List`).*

Reads stop serializing behind the single connection that ingest and LLM work
hold: the service runs on **two handles to the same file** — appkit's `rt.DB()`
stays the sole writer (`MaxOpenConns(1)`, so writes serialize by construction),
and wiki opens one additional read-only handle (a small `MaxOpenConns(N>1)`
pool) whose readers run concurrently and, under WAL, never block on the in-flight
writer. The "exactly one writer" invariant D4/D13/D14 lean on is preserved — a
second *read* handle adds no writer.

Built as the connection shape plus the per-statement routing seam:

- `db.OpenRead(path)` opens the **already-migrated** file read-only with appkit's
  existing pragmas (`journal_mode(WAL)`, `busy_timeout(5000)`, `foreign_keys`)
  plus `query_only(true)` as a structural guard, runs no migrations, and sets a
  small bounded `readPoolSize > 1`.
- `wiki.Conns{Read, Write *sql.DB}` carries the two handles; `wiki.NewService`
  takes `Conns` in place of the single `*sql.DB`.
- Every store routes per statement: **SELECT-only methods use `Read`; every
  mutation and every transaction uses `Write`.** A mixed store (e.g. `JobStore`:
  `Status`/`List` read; `InsertIngest`/`ClaimPending`/`Finish*`/`Abort`/`Rerun`/
  boot-sweep write) holds both and picks per method. Pure-read stores
  (`SubjectStore.List`, `ClaimStore.List`, `PageStore.Get`/`PageWithLinks`,
  `LLMCallStore.List`) take `Read`. No transaction is ever begun on `Read`; the
  recorder (D13) and the integrate write-set (D14) run on `Write`.
- The composition root (D2 `Handlers`) opens the read handle over the
  already-migrated file and builds the service as
  `wiki.NewService(wiki.Conns{Read: read, Write: write}, …)`.

**Done when:** R-FUCC-IT4M, R-FVK8-WKVB, R-FWS5-ACM0, and R-FY01-O4CP are each
covered by a clearly-named test running concurrent goroutines against one real
temp WAL SQLite (a reader returning the last committed snapshot promptly while an
uncommitted write transaction is open; two concurrent writes both succeeding and
serializing with no `SQLITE_BUSY`; a read and a control write both completing
while the write connection is idle; read-your-writes visibility across the two
handles after commit), and the suite is green.
