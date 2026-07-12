# Phase 14 — init-box installs the box-baseline PDF tooling (poppler-utils)

*Realizes design Decision 10 (`project/design/D10.md`). Depends on Phase 07 (the
current init-box package-install shape). Unit id `R-WHC0-I9HL` is loop-driven
here; the live-box id `R-WIJW-W18A` is a real-substrate check the fake `System`
cannot falsify — operator-verified out-of-loop (partial-Decision split). Touches
`internal/opsctl/initbox.go` and the init-box/provision tests only; the `System`
seam is reused unchanged.*

Make init-box's step-1 package install carry `poppler-utils` alongside
`nginx`+`certbot`, so every provisioned box has `pdftotext`/`pdftoppm`/`pdfinfo`
as box-global substrate for sandboxed prompts runs (per D10 and the suite's
content-plane design). The observable end state:

- `InitBox` requests **one** package install through the existing
  `System.InstallPackages` seam, now listing `nginx`, `certbot`,
  `poppler-utils` — still as step 1, before the cert/nginx branch, so it runs on
  the `--skip-cert` path too.
- Idempotency is unchanged: the seam's `dnf install -y` contract no-ops on
  already-present packages; init-box remains safe to re-run.
- Existing provision tests that assert the recorded op
  `install-packages:nginx,certbot` are updated to the new three-package op; no
  other init-box behavior changes.

Non-goals: no new seam, verb, flag, or package-list constant; no change to
per-service `setup --packages`; nothing in the prompts service (its sandbox
merely finds the binaries on the box).

**Done when** the suite is green — `GOWORK=off go build ./...` succeeds and
`GOWORK=off go test ./...` passes from `opsctl/` — and this id is covered by a
clearly-named test (temp `OPSCTL_ROOT` + the fake `System`):

- **R-WHC0-I9HL** — after `InitBox` (including on the `--skip-cert` path), the
  fake `System` has recorded `install-packages:nginx,certbot,poppler-utils`. The
  test fails against today's `initbox.go`, which records
  `install-packages:nginx,certbot`.

Operator-verified out-of-loop (not loop-driven): **R-WIJW-W18A** — on
`int.ikigenba.com` after `opsctl init-box`, `command -v pdftotext pdftoppm
pdfinfo` succeeds and `pdftotext -v` exits cleanly, and a rerun of init-box
succeeds with the package already present.
