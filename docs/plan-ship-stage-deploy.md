# Plan — ship → stage → deploy refactor + new `opsctl` verbs

Status: **design locked, not yet implemented.** This file is the working plan;
the authoritative deploy model lives in `docs/adr-deployment-redesign.md` and
`docs/versioning.md` (both get updated as part of step H).

## Goal in one paragraph

Split today's monolithic on-box `opsctl install` into two single-purpose verbs —
`stage` (place a release, not live) and `deploy` (activate it) — and split the
laptop side so `bin/deploy` becomes `bin/ship`, which **only ships a build to the
box `/tmp` and stops**. The box owns every state change. Add the operator-facing
read/control verbs the runbooks have been faking by hand (`status`, `releases`,
`tail`, and `start/stop/restart/enable/disable` passthroughs), and give `opsctl`
a real grouped `--help`.

## Locked decisions

1. **`bin/ship` does NOT run `opsctl stage`.** Its only box write is the `scp`
   into `/tmp`. It prints the next box commands (`stage` then `deploy`) and stops.
   The laptop never changes box state beyond `/tmp`.
2. **`opsctl stage` deletes the `/tmp` artifact after it has copied it** into
   `releases/<ver>/<app>`. Delete on success (freshly placed *or* idempotent
   same-SHA no-op — the release is confirmed in place either way). **Leave the
   `/tmp` file on refusal/error** (a different-SHA collision without `--force`, a
   failed preflight) so the operator can retry — e.g. with `--force` — without a
   re-`scp`.
3. `deploy <app> <ver>` takes a **required** version (no "newest staged" default
   — explicit over implicit).
4. `opsctl` itself is **not versioned** (no `bin/bump`, no `<app>/VERSION`); it
   ships as a hand-built binary `install -m 0755`'d to `/usr/local/bin/opsctl`.
5. The old `install` verb is **removed cleanly** — no deprecated alias (greenfield,
   pre-prod, and our "no backwards-compat hacks" principle).

## The shape after this change

**laptop `bin/ship`** (rename of `bin/deploy`): build current `main` HEAD in a
throwaway worktree → read `<app>/VERSION` from that worktree → **early existence
check** (`ssh test -e /opt/<app>/releases/v<ver>`, refuse unless `--force`) →
`scp` artifact to `/tmp/<app>-v<ver>` → **stop**, printing the two box commands.
The existence check runs *after* the (cheap) worktree add + VERSION read but
*before* the (expensive) compile, so a forgotten `bin/bump` fails before the build.

**`opsctl` verbs (on box, via `sudo`):**

| verb | does |
|---|---|
| `stage <app> <ver> --artifact <path> [--force]` | preflight → **SHA collision guard** → place into `releases/<ver>/` → **delete the `/tmp` artifact on success**. NOT live. |
| `deploy <app> <ver>` | **guard: release was staged** → regenerate manifest → backup-if-schema-advances → migrate → chown → swap `current` → restart → is-active → prune. `<ver>` REQUIRED. |
| `rollback <app> [ver]` | unchanged |
| `prune <app> [--keep N]` | unchanged |
| `status [app]` | one app or all: `app · version · sha · active`. All = scan `OPSCTL_ROOT/*` for dirs with a `current` symlink. |
| `releases <app>` | list `releases/<ver>/` ascending, mark current + predecessor |
| `tail <app> [args…]` | `journalctl -u <app> -f [args…]`; streams (inherits stdfds) |
| `start`/`stop`/`restart`/`enable`/`disable <app> [args…]` | `systemctl <verb> <app> [args…]` passthrough |
| `setup` / `init-box` | unchanged |

**Key invariant kept:** the stable `/opt/<app>/etc/manifest.env` changes only at
`deploy` (with the swap), NEVER at `stage` — else the dashboard would advertise
the new version while the old binary is still live. `migrate` likewise stays on
the deploy side (bound to the cutover).

## Collision guard (two layers)

- **`stage` is authoritative.** It runs the app's `version` verb on **both** the
  incoming artifact and any already-placed `releases/<ver>/<app>`, and compares
  the **commit SHA** (the `(<sha>[-dirty])` field, not the version token):
  - same SHA → idempotent no-op (skip the copy); still delete `/tmp` (decision 2).
  - different SHA → refuse unless `--force`; keep `/tmp`.
  - existing release binary won't exec (corrupt) → treat as a collision, refuse
    unless `--force`.
  - note: two `-dirty` builds of the same version collide on an identical stamp
    despite differing content — `--force` is the escape hatch.
- **`bin/ship`** does only the cheap early `test -e releases/v<ver>` existence
  check (it cannot know the SHA before the build).

## Execution model — serialized subagents

**This plan is executed by a sequence of subagents, one step at a time, with NO
parallel work.** Each step below is a self-contained brief: a fresh subagent that
has read only this file (plus the files its step names) can complete it. The steps
are ordered so that:

- every dependency a step needs already exists from an earlier step, and
- **the `opsctl` module builds, vets, and tests green at the end of every step**
  (`cd opsctl && GOWORK=off go build ./... && GOWORK=off go vet ./... && GOWORK=off
  go test ./...`). No step is allowed to leave the tree broken for "a later step
  to fix" — that is why verb implementations and their dispatcher wiring + tests
  are grouped into the same step rather than split across A/D/G as an earlier draft
  did.

Run the steps strictly in the numbered order. Each step ends with an explicit
**Done when** gate; do not start the next step until it passes.

**Shared context every step depends on (read before doing any step):**
- This file's *Goal*, *Locked decisions*, *The shape after this change*,
  *Collision guard*, and the schema/migration invariant.
- `docs/adr-deployment-redesign.md` — the authoritative deploy model.
- The seam/layout idioms in `opsctl/internal/opsctl/{opsctl,layout,seam}.go`.

## Implementation steps

### Step 1 — Seam additions + fakeapp `FAKE_COMMIT` (foundation)
*Depends on: nothing. Purely additive — nothing calls the new code yet.*

- Read first: `opsctl/internal/opsctl/seam.go`, `opsctl/internal/opsctl/helpers_test.go`,
  `opsctl/internal/opsctl/testdata/fakeapp/main.go`.
- Do, in `seam.go`:
  - Add `IsActiveState(ctx, app) (string, error)` to the `System` interface and
    `RealSystem`: capture `systemctl is-active` stdout and return the state string
    **regardless of exit code** (it exits non-zero for inactive/failed); error
    only on a genuine exec failure (binary missing, context cancelled).
  - Reimplement the existing `IsActive` (the deploy gate) on top of `IsActiveState`
    so behavior is unchanged for callers (nil iff state == "active").
  - Add `Systemctl(ctx, args...) error` and `Journalctl(ctx, args...) error` to the
    interface and `RealSystem`. `Systemctl` reuses the `run()` helper. `Journalctl`
    **streams** (sets `cmd.Stdin/Stdout/Stderr` to the process's own) so `tail`
    follows live; it does not capture output.
- Do, in `helpers_test.go` (`stubSystem`): implement the three new methods —
  `IsActiveState` returns a settable state (default "active"), `Systemctl`/
  `Journalctl` **record argv** via `s.record(...)` (e.g. `"systemctl:restart crm"`)
  so later steps can assert argv with no real systemd.
- Do, in `testdata/fakeapp/main.go`: add a `FAKE_COMMIT` env knob; when set, the
  `version` verb prints `"<FAKE_VERSION> (<FAKE_COMMIT>)"` instead of bare
  `<FAKE_VERSION>` (mirrors the real `<ver> (<sha>[-dirty])` self-report). When
  unset, behavior is unchanged so existing tests still pass.
- Done when: `go build/vet/test ./...` green; no behavior change for existing
  callers (the suite still passes untouched).

### Step 2 — Split `Install` into `stage` + `deploy` (core verbs)
*Depends on: Step 1 (`FAKE_COMMIT` for collision tests). This step removes
`Install`, so it MUST also rewire the dispatcher and the install tests in the same
step to stay green.*

- Read first: `opsctl/internal/opsctl/install.go`, `preflight.go`, `opsctl.go`,
  `layout.go`, `cmd/opsctl/main.go`, `install_test.go`.
- Do, in `preflight.go`: add `commitToken(out string) string` next to
  `versionToken`, extracting the `(<sha>[-dirty])` field (returns "" if absent).
  `versionToken` strips it, so the collision guard cannot reuse it.
- Do, in `install.go` (rename file to `deploy.go` is optional; keep helpers like
  `stampDataPaths`/`copyExecutable`/`writeFileAtomic`/`schemaAdvances` where they
  are or move with the code — keep one home):
  - `Stage(ctx, app, version, artifact string, force bool) error` = today's steps
    1–2 (preflight → place into `releases/<ver>/`) plus:
    - **Collision guard** (see *Collision guard* section): if `releases/<ver>/<app>`
      exists, run its `version` verb and the incoming artifact's `version` verb,
      compare `commitToken`s. Same SHA → idempotent no-op (skip the copy).
      Different SHA, or existing binary won't exec → refuse unless `force`.
    - **On success** (placed OR idempotent no-op): `os.Remove(artifact)` to delete
      the `/tmp` file (decision 2). On refusal/error: leave it.
  - `Deploy(ctx, app, version string) error` = today's steps 3–8 (manifest regen →
    schemaAdvances/backup → migrate → chown → ensure `bin/run` → atomic swap →
    restart → is-active → prune), preceded by a **"was it staged?" guard**: refuse
    early if `releases/<ver>/<app>` is absent ("stage it first") so the
    manifest/schema/migrate execs never fail opaquely.
  - Delete `Install`.
- Do, in `cmd/opsctl/main.go`: replace the `install` case with `stage` and
  `deploy`. `stage`: `--artifact` (value flag, required) + `--force` (bool), both
  through `reorderArgs(args, {"artifact": true})`; positionals `<app> <version>`.
  `deploy`: positionals `<app> <version>` (both required, no default). Update the
  `usage` const minimally for now (Step 6 rewrites it fully).
- Do, in tests: rename `install_test.go` → `deploy_test.go`; add a `stageAndDeploy`
  test helper (`o.Stage(...); o.Deploy(...)`) and convert the existing acceptance
  tests (install→install→rollback, schema-advance, legacy bin/run, chown, is-active
  failure, preflight rejections) to use it. Add a new `stage_test.go` covering:
  same-SHA no-op (no re-copy, `/tmp` deleted), different-SHA refuse (`/tmp` kept),
  `--force` override, and the deploy "not staged" guard.
- Done when: `go build/vet/test ./...` green; `opsctl install` is gone and
  `opsctl stage`/`opsctl deploy` work end-to-end in tests.

### Step 3 — `status` + `releases` verbs
*Depends on: Steps 1 (`IsActiveState`) and 2 (`commitToken`, dispatcher shape).*

- Read first: `opsctl/internal/opsctl/{rollback,prune,layout,opsctl}.go`,
  `cmd/opsctl/main.go`.
- Do: add `status.go` and `releases.go`.
  - App-discovery helper (in `layout.go` or `opsctl.go`): list `OPSCTL_ROOT/*`,
    keep dirs that have a `current` symlink.
  - `Status(ctx, app string)`: version = `currentVersion` (readlink basename); sha =
    run `<current>/<app> version` → `commitToken`; active = `IsActiveState`. Empty
    app → iterate the discovery helper. Format a simple aligned table (`app ·
    version · sha · active`); exact column layout is left to judgment.
  - `Releases(ctx, app string)`: reuse `listReleases` (ascending) + `currentVersion`
    + `priorRelease`; print each release, marking current and its predecessor.
- Do, in `cmd/opsctl/main.go`: add `status` (0–1 positional) and `releases`
  (1 positional) cases.
- Do, in tests: `status_test.go` (single app + all-apps discovery, using the
  stub's settable state) and `releases_test.go` (current/predecessor marks) over a
  temp `OPSCTL_ROOT`.
- Done when: `go build/vet/test ./...` green.

### Step 4 — `tail` + `start/stop/restart/enable/disable` passthroughs
*Depends on: Steps 1 (`Systemctl`/`Journalctl` seam) and 2/3 (dispatcher shape).*

- Read first: `opsctl/internal/opsctl/seam.go`, `cmd/opsctl/main.go`,
  `helpers_test.go`.
- Do: add `ops.go`.
  - `start/stop/restart/enable/disable`: thin wrappers calling
    `System.Systemctl(ctx, <verb>, app, extraArgs...)`.
  - `tail`: call `System.Journalctl(ctx, "-u", app, <args or default -f>)`.
    **Suppress the default `-f`** only if the passed args already carry a
    follow/range flag — the set is `-f`/`-n`/`--since`/`--no-tail` (recommendation
    adopted).
- Do, in `cmd/opsctl/main.go`: add the six passthrough verb cases. **Passthrough
  verbs bypass `reorderArgs`**: `app = args[0]`, forward `args[1:]` verbatim (so
  `opsctl tail crm -n 100 --since 1h` reaches journalctl unscrambled). Structured
  verbs (stage/setup/prune/init-box) keep `reorderArgs`.
- Do, in tests: `ops_test.go` asserting recorded argv for each passthrough and for
  `tail` (default `-f` present; suppressed when `-n`/`-f`/`--since`/`--no-tail`
  given).
- Done when: `go build/vet/test ./...` green.

### Step 5 — `opsctl --help` (grouped) + per-verb help + coverage test
*Depends on: Steps 2–4 (all verbs must exist so help lists them all).*

- Read first: `cmd/opsctl/main.go`.
- Do:
  1. Rewrite the top-level `usage` into grouped sections, one example each:
     *Deploy lifecycle* (`stage`/`deploy`/`rollback`/`prune`), *Inspect*
     (`status`/`releases`), *Service control*
     (`start`/`stop`/`restart`/`enable`/`disable`/`tail` — note extra args forward
     to systemctl/journalctl), *Provisioning* (`setup`/`init-box`), and the `env`
     section (`OPSCTL_ROOT`, `OPSCTL_SYSROOT`).
  2. Per-verb help: `opsctl <verb> --help` prints that verb's synopsis + flags
     (give each `flag.FlagSet` a one-line `Usage` synopsis).
  3. Help-coverage test (`main_test.go` or similar): assert the usage text names
     every dispatched verb — guards against adding a verb to the switch but not to
     help. Drive it off a single source-of-truth verb list if practical.
- Done when: `go build/vet/test ./...` green; `opsctl --help` and
  `opsctl <verb> --help` read well by hand.

### Step 6 — `git mv bin/deploy bin/ship` (laptop side)
*Depends on: the verbs being final (the commands ship prints). No Go build impact.*

- Read first: `bin/deploy`, `bin/bump`, `bin/start`.
- Do:
  - `git mv bin/deploy bin/ship`; rewrite per the locked shape: worktree add →
    read `<app>/VERSION` → early `ssh test -e /opt/<app>/releases/v<ver>` existence
    check (refuse unless `--force`) → compile → `scp` to `/tmp/<app>-v<ver>` →
    **stop**, printing the two box commands (`sudo opsctl stage …` then `sudo
    opsctl deploy …`). Remove the `ssh sudo opsctl install` call.
  - The `SHIPPING …` echo becomes `SHIPPED -> /tmp` (or similar).
  - Update `bin/bump`'s "next step" hint (`bin/deploy` → `bin/ship`) and the
    `bin/start` comment references.
  - (The existence check needs `<ver>`, so it runs after the cheap worktree add +
    VERSION read but before the expensive compile. If `ec2-user` cannot
    `test -e /opt/<app>/releases/...` at 0755, fall back to `sudo test -e`.)
- Done when: `bin/ship --help` and `bin/ship <app> --dry-run` behave; `shellcheck`
  (if available) clean.

### Step 7 — Docs sweep
*Depends on: all behavior final (Steps 1–6).*

- Update forward/authoritative docs to ship→stage→deploy + the new verbs:
  `docs/adr-deployment-redesign.md`, `docs/versioning.md`, `AGENTS.md`,
  `README.md`, root `CLAUDE.md`, per-service `CLAUDE.md`/`AGENTS.md`, **and
  `docs/runbook-dashboard-box-cutover.md`** (treated as a forward operator
  deliverable — recommendation adopted).
- Update active script echoes naming `bin/deploy`: `dashboard/bin/secrets`,
  `notify/bin/secrets`, the `crm/bin/start` + `ledger/bin/start` comments, and the
  `appkit/appkit.go:238` comment.
- **Leave historical runbook logs** as past-tense records
  (`docs/runbook-d2-ledger-box-prototype.md` — which already flagged "No `opsctl
  status` verb … candidate for a follow-up," now closed).
- Done when: `grep -rn "opsctl install\|bin/deploy"` over forward docs + active
  scripts returns only intentional historical references.

### Step 8 — Commit
*Depends on: Steps 1–7.*

- One commit to `main` + push. opsctl is NOT versioned — no `bin/bump`. (Branch
  first if not already isolated, per repo norms; this lands on `main` per the
  versioning model for non-versioned tooling — confirm with the operator before
  pushing.)
- Done when: working tree clean, pushed.

### Step 9 — Ship the new `opsctl` to the box + verify
*Depends on: Step 8 (build from committed state for reproducibility).*

- Rebuild linux/amd64 opsctl → `scp` → `sudo install -m 0755 /usr/local/bin/opsctl`
  on `int.ikigenba.com`. `/opt/*` release dirs untouched.
- **Ordering constraint:** the box's `opsctl` must be replaced **before** the new
  `bin/ship` is used against the box — the old box binary only knows `install` and
  will reject the `stage`/`deploy` commands ship prints. (Saved `opsctl install`
  muscle-memory breaks; that is intended.)
- Verify on the box: `opsctl --help` lists the new verbs; `opsctl status` reports
  the apps; a real `bin/ship <app>` → `opsctl stage` (artifact removed from `/tmp`)
  → `opsctl deploy` round-trip on one service.
- Done when: the round-trip succeeds and `opsctl status` shows the new release
  active. (Box verification may surface fixes that loop back to earlier steps.)

## Decided (formerly open)
- **Dashboard cutover runbook:** treated as a **forward** doc and updated (Step 7).
- **`tail` default-`-f` suppression set:** `-f`/`-n`/`--since`/`--no-tail` (Step 4).
- **`status` table layout:** left to implementer judgment (Step 3).
- **`ec2-user` `test -e` without sudo:** assume yes at 0755; fall back to
  `sudo test -e` (Step 6).

## Key files
- opsctl core: `opsctl/internal/opsctl/{install,rollback,prune,preflight,layout,
  seam,manifest,opsctl,setup,initbox,version}.go` (+ new `status.go`,
  `releases.go`, `ops.go`)
- dispatcher: `opsctl/cmd/opsctl/main.go`
- tests: `opsctl/internal/opsctl/{install,prune,provision,helpers}_test.go`
  (+ new `stage_test.go`, `status_test.go`, `releases_test.go`, `ops_test.go`),
  `opsctl/internal/opsctl/testdata/fakeapp/main.go`
- laptop: `bin/{deploy,bump,start}`
