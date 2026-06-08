# ADR — Timestamped, immutable migrations

> **Status: ACCEPTED (2026-06-07).** This is the durable architecture decision
> record for how suite services name, generate, and protect their schema
> migrations. It is the contract every phase of `docs/plan-migration-timestamps.md`
> reads and implements; the loader (`appkit/db/db.go`), the generator
> (`bin/new-migration`), the CI guard (`bin/check-migrations`), and the prose in
> `AGENTS.md` all derive their behavior from this document.
>
> **Scope.** The per-service migration sets under `<service>/internal/db/migrations/`
> and the appkit runner that applies them. It does **not** change the deploy model
> (`docs/adr-deployment-redesign.md`), the downgrade guard's contract, or any
> service's schema.
>
> It is written in the house *context → decision → consequences* shape of
> `docs/event-plane-decisions.md` and `docs/adr-deployment-redesign.md`.

---

## Context — what we are replacing

Each service owns its schema as a set of ordered SQL files under
`<service>/internal/db/migrations/`, embedded into the binary and applied
forward-only by the appkit runner. Today every file is named `NNN_name.sql` with
a **hand-picked sequential integer** version (`001_…`, `002_…`, `003_…`). The
appkit runner parses that prefix and tracks each version individually in
`schema_migrations` (`appkit/db/db.go`).

Two problems follow from the hand-picked integer.

### 1. Integer versions collide silently across branches

The "next number" is chosen from a single branch's local view of the migration
directory. When two agents add a migration to the **same service** on two
branches, both independently pick the same next integer — e.g. both write a
`004_*.sql`. Because the files have **different names** (`004_a.sql`,
`004_b.sql`), git merges them with **no textual conflict**: the colliding
versions land on `main` cleanly and nothing notices.

The only duplicate check is the dedup loop in `LoadMigrations`
(`appkit/db/db.go:82-86`):

```go
for i := 1; i < len(out); i++ {
    if out[i].Version == out[i-1].Version {
        return nil, fmt.Errorf("migration version %d duplicated (%s and %s)", ...)
    }
}
```

That check runs only when `LoadMigrations` is called — i.e. at `migrate`/`serve`,
which is **deploy time**. Worse, most services lack a test that loads their real
embedded set, so `go test ./...` stays green on the colliding `main`. The
collision is discovered, at the earliest, when `opsctl deploy` runs `migrate` on
the box.

### 2. Editing an applied migration causes undetected schema drift

The runner keys on the version integer. `Migrate` builds an `applied` set from
`schema_migrations` and **skips any version already recorded**
(`appkit/db/db.go:128-131`):

```go
for _, m := range migs {
    if applied[m.Version] {
        continue
    }
    ...
}
```

So if someone edits the **body** of an already-applied migration, existing
databases — which already carry that version — silently skip the new body, while
freshly-created databases apply it. The two diverge with no error and no signal:
**undetected schema drift**.

Rails solved the first problem with timestamp versions (collision becomes
astronomically unlikely, and per-version tracking — which we already have —
tolerates out-of-order merges) and the second with convention + tooling
(migrations are immutable; you change schema by adding a new one). This ADR
adopts both.

---

## Decision

### New migrations use a generated UTC timestamp version

Every **new** migration is named:

```
YYYYMMDDHHMMSS_name.sql
```

— a 14-digit UTC timestamp prefix, an underscore, a descriptive slug, `.sql`.
The version is **generated, never hand-picked**: it is produced by
`bin/new-migration` (see *The generator contract*), which stamps
`date -u +%Y%m%d%H%M%S`. Hand-typing a migration filename — integer or timestamp
— is forbidden.

Two agents on two branches generating a migration at different wall-clock seconds
get different timestamps; the merge carries both with no collision. If two are
generated in the same second the generator refuses (it fails rather than silently
collide — see the generator contract), and the CI uniqueness guard is the
backstop.

### Legacy integer migrations are frozen

Existing `NNN_*.sql` files are **legacy** and stay exactly as they are. They are
**never renumbered** and never converted to timestamps. New schema changes are
always timestamp migrations layered on top.

---

## Transition — the two schemes coexist in one sorted run

The runner sorts migrations numerically by version
(`appkit/db/db.go:81`) and applies forward-only. A 14-digit timestamp
(e.g. `20260607143022` ≈ 2.0 × 10¹³) is vastly larger than any legacy integer
(`1`, `2`, `3`), so **timestamps sort strictly after the legacy integers**. A
single service can therefore hold both `001_init.sql … 003_outbox.sql` and
`20260607143022_add_widgets.sql` in one directory, and the runner orders them
correctly with no special-casing:

```
1, 2, 3, 20260607143022, 20260608090501, …
```

Consequences of this ordering choice:

- **No collision between schemes** — the integer space (1..N) and the
  timestamp space (10¹³..) never overlap.
- **The downgrade guard stays happy.** The guard (`appkit/db/db.go:114-125`)
  refuses to start on a DB carrying an applied version this binary no longer
  embeds. Because legacy versions are frozen into the binary forever and new
  timestamps are only ever *added*, every applied version remains embedded.
- **No risky one-time `schema_migrations` rewrite on production.** Live DBs on
  `int` already carry versions `1, 2, 3`. They do **not** need any backfill or
  renaming: the binary keeps embedding `1, 2, 3`, the DB keeps recording them,
  and new timestamp migrations simply apply on top as the next unapplied
  versions. Nothing rewrites `schema_migrations` on a live box.

This is the central safety property of the design: the migration is **additive
and forward-only on production**, not a migration *of* the migration ledger.

---

## Implementation note — `Version int` is kept (not widened)

A 14-digit timestamp (`20260607143022`) requires a 64-bit integer to hold
(`int32` overflows past ~2.1 × 10⁹). We **keep** the existing `Version int` field
(`appkit/db/db.go:48-52`) rather than widen it to `int64`, because:

- The only build target is **`linux/amd64`**, where Go's `int` is 64-bit. On that
  target `strconv.Atoi` (`appkit/db/db.go:71`) parses a 14-digit timestamp into
  an `int` without overflow.
- Keeping `int` keeps the diff out of `opsctl` and everything else that consumes
  `Migration.Version` / `AppliedVersion` / `MaxEmbedded` as `int`.

As a defensive measure, the loader adds a **`Version > 0`** check (a 14-digit
timestamp is always positive; this rejects a `0`- or negative-prefixed file as
malformed rather than letting it sort ahead of everything). This is the only
behavioral change the runner needs to accept timestamps; the existing duplicate
guard is kept unchanged.

---

## Immutability — a committed migration is never edited or deleted

Once a migration is on `main` it is **immutable**. You do not edit its body, you
do not delete it, you do not rename it. The runner keys on the version: an edited
body is silently skipped on existing DBs (problem #2 above), so an "edit" reaches
new databases but not existing ones — exactly the drift we are eliminating.

**To change schema, add a new migration.** That is the only supported mechanism.
A correction to a prior migration is itself a new, forward-only migration.

---

## The two CI guards (implemented later in `bin/check-migrations`)

The collision and edit problems are both caught at deploy today. We move both
checks to **CI**, where they fail a PR instead of a box. `bin/check-migrations`
(a later phase) enforces, across every `*/internal/db/migrations/`:

1. **Uniqueness** — no duplicate numeric prefix *within a service*. A
   `sort | uniq -d` over the per-service prefix set must be empty. This is the
   merge-collision backstop, moved from `LoadMigrations` at deploy to CI.
2. **Immutability** — no edits, deletes, or renames of an existing migration;
   **only additions**. Computed from
   `git diff --name-status <merge-base origin/main>..HEAD` over the migration
   paths: any `M` / `D` / `R` of a file that already exists on `main` is a
   failure; only `A` is permitted.

Both guards exit non-zero with a precise message naming the offending file.

---

## The generator contract

`bin/new-migration <service> <name>`:

- validates that `<service>/internal/db/migrations` exists;
- computes the version as `date -u +%Y%m%d%H%M%S` (UTC);
- if a file with that exact timestamp prefix already exists, **fails** (it never
  silently collides);
- slugifies `<name>` and writes `<ts>_<slug>.sql` with a templated header
  comment;
- prints the created path.

Agents create migrations **only** through this command. The generator is the
single point at which a version number comes into existence.

---

## The shared `outbox` / `feed_offset` migrations are also frozen

The event-plane producer/consumer plumbing ships byte-identical migration files
into the services that participate in the event plane — the `*_outbox.sql`
(producers) and `*_feed_offset.sql` (consumers) files, e.g.
`crm/internal/db/migrations/003_outbox.sql`,
`ledger/internal/db/migrations/003_outbox.sql`,
`notify/internal/db/migrations/002_feed_offset.sql`,
`wiki/internal/db/migrations/003_feed_offset.sql`, and the same pair across
`dropbox`, `gmail`, `cron`, `scripts`, `prompts`. These are **library-coupled**:
their content is owned by the event-plane library convention, not by an
individual service.

They are **frozen** under the same legacy rule. They keep their integer prefixes,
they are never renumbered, and `bin/new-migration` refuses to operate on them.
Any change to the shared outbox/feed_offset schema is a coordinated, library-level
decision — not a per-service migration edit — and would land as a new migration,
never an edit to the existing byte-identical files.

---

## Consequences

- **Collisions and edits fail at PR time, not deploy time.** The two CI guards
  move both classes of bug off the box.
- **Production migrates additively.** Live DBs carrying `1, 2, 3` apply new
  timestamp migrations on top with no ledger rewrite; the downgrade guard is
  never tripped.
- **The runner change is minimal.** `LoadMigrations` accepts both naming forms
  (the prefix is parsed by `strconv.Atoi` either way), gains a `Version > 0`
  check, and updates its error message to describe both forms; `Version` stays
  `int`. No change to `Migrate`, the downgrade guard, `AppliedVersion`,
  `MaxEmbedded`, or `opsctl`.
- **Migrations become append-only by rule and by CI.** "Change the schema" always
  means "add a new timestamped migration."

See `docs/plan-migration-timestamps.md` for the phased implementation and
`AGENTS.md` for the day-to-day operator rules.
