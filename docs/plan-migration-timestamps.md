# Plan — timestamped, immutable migrations

Implementation plan for moving service migrations off hand-picked sequential
integers and onto generated UTC timestamps, with CI guards that catch collisions
and edits before deploy instead of at deploy.

Designed for **sequentially-run subagents**: phases are strictly linear, each is
sized for one subagent, and each ends green and verifiable. Subagents do not
share context, so **every phase begins by reading `docs/adr-migration-timestamps.md`**
(produced in Phase 1) and this plan.

## The problem

Migrations are named `NNN_name.sql` with a sequential integer version
(`appkit/db/db.go`). The integer is hand-picked from the branch's local view of
"the next number." When two agents add a migration to the **same service** on two
branches, both pick the same next integer. Git merges the two differently-named
files (`004_a.sql`, `004_b.sql`) with **no textual conflict**. Nothing notices —
the only duplicate check is in `LoadMigrations`, which runs at `migrate`/`serve`,
i.e. **deploy time**. Most services also lack a test that loads their real
embedded set, so `go test ./...` stays green on the colliding `main`.

Separately, the runner keys on the version integer: editing an already-applied
migration's body is **silently skipped** on existing DBs (version already in
`schema_migrations`) while new DBs get the new body → undetected schema drift.

Rails solved the first with timestamp versions (collision becomes
astronomically unlikely; individual per-version tracking — which we already have —
tolerates out-of-order merges) and the second by convention + tooling
(migrations are immutable; you add a new one to change schema).

## Load-bearing design decisions

1. **Do not renumber existing migrations; do not migrate any live DB.** Integer
   migrations are frozen as legacy. All **new** migrations are timestamps
   (`YYYYMMDDHHMMSS_name.sql`). 14-digit timestamps sort *after* `001…003`, so
   the two schemes coexist in one sorted run: no collision, downgrade guard stays
   happy, and live DBs on `int` (carrying versions 1,2,3) just apply new
   timestamp migrations on top. Avoids a risky one-time `schema_migrations`
   rewrite on production.
2. **Keep `Version int` (do not widen to int64).** The only build target is
   `linux/amd64`, where Go's `int` is 64-bit, so `strconv.Atoi` parses a 14-digit
   timestamp without overflow. Keeps the diff off `opsctl`. Add a defensive
   `Version > 0` check instead.
3. **`AGENTS.md` is canonical; root `CLAUDE.md` is a symlink to it** — one edit
   updates both.

## Phases

### Phase 1 — Design ADR (the contract for all later phases)
- **Scope:** write `docs/adr-migration-timestamps.md`. Pure docs, no code.
- **Content:** problem (integers collide across branches, caught only at deploy;
  edits silently skipped); decision (timestamp `YYYYMMDDHHMMSS_name.sql`, UTC,
  generated never hand-picked); transition (legacy integers frozen, never
  renumbered, coexist via sort order); immutability (a committed migration is
  never edited/deleted — schema changes are new migrations); the two CI guards
  (uniqueness, immutability); the generator contract. State that the shared
  `outbox`/`feed_offset` byte-identical migrations are also frozen.
- **Done when:** the ADR fully specifies the scheme. Every later phase reads it.

### Phase 2 — appkit runner: format support, drop contiguity
- **Scope:** `appkit/db/db.go` + `appkit/db/db_test.go`.
- **Changes:**
  - `LoadMigrations`: update the `NNN_name.sql` error message (`db.go:69`) to
    describe both forms; reject `Version <= 0` defensively; keep the duplicate
    guard unchanged.
  - Update the package doc comment (`db.go:1-9`) to describe the timestamp scheme
    + legacy integers.
  - `db_test.go`: **delete** the gap assertion in
    `TestLoadMigrations_OrderAndNaming` (`db_test.go:113-122`, "gaps not
    allowed"). Add tests: a 14-digit timestamp loads/parses; a mixed
    `[1,2,3, 20260607143022]` set sorts correctly; duplicates still error.
  - Audit/fix other contiguity assumptions: `wiki/internal/db/db_test.go:184`
    (`TestLoadMigrations_Order`) and `prompts/internal/db/redesign_migration_test.go:27`.
- **Verify:** `go test ./...` green in appkit, wiki, prompts.
- **Done when:** loader accepts mixed integer+timestamp sets, rejects dupes, no
  test asserts contiguity.

### Phase 3 — the generator `bin/new-migration`
- **Scope:** new `bin/new-migration` (+ a `.test.sh` matching the
  `bin/registry.test.sh` convention).
- **Changes:** read `bin/bump`/`bin/ship` first to match shell style. Signature
  `bin/new-migration <service> <name>`: validate `<service>/internal/db/migrations`
  exists; compute `date -u +%Y%m%d%H%M%S`; if that filename already exists, fail
  (don't silently collide); slugify `<name>`; write `<ts>_<slug>.sql` with a
  templated header comment; print the created path. Refuse use on the shared
  outbox/feed_offset files (library-coupled).
- **Verify:** generate into a real service, confirm `LoadMigrations` still loads,
  then remove the throwaway file; run the `.test.sh`.
- **Done when:** agents can create a correctly-named, unique migration with one
  command.

### Phase 4 — CI guard `bin/check-migrations` + wire into CI
- **Scope:** new `bin/check-migrations`; discover and wire the CI entry point
  (look for `.github/workflows`, a `Makefile`, or a `bin/test`/test runner — the
  agent finds where the suite test runs and adds the call).
- **Changes:** across every `*/internal/db/migrations/`:
  1. **Naming** — every file matches the legacy-int or timestamp form.
  2. **Uniqueness** — no duplicate numeric prefix within a service
     (`sort | uniq -d` on prefixes).
  3. **Immutability** — `git diff --name-status <merge-base origin/main>..HEAD`
     over migration paths; fail on any `M`/`D`/`R` of an existing migration (only
     `A` permitted).
  Non-zero exit with a precise message naming the offending file.
- **Verify:** craft a temp duplicate and a temp edit, confirm the script fails
  each; revert; confirm it passes clean.
- **Done when:** a colliding-or-modified migration fails **CI**, not deploy.

### Phase 5 — backfill per-service loader tests
- **Scope:** add a `TestLoadMigrations` (asserts `LoadMigrations(FS, "migrations")`
  returns no error) to services lacking one: **crm, ledger, notify, dropbox,
  dashboard, gmail, cron, sites, scripts** (wiki and prompts already have it).
- **Verify:** `go test ./...` per module (or via `go.work`) green.
- **Done when:** every service's own `go test` catches an in-service duplicate,
  complementing the repo-wide shell guard.

### Phase 6 — docs/AGENTS.md sweep + full verification
- **Scope:** add the `## Migrations — timestamped and immutable` section (below)
  to **`AGENTS.md`** only (root `CLAUDE.md` is a symlink to it); check whether
  `dashboard/CLAUDE.md` is also a symlink before touching `dashboard/AGENTS.md`.
  Update stale `NNN_name.sql` references elsewhere (service `CLAUDE.md`s, appkit
  comments, `docs/versioning.md`/runbooks), linking the Phase 1 ADR.
- **Verify (proof pass):** `go build ./...` and `go test ./...` across all modules
  green; `bin/check-migrations` exits 0 on clean `main`; a smoke run of
  `bin/new-migration` produces a valid file that loads, then is removed. Confirm
  every path the new AGENTS.md prose references (`bin/new-migration`,
  `bin/check-migrations`, the ADR) exists before committing.
- **Done when:** docs match reality and the whole suite + both guards are green.

#### AGENTS.md section to add

```markdown
## Migrations — timestamped and immutable

Each service owns its schema as ordered SQL files under
`<service>/internal/db/migrations/`, applied forward-only by the appkit runner
and tracked individually in `schema_migrations`. Two hard rules:

- **Never hand-pick a migration number, and never write one by hand.** Run
  `bin/new-migration <service> <name>`; it stamps a UTC timestamp version
  (`YYYYMMDDHHMMSS_name.sql`). Timestamps are why two agents on two branches
  don't collide — sequential integers did, and the clash only surfaced at
  deploy. (Legacy `NNN_*.sql` files predate this and stay frozen; they sort
  before any timestamp, so the two coexist.)
- **Never modify or delete a committed migration.** Once a migration is on
  `main` it is immutable — the runner keys on its version and will silently skip
  an edited body, so the change reaches new databases but not existing ones.
  Change schema by adding a *new* migration. `bin/check-migrations` enforces
  both rules in CI (no duplicate versions, no edits to existing files).

See `docs/adr-migration-timestamps.md`.
```

## Dependency chain

1 → 2 → 3 → 4 → 5 → 6 (strictly linear; each consumes the prior's output, all
anchored to the Phase 1 ADR).
