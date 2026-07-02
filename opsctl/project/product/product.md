# opsctl — Product

**Authority: intent.** This doc owns *why* opsctl exists, *for whom*, what is in
and out of scope, and what we **promise** the operator — stated once, in
**outcome terms**. It does **not** state mechanism, file layouts, exit codes, or
test assertions; those belong to `project/design/README.md`. Where the two could
overlap (behavior), product states the *promise* and design states the *exact,
checkable proof* of that promise. That boundary is load-bearing: it keeps
product, design, and plan from restating each other.

## Problem

Every box-side operation on an ikigenba box — staging a release, deploying it,
rolling back, backing up, restoring, pruning, provisioning — has to happen on a
**real machine** with a real filesystem topology and a real box environment. An
operator running these by hand, under `sudo`, needs each one to behave the same
on the box as it does in a test: if a command works against a temp dir but fails
the moment `/tmp` and `/opt` are separate mounts, or a command silently lacks the
box's S3 credentials because nothing loaded them, the operator discovers it only
mid-operation, on the live box, at the worst possible time.

## Purpose

opsctl is the single on-box CLI that owns every box-side operation for an
ikigenba box. One privileged binary, invoked as `sudo opsctl <verb>`, performs
the stage / deploy / rollback / prune / status / backup / restore / provision
work so the operator never hand-runs the underlying filesystem, systemd, or
snapshot steps.

## Users

The **operator** — the person (or an automated deploy step acting as them) who
brings a box up, ships new releases to it, and recovers it. They run opsctl
interactively over SSH under `sudo`, and they expect each verb to either
complete or fail loudly with a clear reason, never to half-succeed because the
environment it ran in differed from the one it was tested in.

## Scope

opsctl owns the box-side lifecycle verbs and nothing else. It stages and swaps
release bundles, migrates and snapshots per-app state, and provisions the box.
It does **not** build or version the release artifacts (that is off-box tooling
under `bin/`), it does **not** implement any app's domain behavior, and it is
**not** itself a released, versioned application — it is tooling installed to the
box by hand. Delivering the release bundle in the layout opsctl expects is the
producer's job, not opsctl's.

## What we promise (operator-facing behavior)

- **A verb behaves the same on the box as under test.** A command that succeeds
  in isolation does not fail on the real box merely because the box's filesystem
  is laid out across more than one mount. Staging a release works whether or not
  the operator's temp directory and the install root sit on the same filesystem.
- **Interactive verbs have the box's environment.** When the operator runs a verb
  by hand under `sudo`, opsctl already has the box's operational environment (for
  example, the backup destination and region) available to it, without the
  operator having to remember to load it first. A verb that requires the box
  environment does not fail for lack of it when run interactively.
- **Failures are loud and specific.** When a verb cannot proceed, it stops with a
  message naming what was missing, rather than silently skipping a step or
  leaving the box half-changed.

## Success criteria (outcomes)

- On a box whose temp directory and install root are on **different** filesystems,
  `opsctl stage <app> <version>` completes and the release is staged, with no
  cross-device failure.
- Running `sudo opsctl <verb>` interactively on the box, a verb that needs the
  box's operational environment (such as the backup bucket) finds it already
  available and proceeds, without the operator manually loading the box env
  first.
- When the box environment genuinely is absent, the affected verb fails with a
  message that names the missing value, rather than proceeding as if it were
  present.
