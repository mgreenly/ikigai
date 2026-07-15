# repos

The repos service provisions repositories and runs issue or manually started
agent sessions. It is a standalone Go module on the shared appkit chassis.

Runtime configuration is read from `REPOS_`-prefixed environment variables at
the composition root. SQLite access uses appkit's single-writer handle and the
embedded migrations in `internal/db/migrations`.
