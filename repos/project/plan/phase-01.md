# Phase 1 — Module scaffold & data model (migrations + Store)

*Realizes design Decision 2 (data model & migrations). Depends on nothing.*

The `repos` Go module exists and its database layer works. Scaffold the
standalone module at `repos/` (`go.mod` with committed `replace appkit =>
../appkit`, `replace eventplane => ../eventplane`, `require registry`;
`VERSION` at `v0.1.0`; `.envrc`; `AGENTS.md` + `CLAUDE.md` symlink).
`internal/db` holds only the migration embed (`db.FS`) and the DDL drift
guard; migrations (created via `bin/create-migration repos <name>`) build the
`repos` and `sessions` tables and the eventplane outbox from the current
`outbox.SchemaSQL`. `internal/repos/store.go` implements the Store over the
shared single-writer handle with the query set D2 names (repo CRUD, session
insert/read/list, `ActiveSessionForIssue`, `CountRunning`, `NextQueued`,
`MarkRunning`, `FinishSession`, `MaxAttempt`, `SweepRunning`), tested against
real temp-file SQLite through the full embedded migration set.

Note: the suite-level preconditions (registry row `{"repos", 3007, Core}`,
`./repos` in `go.work`, `repos` in `bin/start`) are operator-applied outside
this module; this phase must not edit files outside `repos/`.

**Done when:** R-EMGN-7X72 (migration set yields the exact tables + outbox
drift guard), R-ENOJ-LOXR (Store round-trips and queue/attempt queries), and
R-EOWF-ZGOG (terminal-write transaction atomicity) are each covered by a
clearly-named test, and the suite is green per design Conventions.
