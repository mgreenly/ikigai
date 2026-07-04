# Suite on-box layout, deployment & backup/restore — Product

**Authority: intent.** This doc owns *why* this work exists, *for whom*, what is
in and out of scope, and the user-facing **promises** — stated once, in outcome
terms. It does **not** own mechanism, on-disk paths, permission bits, S3 key
layouts, version-string formats, exit codes, or test assertions; those belong to
`project/design/README.md` and its Decisions. Where the two could overlap on
behavior, this doc states the **promise** and the design states the **exact,
checkable proof** of it. That boundary is load-bearing: it is what keeps product,
design, and plan from restating each other. The normative source notes this doc
realizes are `docs/app-layout.md` (the on-box tree and its delivery) and
`docs/backups-design.md` (off-box-only backup/restore); the operator runbook
`deploy.md` is brought into line with them as part of this work.

## Problem

On the box the rule is supposed to be that every service is delivered, deployed,
and recovered the **same** way — one uniform tree, one app-agnostic toolchain,
parameterized by nothing but the service name. The delivery half does not yet
hold to that. The unit a release ships is a **bare binary**: anything that is not
the compiled binary — the service's nginx location fragment today, shipped
read-only resources tomorrow — is applied out-of-band, by hand, at setup time and
never re-applied on deploy. So a fragment change can sit live in the source tree
while the box keeps serving the old one, and a rollback returns the old binary
**without** the config that matched it. The install tree itself has drifted from
the schema: it still carries a local `backups/` tier, permission bits that
disagree across several sources, and stale `data/` paths that predate the
durable-vs-disposable split — so "what is state" has no single honest answer and
ownership of the served tree is contested.

Backups have a residual hole in the same spirit. A backup is only a backup if it
is **off the box** — yet one path still writes a backup to local disk (a
per-binary verb leaving a pre-migration file under the box's own tree). So "a
backup" is not reliably off the box, and a dead disk can take the backup down
with the service it was meant to protect.

And fleet recovery must stay within a hard limit: every customer box answers on a
subdomain of the single registered domain `ikigenba.com`, and TLS-certificate
reissue is rate-limited per registered domain, so a mass rebuild that reissued
certificates would exhaust the shared quota and strand boxes.

## Purpose

A single, uniform, **app-agnostic** on-box discipline: one install tree, one
versioned delivery bundle, one deploy / rollback / prune path, and one off-box
backup / restore path — so every deployable service is delivered, deployed,
rolled back, and recovered **identically**, parameterized only by its name (and
loopback port). One way to do each thing, for every service, with nothing retained
on the box that a rebuild cannot either reproduce from the release or restore from
off-box.

## Users

The **box operator** (the person running the suite's off-box build tooling and the
on-box `opsctl` CLI — today, the suite maintainer). They are trying to: ship a new
version and roll it back, *config and all*, if it misbehaves; trust that a nightly
backup is happening without tending it; and, when a disk dies or a box must be
rebuilt, recover a service — or an entire box, or the entire fleet — to its last
good state quickly and without special per-service knowledge. The end customer
never touches any of this; their only stake is that their box comes back when it
has to.

## Scope

In scope — applied uniformly to **every deployable service in the suite** (the
twelve that carry a `VERSION`: dashboard, crm, ledger, notify, dropbox, prompts,
wiki, cron, gmail, scripts, sites, **webhooks**):

- A **uniform on-box install tree** per service: one self-contained, FHS-mirroring
  home holding the service's shipped binaries and resources, its host config, its
  durable state, and its disposable runtime data — with durable state and
  disposable data **explicitly separated** so "what is state" has one honest
  answer, and with **nothing retained on the box that is treated as a backup**.
- A **versioned delivery bundle**: the unit of delivery is one versioned artifact
  carrying **every shipped tier together** — the binary plus its host config (the
  nginx location fragment) plus any shipped read-only resources — staged and then
  activated atomically, and **retained per version** so a rollback re-applies the
  *matching* config and resources, not just the matching binary.
- **Shipped config is deploy-applied:** a service's nginx location fragment travels
  in the bundle and is installed on every deploy, so a config change can never be
  live-in-the-release yet un-applied on the box.
- A **single version contract** (SemVer 2.0, `v`-prefixed) produced by the build
  tooling and consumed by every operation that orders or names a version, so
  deploy, rollback, and prune agree by construction.
- **Atomic deploy, one-command rollback, and safe prune** built on that layout and
  version contract: a deploy backs up first, then swaps; a rollback returns to the
  prior version *and its bundle*; prune keeps the newest few by correct SemVer
  order.
- **Uniform, off-box-only backup and restore**, owned by `opsctl` and identical for
  every service — never a per-binary verb and never per-service knowledge: stop,
  snapshot durable state, upload immutably off-box, restart; restore by replacing
  durable state from a chosen off-box snapshot.
- **Rate-limit-safe fleet recovery**: the apex TLS certificate is backed up and
  restored from object storage rather than reissued, so rebuilding many boxes
  consumes no issuance quota.
- **Unattended nightly backups** of the whole box, and a **deliberately guarded,
  human-confirmed restore**.
- A **single, reconciled ownership and permission model** for the install tree,
  applied uniformly by setup, that lets nginx read each service's served tree while
  the rest of the tree stays private to the service.
- A **one-time, safe migration** of the existing live box from the old layout to
  the new one — including retiring the old `data/` and `backups/` locations.

Out of scope — nothing else is promised here:

- **No zero-downtime deploy, backup, or restore.** Short, scheduled per-service
  downtime is accepted by design; there is no live-snapshot or hot-failover
  machinery.
- **No scheduled restore.** Restore is interactive-only, every time.
- **No end-customer-facing surface.** This is operator tooling; there is no UI and
  no MCP exposure of deploy, backup, or restore.
- **No backup of disposable runtime data**, and **no retained on-box backup of any
  kind**. Only durable state is preserved, only off-box; everything a service can
  rebuild on boot is intentionally excluded, and nothing on local disk is a backup
  of record.

## Contractual constants

Fixed, promised values the design must use verbatim and never re-derive:

- **Versions are SemVer 2.0, `v`-prefixed** (e.g. `v0.7.1`) everywhere a version
  appears.
- **Nightly backup runs at 03:00 America/Chicago** (Central, DST-correct).
- **Default backup retention: 30** most-recent snapshots per service
  (operator-configurable; 30 is the out-of-the-box default).
- **Default object-storage region: `us-east-2`** (operator-configurable).

## What we promise (user-facing behavior)

- **Every service is delivered and deployed the same way.** A release is one
  versioned bundle carrying the binary and everything shipped alongside it (its
  nginx fragment, any read-only resources). Staging then deploying it makes the new
  version live with one app-agnostic command per step — no per-service script — and
  the only things that differ between services are the name and the loopback port.
- **Config ships and rolls back with its binary.** The nginx fragment (and any
  shipped resource) travels in the bundle and is applied on deploy, so it can never
  be live-but-un-applied; a rollback re-applies the fragment and resources that
  matched the version it returns to.
- **Deploy, rollback, and prune pick the right binary.** A deploy swaps the live
  version with no moment where the service points at a missing binary; a rollback
  returns to the still-present prior version with one command; pruning keeps the
  newest few by correct SemVer order and never deletes the live or rollback target.
  Pre-release and build-metadata versions order correctly, not alphabetically.
- **Every deploy backs up first.** Before a deploy changes anything — whether or
  not it advances the schema — it captures the pre-deploy durable state off-box, so
  the state you had before the deploy is always recoverable.
- **A backup is only a backup if it is off the box.** Backups live off-box only;
  nothing retained on local disk is ever treated as a backup of record, so a dead
  disk never takes the backup with it.
- **Every service backs up and restores the same way.** `opsctl backup <svc>` and
  `opsctl restore <svc>` work identically for any service, with no per-service
  knowledge. The same command sweeps the whole box (`opsctl backup --all`).
- **A restore brings a service back to a chosen good state.** After a restore the
  service's durable state matches the snapshot that was restored, byte for byte,
  and the service comes back healthy on its own — recreating its disposable runtime
  data and re-minting a fresh event epoch automatically, so it never resumes onto a
  stale event sequence. Restoring an older snapshot never silently carries newer
  runtime data forward.
- **Restore is always deliberate.** Restore never proceeds without an interactive
  confirmation.
- **Rollback is the immediate undo of the deploy you just made.** Rolling back
  restores the pre-deploy state and returns the prior version with its bundle.
  Reverting to an old version *after* later backups have superseded that pre-deploy
  state is a forward deploy of an older version, not a rollback — and that boundary
  fails **loudly** (the forward-only downgrade guard refuses to boot) rather than
  silently restoring a mismatched state.
- **Nightly backups just happen.** Without anyone tending it, the box backs every
  service up once a night at 3 AM Central. If one service fails to back up, the
  rest still do, and the failure is surfaced rather than swallowed. A box that was
  off at 3 AM backs up at next boot.
- **The fleet can be rebuilt within certificate limits.** Recovering a box — or
  many boxes — restores the apex TLS certificate from backup instead of reissuing
  it, so a fleet-wide rebuild consumes no certificate-issuance quota.
- **"What is state" is explicit and stays honest.** Each service's durable data is
  clearly delimited from its disposable data, nothing on the box is a retained
  backup, and the regular restore drills are what keep that line correct — anything
  misfiled outside durable state is caught the first time a drill would lose it.
- **The served tree is readable by nginx and nothing else is exposed.** One uniform
  ownership model across every service lets nginx read each service's served files
  while the rest of the service's tree stays private to the service.
- **Backup secrets stay opaque.** The apex certificate's private key travels
  through backup and restore as opaque bytes that are never read, printed, or
  logged.
- **The live box migrates safely.** Moving the existing box to the new layout is a
  one-time, non-destructive, re-runnable step that drops no data and leaves a
  bootable service even if interrupted.

## Success criteria (outcomes)

Each item is a result the operator can confirm end-to-end against the real, built
tooling:

- For **every** deployable service, a release is delivered as a single versioned
  bundle; staging then deploying it makes the new binary live and installs its
  shipped config and resources, with no moment where the live entrypoint points at
  a missing binary.
- A change to a service's nginx fragment, shipped in its bundle and deployed, is
  the fragment serving on the box afterward with no separate manual nginx step; a
  rollback returns the previous fragment and resources together with the previous
  binary.
- For **every** deployable service, `opsctl backup <svc>` followed by
  `opsctl restore <svc>` returns the service's durable state to exactly what it was
  at backup, and the service comes back healthy.
- After a backup completes, no backup artifact remains on the box's local disk —
  the only durable copy is off-box.
- After restoring an **older** snapshot, the service is healthy and shows none of
  the disposable runtime data or event epoch that existed before the restore.
- A service started against a wiped runtime area (no caches, no event sidecar)
  recreates what it needs and reaches a healthy state on its own.
- `opsctl restore` cannot be made to run without an interactive confirmation.
- `opsctl backup --all` backs up every service plus the apex certificate in one
  run; if one service is wedged, the others are still backed up and the run reports
  the failure.
- The nightly schedule is installed such that backups fire at 03:00 America/Chicago
  and a missed run (box was down) fires at next boot.
- With more than the configured retention of snapshots present, a backup run prunes
  the oldest down to the configured count (default 30) and never removes the
  most-recent pointer.
- Recovering a box restores the apex TLS certificate from backup and issues **no**
  new certificate (no issuance-quota consumption).
- A deploy never leaves the live entrypoint pointing at a missing binary; a
  rollback returns to the prior version — binary and bundle — in one command; a
  rollback attempted after a later backup has superseded the pre-deploy state fails
  loudly rather than booting a mismatched state.
- Given mixed release / pre-release / build-metadata versions,
  deploy / rollback / prune select by correct SemVer precedence; the build tooling
  produces, and `opsctl` accepts, only `v`-prefixed SemVer 2.0 versions, rejecting a
  bare or malformed version rather than mis-handling it.
- Across every service, the installed permission/ownership model is the same one
  model: nginx can read the service's served tree and the rest of the tree is not
  world-exposed.
- Running the one-time migration against an old-layout box produces a working
  new-layout service with its data intact — old `data/` and `backups/` locations
  retired — and running it again changes nothing.
