# Phase 3 — opsctl loads the box env file at startup

*Realizes design Decision 3 (opsctl loads the box env file at startup), ids
`R-6AIE-QTDC`, `R-6BQB-4L41`, `R-6CY7-ICUQ` — a partial-Decision split: D3's
fourth id `R-6FE0-9WC4` is a real-substrate (live-box) check verified by the
operator out-of-loop, not in this phase. Depends on no earlier phase.*

## What gets built

Package `internal/opsctl` (the `LoadEnvFile` loader) plus its wiring in
`cmd/opsctl/main.go`.

opsctl is run interactively as `sudo opsctl <verb>`, not by systemd, so nothing
loads `/etc/ikigenba/env` into its process; interactive `deploy`/`backup` then
find `IKIGENBA_BACKUP_BUCKET` empty (read via `os.Getenv` in
`objectStore()`, `backup.go`) and fail their S3 step.

Add `LoadEnvFile(path string) error` in `internal/opsctl`: it parses a
systemd-style env file — `KEY=VALUE` per line, `#`-comment and blank lines
ignored — and sets each KEY via `os.Setenv`, but **only if that KEY is not already
present** in the environment (non-override / fallback semantics, so the systemd
unit's own `EnvironmentFile` and an operator's explicit inline override both still
win). A **missing** file returns `nil` (off-box/dev has no `/etc/ikigenba/env`); a
**malformed** line (no `=`) returns an error. Wire `main()` to call
`LoadEnvFile("/etc/ikigenba/env")` before it reads `OPSCTL_ROOT`/dispatches,
exiting non-zero if it returns an error. The env consumers (`objectStore()` etc.)
are unchanged — they keep reading `os.Getenv`, and their existing
"IKIGENBA_BACKUP_BUCKET is required" errors remain the loud failure when the value
is genuinely absent.

Observable end state: after `LoadEnvFile` runs over a systemd-style file, the
file's variables are visible via `os.Getenv` except where already set; a missing
file is a no-op; opsctl's verbs see the box env on the interactive path.

## Done when

All of the following hold on identical repo state, from the service root
(`opsctl/`):

- `GOWORK=off go build ./...` exits 0.
- `GOWORK=off go test ./...` exits 0 (suite green).
- The three ids are covered by named tests:
  `grep -rE 'R-6AIE-QTDC|R-6BQB-4L41|R-6CY7-ICUQ' internal/ --include='*_test.go'`
  returns ≥ 3 matching lines, and those tests assert:
  - `R-6AIE-QTDC` — after `LoadEnvFile` over a file with `KEY=VALUE`, `#` comment,
    and blank lines, `os.Getenv` returns each defined value and no variable for
    the commented/blank lines.
  - `R-6BQB-4L41` — a KEY pre-set in the environment is unchanged after
    `LoadEnvFile` assigns it a different value (non-override).
  - `R-6CY7-ICUQ` — `LoadEnvFile` on a non-existent path returns `nil` and sets
    nothing.
