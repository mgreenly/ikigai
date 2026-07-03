# Phase 07 — Root the rebuildable `runs/` tree under the service-owned `cache/` dir

*Realizes design Decision 9. Touches `cmd/scripts/main.go` (the composition-root
runtime-root resolver) and the three tests that pin the old runs location
(`cmd/scripts/main_test.go`, `internal/web/web_test.go`,
`internal/runner/runner_test.go` — the last only by confirmation). No schema, no
nginx, no `internal/web` landing change, no new dependency. Independent of all
other open work (orthogonal to the D1–D8 landing-page series).*

On the D01 on-box layout `scripts` crash-loops to `failed` on boot with
`unlinkat /opt/scripts/runs: permission denied`. `scriptsRuntimeRoot` returns the
**AppDir** (`/opt/scripts`, `root:root 0755`, deliberately not service-writable)
as the parent of the rebuildable `runs/` tree, so `recreateRunsDir`'s boot-time
`os.RemoveAll` + `os.MkdirAll` of the `runs` directory entry needs write on that
root-owned parent and is denied. Runs are scratch we do not promise to preserve;
they belong under the service-owned, wipeable `cache/` tier (opsctl backs up
`state/` only). This phase moves the runs parent to the cache dir in every layout.

In **`cmd/scripts/main.go`**:
- Collapse `scriptsRuntimeRoot` to resolve the runs parent from the **generation
  sidecar's directory** (the cache dir) in all layouts, dropping the
  `IKIGENBA_ROOT`/`SCRIPTS_DB_PATH` branch that returned
  `filepath.Dir(filepath.Dir(dbPath))`. Narrow the signature to a single
  argument:
  ```go
  // scriptsRuntimeRoot returns the parent of the rebuildable runs/ tree. Runs
  // are scratch, not durable state, so they live under the service-owned cache
  // dir (the generation sidecar's parent) in every layout. On the D01 on-box
  // layout that is <root>/scripts/cache (owned by the scripts user), never the
  // root:root AppDir <root>/scripts — so recreateRunsDir can unlink and recreate
  // runs/ on boot without write on the AppDir.
  func scriptsRuntimeRoot(generationPath string) string {
      cacheDir := filepath.Dir(generationPath)
      if cacheDir == "" {
          return "."
      }
      return cacheDir
  }
  ```
- Update the one call site: `rootDir := scriptsRuntimeRoot(cfg.GenerationPath)`.
- Leave `runsDir := filepath.Join(rootDir, "runs")`,
  `runner.New(store, rootDir, runTTL)`, and
  `svc := script.NewService(store, runsDir, run)` **unchanged**.
- If `strings` is now unused in `main.go` (its only use was the removed branch),
  remove the import so `go build`/`go vet` stay clean. The `dbPath` parameter and
  the `getenv` argument disappear with the branch — do not leave them as unused
  parameters.

**Update the tests that pin the old (AppDir-root) location:**
- `cmd/scripts/main_test.go` **`TestScriptsBootsFromOpsctlLayoutAndServesHealth`**
  (id `R-4LKF-FB23`, retained): change `runsDir := filepath.Join(appRoot, "runs")`
  to `runsDir := filepath.Join(cacheDir, "runs")`; keep the "recreated and empty"
  assertions against that path; and **add** an assertion that the AppDir-root
  `filepath.Join(appRoot, "runs")` does **not** exist after boot
  (`os.IsNotExist`). The existing `state/runs` absent assertion stays.
- `internal/web/web_test.go`
  **`TestFreshSetupBootsFromNewScriptsLayoutAndPassesHealth`** (id `R-4LKF-FB23`,
  retained): change `runsDir := filepath.Join(root, "runs")` to
  `filepath.Join(cacheDir, "runs")`; keep the "recreated directory" assertion
  against that path; **add** an assertion that `filepath.Join(root, "runs")` (the
  AppDir root) does **not** exist. The `state/runs` and `backup` absent assertions
  stay.
- `internal/web/web_test.go` **`TestCompositionRootAdoptsNewScriptsLayout`** (id
  `R-4LKF-FB23`, retained): in the required-substrings slice, replace
  `` `rootDir := scriptsRuntimeRoot(cfg.DBPath, cfg.GenerationPath, os.Getenv)` ``
  with `` `rootDir := scriptsRuntimeRoot(cfg.GenerationPath)` ``. The other pinned
  substrings (`runsDir := filepath.Join(rootDir, "runs")`,
  `run := runner.New(store, rootDir, runTTL)`,
  `svc := script.NewService(store, runsDir, run)`, the `config.Resolve` line) stay.
- `internal/runner/runner_test.go`
  **`TestSpawnUsesRebuildableRunsDirOutsideState`** (id `R-4LKF-FB23`): passes a
  `dataDir` straight into `runner.New` and does not call `scriptsRuntimeRoot`, so
  it stays **unchanged** — confirm it still compiles and passes.

**Add the D9 verification coverage:**
- **R-RUNS-CDIR** — a direct unit test on `scriptsRuntimeRoot` in
  `cmd/scripts/main_test.go`: for
  `generationPath = filepath.Join(root, "scripts", "cache", "scripts.db.generation")`,
  `scriptsRuntimeRoot(generationPath)` returns
  `filepath.Join(root, "scripts", "cache")` and **not**
  `filepath.Join(root, "scripts")` (the AppDir root). Tag the test with the id.
- **R-RUNS-BOOT** — satisfied by the two updated boot tests above once they assert
  `cache/runs` present-and-empty and the AppDir-root `runs` absent; tag the
  `main_test.go` boot test with `// R-RUNS-BOOT` alongside its retained
  `R-4LKF-FB23`.

**Done when:** the suite is green (per design *Conventions*: `cd scripts && go
build ./...`, `cd scripts && go vet ./...`, `cd scripts && gofmt -l .` (no
output), `cd scripts && go test ./...`, and `bin/check-migrations scripts` all
succeed with zero failures) and these ids are covered by clearly-named tests:

- **R-RUNS-CDIR** — `scriptsRuntimeRoot(<...>/scripts/cache/scripts.db.generation)`
  returns `<...>/scripts/cache`, not `<...>/scripts`. *(unit test on
  `scriptsRuntimeRoot`)*
- **R-RUNS-BOOT** — booting from the opsctl layout serves `/health` `ok`,
  recreates an empty `<root>/scripts/cache/runs`, and does **not** create
  `<root>/scripts/runs`. *(updated `TestScriptsBootsFromOpsctlLayoutAndServesHealth`
  and `TestFreshSetupBootsFromNewScriptsLayoutAndPassesHealth`)*
</content>
