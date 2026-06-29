# Phase 03 — Layout path scheme for /opt/<svc>/ (structural)

*Realizes design Decision 1 (the `/opt/<svc>/` install tree) — the path-scheme slice; D1's behavioral ids land in Phase 04. Depends on no earlier phase.*

This is the **structural** slice of D1: opsctl's `Layout` type grows the new
path-scheme methods for the install tree, **additively** — the old methods stay in
place so the build remains green (their last consumer is migrated, and they are
removed, in Phase 05). The observable end state: `Layout` exposes `LibexecDir`,
`LibexecBinary(v)`, `RunLink`, `StateDir`, `DBPath`, `WWWDir`, `WWWPublicDir`,
`WWWPrivateDir`, `CacheDir`, `GenerationPath`, and `BackupsDir`, each returning its
canonical path under a configurable `OPSCTL_ROOT`. No behavior beyond path
construction is added here.

This phase realizes none of D1's Verification ids (they are `setup` behavior, Phase
04); its deterministic check is structural.

**Done when:** `bin/test` exits 0 and a clearly-named opsctl unit test asserts each
new `Layout` method returns its **exact** expected path under a temp `OPSCTL_ROOT`
(e.g. `StateDir()` = `<root>/<svc>/state`, `LibexecBinary("v1.2.3")` =
`<root>/<svc>/libexec/<svc>-v1.2.3`, `GenerationPath()` =
`<root>/<svc>/cache/<svc>.db.generation`). Exact-string assertions; reproducible on
identical repo state.
