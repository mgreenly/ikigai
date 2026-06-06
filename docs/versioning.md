# Versioning — the bump→ship→stage→deploy workflow

> **Operator how-to.** This is the concrete, day-to-day procedure for advancing
> the version of a suite service and shipping it. The *why* lives in
> [`adr-deployment-redesign.md`](./adr-deployment-redesign.md) (§6 Versioning, the
> decisions list, and the "known build bugs" appendix) and in `PLAN.md` §1.6 /
> §F1 — on any conflict those win and this doc is corrected to match.

## TL;DR

```sh
# 1. advance the version for the app you're releasing (you never type a number):
bin/bump crm patch                # X.Y.Z -> X.Y.(Z+1); writes crm/VERSION, commits to main, pushes
bin/bump crm minor                # X.Y.Z -> X.(Y+1).0
bin/bump crm major                # X.Y.Z -> (X+1).0.0

# 2. ship it (build current main off-box, scp the static binary to the box /tmp):
bin/ship crm                      # no version arg — builds HEAD, version from crm/VERSION;
                                  #   then prints the two box commands and stops

# 3. on the box, stage then activate (the commands bin/ship printed):
sudo opsctl stage crm v1.4.0 --artifact /tmp/crm-v1.4.0   # preflight + place release, not live
sudo opsctl deploy crm v1.4.0                             # manifest + migrate + atomic swap + restart
```

The binary self-reports what it is — `crm version` → `v1.4.0 (<sha>)` — so the
box can never lie about what's deployed.

## The version convention

- **The source of truth is the committed file `<app>/VERSION`** — a bare SemVer
  number with **no** leading `v` (e.g. `0.1.1`), one line. Each of the seven
  deployable services carries one: `dashboard`, `crm`, `ledger`, `notify`,
  `dropbox`, `ralph`, `wiki`. There is **no global suite version**; each service
  versions independently.
- **Git tags are NOT the version mechanism.** There is no `git tag <app>/vX.Y.Z`
  and no `git describe` lookup. The version state lives in `main`'s commit history
  (the committed `<app>/VERSION` file), which — under branch protection (below) —
  makes it durable and immutable. Any release tags that survive from the old
  scheme are **vestigial/historical**: they are left in place but drive nothing.
- **The `v` prefix is a deploy-time display/release convention**, not stored in
  the file. The file holds the bare `0.1.1`; `bin/ship` prepends the `v` so the
  on-box release dir is `/opt/<app>/releases/v0.1.1/` and the binary self-reports
  `v0.1.1 (<sha>)`. `bin/bump` deals only in bare numbers (it shows `v…` only in
  the commit message / human-facing prints).
- **The commit pins code + libraries atomically.** Because the in-repo libraries
  are consumed via committed `replace` directives (not versioned `require`s), the
  commit on `main` already contains the exact `eventplane`/`agentkit`/`appkit`
  source that app builds against. The commit SHA (stamped into the binary as
  `appkit.commit`) ties the app code and its committed `replace … => ../<lib>`
  library trees together — one commit = one reproducible build. (This is the job a
  tag used to do; the commit does it now.)

### Libraries and `opsctl` are NEVER versioned

`eventplane`, `agentkit`, and `appkit` are **in-repo sibling libraries consumed at
HEAD** via a committed `replace <lib> => ../<lib>` + `require <lib> v0.0.0`
(`agentkit` uses the zero-pseudo-version `v0.0.0-00010101000000-000000000000`;
same mechanism). They carry **no `<lib>/VERSION` file** and get no version. The
on-box platform CLI `opsctl` is likewise unversioned. Only the seven deployable
services have a `VERSION` file — `bin/bump` refuses anything else.

> **HARD RULE:** never convert an internal `replace` into a versioned `require`.
> Doing so drags in the Go module proxy + subdir machinery this whole design
> routes around. The libraries ship as source-in-the-committed-tree, full stop.

## How `bin/bump` advances the version

`bin/bump <app> <major|minor|patch> [--dry-run]` is the companion to `bin/ship`:
`bump` decides the version, `ship` builds + uploads it (and the box `stage`/`deploy`
verbs activate it). They stay separate steps — bumping never touches the live box.
`bump`:

1. reads the current bare number from the committed `<app>/VERSION`,
2. increments the requested field (`patch` → X.Y.Z+1, `minor` → X.Y+1.0,
   `major` → X+1.0.0),
3. writes the new **bare** number back to `<app>/VERSION`,
4. makes a **path-limited** commit of **only** that file straight to `main`
   (`git commit -m "<app>: bump version to v<new>" -- "<app>/VERSION"` — so
   unrelated working-tree edits are never swept into the version commit),
5. `git push origin main`.

It **enforces it runs on `main`** (version state commits directly to `main`, no PR
branch) and that the service is a versioned one (has a `<app>/VERSION` file).
You **never need to know the current version number** — `bump` reads it for you.
`--dry-run` prints the next version + the commit message and writes/commits/pushes
**nothing** (answers "what would the next patch be?").

## How `bin/ship` builds the current main

`bin/ship <app>` takes **no version argument**. It always builds **current main
(HEAD)** in a throwaway detached `git worktree`, then reads the version from
**that worktree's** `<app>/VERSION` (the actual build source) and ships it:

| invocation | builds |
|---|---|
| `bin/ship crm` | the current `main` tip (HEAD); the release version is whatever `crm/VERSION` holds on that commit |

The detached-HEAD worktree materializes exactly what is committed on `main`,
**excluding** any uncommitted edits in the operator's working tree — that
guarantee (artifact == committed main) is the whole point of the file-on-main
model. The build then runs **inside the worktree's `<app>/` dir** so `./cmd/<app>`
and the committed `replace … => ../<lib>` directives resolve against the
worktree's sibling library trees — no network, no `go.work` (`GOWORK=off`). Flags:
`CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off go build -trimpath
-buildvcs=false`. The worktree is removed on exit (success or failure).

`bin/ship`'s only box write is the `scp` of the single artifact into `/tmp`. After
the upload it **prints the two box commands** (`sudo opsctl stage <app> v<ver>
--artifact …` then `sudo opsctl deploy <app> v<ver>`) and **stops** — it runs no
stage/deploy over ssh. The box owns every state change beyond `/tmp`.

## The SHA + dirty co-stamp

The version is **co-stamped with the git commit** so the running binary identifies
exactly what built it. Because `-buildvcs=false` is mandatory (the module is a
subdir of a **bare** mono-repo `.git`; Go's auto VCS stamp runs git at the bare
root and aborts with exit 128), Go's automatic `vcs.revision`/`vcs.modified` stamp
is dropped — so `bin/ship` re-injects it via ldflags:

```
-ldflags "-s -w -X appkit.version=v<bare-version> -X appkit.commit=<short-sha>[-dirty]"
```

- `appkit.version` / `appkit.commit` are package-level **`var`s** in
  `appkit/appkit.go` (NOT `const`s — `-X` against a `const` is silently ignored,
  leaving the `dev`/`none` defaults). `appkit.versionString()` renders them as
  `"<version> (<commit>)"`.
- **`-dirty` is clean-by-construction for a normal deploy.** The throwaway
  worktree is a clean detached checkout of `main`'s HEAD, so its tree has no diff
  and the suffix is empty: a deployed artifact always self-reports `vX.Y.Z (<sha>)`.
- **`-dirty` only surfaces if a build is ever driven from a dirty source tree.**
  `bin/ship` re-derives the flag *from the actual build source*
  (`git -C <worktree> status --porcelain`), so the stamp tells the truth about
  exactly what was compiled. A direct ad-hoc `go build` off a dirty tree with
  `-X appkit.commit=$(git rev-parse --short HEAD)-dirty` self-reports
  `vX.Y.Z (<sha>-dirty)`.

So: `<app> version` →
- `v1.4.0 (a1b2c3d)` — a clean build of committed main; deployable.
- `v1.4.0 (a1b2c3d-dirty)` — an ad-hoc build off a dirty tree; do not ship.
- `dev (none)` — un-stamped (a bare `go build` with no ldflags); local dev only.

## The box release-dir naming

`bin/ship` prepends the `v` to the bare file version and prints it in the box
commands; `opsctl stage`/`opsctl deploy` name the on-box release dir accordingly:

```
crm/VERSION = 1.4.0   →   bin/ship crm   →   opsctl stage crm v1.4.0 + opsctl deploy crm v1.4.0   →   /opt/crm/releases/v1.4.0/crm
```

`opsctl` owns the release-dir / atomic-`current`-symlink / migrate / restart /
rollback machinery on the box (see the ADR §5 and `PLAN.md` §1.4). The laptop has
**no install logic** — it only builds and hands off the single static artifact.

## main is protected (the immutable version ledger)

Because the version state lives as commits on `main`, `main` is the ledger — and
it is protected from rewriting by a GitHub ruleset that **blocks force-push and
branch deletion and requires linear history**. That protection is the enforcement
half of "version state lives on main": it is what makes the committed version
history durable and immutable. (`bin/bump` commits directly to `main` and pushes,
so the protected `main` is always the authoritative version state.)

## Cutting a release, end to end

1. **Land the change** on `main` (the deployable branch); make sure the working
   tree is clean (`git status --short` empty) so the build is honestly clean.
2. **Bump the version:** `bin/bump <app> <major|minor|patch>` — it reads
   `<app>/VERSION`, increments, commits **only** that file to `main`, and pushes.
   Use `--dry-run` first if you just want to see the next number. You never type a
   version yourself.
3. **Ship it:** `bin/ship <app>` (no version arg) — it builds current `main`
   (HEAD), `scp`s whatever `<app>/VERSION` holds on that commit to the box `/tmp`,
   then prints the two box commands and stops. Use `--dry-run` (or `DRY_RUN=1`) to
   do the full off-box build and print the `scp`/`opsctl` commands without
   shipping.
4. **Stage + deploy on the box** (the commands `bin/ship` printed): `sudo opsctl
   stage <app> v<ver> --artifact /tmp/<app>-v<ver>` runs preflight (static? amd64?
   `<app> version` matches the version arg? `<app> manifest` parses?) plus the SHA
   collision guard, places the release (not live), and deletes the `/tmp` artifact
   on success; then `sudo opsctl deploy <app> v<ver>` regenerates the manifest,
   backs up the DB if the schema advances, migrates, atomically swaps `current`,
   restarts, and confirms `is-active`. Confirm `<app> version` self-reports `v<the
   number you bumped to>`.
5. **Roll back if needed:** `sudo opsctl rollback <app>` repoints `current` to the
   prior release (restoring the DB first if the rolled-back-from release advanced
   the schema — the forward-only migration runner's downgrade guard requires it).
   The box keeps prior release dirs, so rollback is on-box — you never need to
   rebuild an arbitrary past version off-box.
