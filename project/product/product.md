# Suite on-box layout, versioning & backup/restore — Product

**Authority: intent.** This doc owns *why* this work exists, *for whom*, what is
in and out of scope, and the user-facing **promises** — stated once, in outcome
terms. It does **not** own mechanism, on-disk paths, permission bits, S3 key
layouts, version-string formats, exit codes, or test assertions; those belong to
`project/design/design.md` and its Decisions (D1–D9). Where the two could
overlap on behavior, this doc states the **promise** and the design states the
**exact, checkable proof** of it. That boundary is load-bearing: it is what keeps
product, design, and plan from restating each other.

## Problem

Recovering a service on the box is not something an operator can do the same way
twice. Backups today are **three incompatible answers plus seven non-answers**:
`crm`, `prompts`, and `scripts` each ship their own `bin/backup`/`bin/restore`
shell script with different conventions (immutable timestamped snapshots in some,
a history-less mirror in others); the dashboard buries its own S3 path inside the
binary; and the remaining services have **no off-box backup at all**. Nothing
agrees on what is even worth backing up — runtime junk and durable data sit
together in one `data/` directory, so "what is state?" has no enforced answer and
a restore can silently carry stale derived data forward.

Two failures make this acute. First, **fleet recovery doesn't scale**: every
customer box answers on a subdomain of the single registered domain
`ikigenba.com`, and TLS-certificate reissue is rate-limited per registered
domain, so a mass rebuild that reissued certificates would exhaust the shared
quota and strand boxes. Second, the supporting machinery is **quietly wrong**:
versions are ordered by a hand-rolled comparison that mis-sorts pre-releases and
build metadata (so "roll back to the prior release" and "prune to the newest few"
can pick the wrong binary), and the event-plane epoch sidecar must be hand-deleted
on every restore — a step two of the three existing scripts already forget,
letting a restored service silently resume on a reused event sequence.

## Purpose

A single, uniform on-box discipline — one install layout, one version contract,
one backup/restore path — so that **any service can be backed up off-box and
recovered identically**, a whole box or the whole fleet can be rebuilt without
exhausting certificate-issuance limits, and the operations that move versions
around (deploy, rollback, prune) pick the right binary every time. One way to do
each of these, applied to every deployable service.

## Users

The **box operator** (the person running the suite's off-box tooling and the
on-box `opsctl` CLI — today, the suite maintainer). They are trying to: ship a
new version and roll it back if it misbehaves; trust that a nightly backup is
happening without tending it; and, when a disk dies or a box must be rebuilt,
recover a service — or an entire box, or the entire fleet — to its last good
state quickly and without special per-service knowledge. The end customer never
touches any of this; their only stake is that their box comes back when it has to.

## Scope

In scope — applied uniformly to **every deployable service in the suite** (the
twelve that carry a `VERSION`: dashboard, crm, ledger, notify, dropbox, prompts,
wiki, cron, gmail, scripts, sites, **webhooks**):

- A **uniform on-box install tree** per service: one self-contained home holding
  the service's binaries, its durable state, its disposable runtime data, and its
  local snapshots — with durable state and disposable data **explicitly
  separated** so "what is state" has one honest answer.
- A **single version contract** (SemVer 2.0, `v`-prefixed) produced by the build
  tooling and consumed by every operation that orders or names a version, so
  deploy, rollback, and prune agree by construction.
- **Atomic deploy, one-command rollback, and safe prune** built on that layout and
  version contract (the deploy *pipeline* itself — staging, the launch chain — is
  not redesigned here; only the version-selection and swap behavior it relies on).
- **Uniform backup and restore to off-box object storage**, owned by `opsctl` and
  identical for every service: stop, snapshot durable state, upload immutably,
  restart; restore by replacing durable state from a chosen snapshot.
- **Rate-limit-safe fleet recovery**: the apex TLS certificate is backed up and
  restored from object storage rather than reissued, so rebuilding many boxes
  consumes no issuance quota.
- **Unattended nightly backups** of the whole box, and a **deliberately guarded,
  human-confirmed restore**.
- A **one-time, safe migration** of the existing live box from the old layout to
  the new one.

Out of scope — nothing else is promised here:

- **No redesign of the deployment pipeline.** The end-to-end `bump → ship → stage
  → deploy` runbook and the launch chain stay as documented in `deploy.md`; this
  work changes only the version-selection and swap behavior they depend on.
- **No zero-downtime backup or restore.** Short, scheduled per-service downtime is
  accepted by design; there is no live-snapshot or hot-failover machinery.
- **No scheduled restore.** Restore is interactive-only, every time.
- **No end-customer-facing surface.** This is operator tooling; there is no UI and
  no MCP exposure of backup/restore.
- **No backup of disposable runtime data.** Only durable state is preserved;
  everything a service can rebuild on boot is intentionally excluded.

## Contractual constants

Fixed, promised values the design must use verbatim and never re-derive:

- **Versions are SemVer 2.0, `v`-prefixed** (e.g. `v0.7.1`) everywhere a version
  appears.
- **Nightly backup runs at 03:00 America/Chicago** (Central, DST-correct).
- **Default backup retention: 30** most-recent nightly snapshots per service
  (operator-configurable; 30 is the out-of-the-box default).
- **Default object-storage region: `us-east-2`** (operator-configurable).

## What we promise (user-facing behavior)

- **Every service backs up and restores the same way.** `opsctl backup <svc>` and
  `opsctl restore <svc>` work identically for any service, with no per-service
  script and no per-service knowledge. The same command sweeps the whole box
  (`opsctl backup --all`).
- **A restore brings a service back to a chosen good state.** After a restore, the
  service's durable state matches the snapshot that was restored, byte for byte,
  and the service comes back healthy on its own — recreating its disposable
  runtime data and re-minting a fresh event epoch automatically, so it never
  resumes onto stale event sequences. Restoring an older snapshot never silently
  carries newer runtime junk forward.
- **Restore is always deliberate.** Restore never proceeds without an interactive
  confirmation, and it sets aside a safety copy of the current state before
  replacing anything — so an accidental or wrong restore is itself recoverable.
- **Nightly backups just happen.** Without anyone tending it, the box backs every
  service up once a night at 3 AM Central. If one service fails to back up, the
  rest still do, and the failure is surfaced rather than swallowed. A box that was
  off at 3 AM backs up at next boot.
- **The fleet can be rebuilt within certificate limits.** Recovering a box — or
  many boxes — restores the apex TLS certificate from backup instead of reissuing
  it, so a fleet-wide rebuild consumes no certificate-issuance quota.
- **Deploy, rollback, and prune pick the right binary.** A deploy swaps the live
  version with no moment where the service points at a missing binary; a rollback
  returns to the still-present prior version with one command; pruning keeps the
  newest few by correct SemVer order and never deletes the live or rollback
  target. Pre-release and build-metadata versions order correctly, not
  alphabetically.
- **"What is state" is explicit and stays honest.** Each service's durable data is
  clearly delimited from its disposable data, and the regular restore drills are
  what keep that line correct — anything misfiled outside durable state is caught
  the first time a drill would lose it.
- **Backup secrets stay opaque.** The apex certificate's private key travels
  through backup and restore as opaque bytes that are never read, printed, or
  logged.
- **The live box migrates safely.** Moving the existing box to the new layout is a
  one-time, non-destructive, re-runnable step that drops no data and leaves a
  bootable service even if interrupted.

## Success criteria (outcomes)

Each item is a result the operator can confirm end-to-end against the real,
built tooling:

- For **every** deployable service, `opsctl backup <svc>` followed by
  `opsctl restore <svc>` returns the service's durable state to exactly what it
  was at backup, and the service comes back healthy.
- After restoring an **older** snapshot, the service is healthy and shows none of
  the disposable runtime data or event epoch that existed before the restore.
- A service started against a wiped runtime area (no caches, no event sidecar)
  recreates what it needs and reaches a healthy state on its own.
- `opsctl restore` cannot be made to run without an interactive confirmation, and
  a safety copy of the prior state exists after any restore.
- `opsctl backup --all` backs up every service plus the apex certificate in one
  run; if one service is wedged, the others are still backed up and the run
  reports the failure.
- The nightly schedule is installed such that backups fire at 03:00 America/
  Chicago and a missed run (box was down) fires at next boot.
- With more than the configured retention of snapshots present, a backup run
  prunes the oldest down to the configured count (default 30) and never removes
  the most-recent pointer or the pre-restore safety copies.
- Recovering a box restores the apex TLS certificate from backup and issues **no**
  new certificate (no issuance-quota consumption).
- A deploy never leaves the live entrypoint pointing at a missing binary; a
  rollback returns to the prior version in one command; given mixed
  release/pre-release/build-metadata versions, deploy/rollback/prune select by
  correct SemVer precedence.
- The build tooling produces, and `opsctl` accepts, only `v`-prefixed SemVer 2.0
  versions; a bare or malformed version is rejected rather than mis-handled.
- Running the one-time migration against an old-layout box produces a working
  new-layout service with its data intact, and running it again changes nothing.
