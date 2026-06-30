# Phase 26 — opsctl stage: unpack the versioned bundle

*Realizes design Decision 10 (stage) and Decision 2 (per-version placement). Depends on Phase 24.*

`opsctl stage <svc> v<ver> --artifact /tmp/<svc>-v<full>.tar.gz` makes a version's tiers present and
changes nothing live: a D03 version gate refuses non-`v`/partial/invalid before any work; a
SHA-collision guard refuses a *differing* re-stage of an already-present `libexec/<svc>-v<full>` unless
`--force`; on success it unpacks the bundle into `libexec/<svc>-v<full>`, `etc/<v>/nginx.conf`,
`etc/<v>/manifest.env`, `share/<v>/…` (root-owned) and deletes the `/tmp` artifact. No symlink swap,
migrate, or restart.

**Done when:**
- `stage_test.go` tagged `R-84VR-7U2K` asserts: (a) invalid/partial version → non-zero exit, nothing
  unpacked; (b) present version + differing bytes → refused unless `--force`; (c) success unpacks all
  four tiers **and** deletes the `/tmp` artifact.
- A test tagged `R-1BF5-X7QS` asserts that after `stage` of a real `tar` bundle all per-version paths
  exist for that version (`libexec/<svc>-v<full>`, `etc/<v>/{nginx.conf,manifest.env}`, `share/<v>/`).
- Substrate is a real `tar`/filesystem under a temp `OPSCTL_ROOT`; `bin/test` exits 0.
