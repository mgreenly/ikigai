# cleanup-research — status & how to read these reports

This folder holds a one-per-top-level-folder research pass that located
**non-current information** across the suite, ahead of a cleanup. Two migrations
were the priority targets: the **`registry/` service-naming** migration and the
**tar.gz deploy-format** migration. Each `<folder>.md` lists `filename:line`
pointers, migrations flagged high-priority. `plan/` directories were excluded
(old plan phases are allowed to hold non-current info).

> **Read this first.** The per-folder reports are a **point-in-time snapshot**
> from before the registry work landed. Several of them describe the registry as
> "not yet built" / "not yet landed" or treat the deploy format as in-flight.
> **That framing is itself now outdated** — see the status below. Where a report
> and this README disagree about whether registry/deploy is done, **this README
> wins.**

## Status: both priority migrations are DONE

### registry — built, adopted by the first four consumers, and deployed

- **`registry/` is built.** It is a real, standalone, zero-dependency Go module
  at the repo root (`registry/go.mod`, `module registry`) with the authoritative
  `name → port` table (D02) and the resolution API (D03:
  `Port`, `MustPort`, `BaseURL`). Wired into the root `go.work`.
- **Four consumers have adopted it:** `prompts`, `scripts`, `notify`, `sites`.
  Their hardcoded loopback port literals (own port **and** peer feed maps) are
  gone, replaced by `registry.MustPort(...)` / `registry.BaseURL(...) + "/<path>"`
  resolved once at startup. Each carries a committed
  `replace registry => ../registry`, so `GOWORK=off` production builds are clean.
- **Deployed and active on `int.ikigenba.com`:** prompts `v0.14.0`,
  scripts `v0.8.0`, notify `v0.12.0`, sites `v0.9.0` (all `+39fc855`).

**Consequence for the findings:** every high-priority "registry" pointer of the
form `(When the service registry lands, <svc>'s port becomes NNNN; update the
literal below then.)` in the `etc/nginx.conf` files (crm, cron, dropbox, gmail,
notify, prompts, wiki) is now **stale-by-completion**. The registry has landed
and did **not** renumber anyone (it pins current ports), so those notes are not
merely premature, they are wrong: several predict a renumber that never happened
(e.g. prompts predicted 3101 = now ledger's; wiki predicted 3100 = now crm's).
These should be removed/corrected in the cleanup pass.

### deploy — the tar.gz format is current and confirmed live

The `bump → ship → opsctl stage → opsctl deploy` flow (versioned release slots,
three-symlink atomic swap, S3 pre-deploy backup, nginx reload) is the live
mechanism, exercised end to end for the four deploys above. Root `deploy.md` is
the reference. Remaining stale references to the OLD flat-bin model (e.g. the
`Makefile` "production deploy spine (setup/deploy/...)" boilerplate and the
`bin/teardown` flat-`/opt/<app>/{bin,etc,data}` scripts in several services) are
**still stale** and are part of the not-yet-applied cleanup below.

## Applied so far (2026-07-03)

A first cleanup batch has been applied and the resolved findings are marked
`✅ **DONE**` inline in the per-folder reports. What landed:

- **All 7 stale registry parentheticals** removed from the `etc/nginx.conf`
  fragments (crm, cron, dropbox, gmail, notify, prompts, wiki).
- **`notes/` scrubbed:** the `notes/` row dropped from 13 `project/README.md`
  tables, and the dead `notes/PLAN.md` / `ARCHITECTURE.md` pointers repointed to
  `project/design/design.md` (or, in preambles, reduced to the `CLAUDE.md`
  owner) across crm, ledger, dropbox, scripts, and a dropbox nginx.conf comment.
- **Superseded verb sets fixed** (`backup`/`restore` → `schema`) in cron, gmail,
  notify, dropbox, sites, scripts, webhooks, and wiki docs plus
  `docs/positioning-onepage.md`; the wiki doc's wrong `appkit.go:244-260` cite
  corrected to `215-224`.
- **`bin/build` bug fixed** in all five `bin/start` scripts that called the
  deleted wrapper (crm, notify, dropbox, prompts, scripts → `make build`); crm's
  invocation also aligned to the canonical `serve --port` form.
- **`docs/README.md` pointer removed** from root `AGENTS.md`/`CLAUDE.md` (the
  file stays intentionally absent; the "In short:" convention summary was kept).

## What is NOT done

- **Most per-folder findings are still open** — the batch above covered the two
  named migrations' loose ends plus the greenlit `notes/` / verb-set / bin
  fixes. Unmarked bullets (contradictory `Status: scaffold` READMEs, stale
  `internal/server`/`internal/logging` package layouts, `CLAUDE.md` Consumes
  drift, stale Makefile "deploy spine" comments, the `bin/teardown` flat-layout
  scripts, remaining scripts `notes/` refs in `product.md`/`D05.md`/`loops/`/
  `main.go`, etc.) are still valid work.
- **registry adoption is only partial.** Only prompts/scripts/notify/sites
  import registry so far. Other services still hardcode their own port literal;
  broader adoption is separate future work, not stale info to scrub.

## Scope reminder for whoever applies the cleanup

Non-current info is allowed to remain in `**/project/plan/**` (old phases) and in
`docs/archive/**` (intentionally historical). Everything else is fair game.
