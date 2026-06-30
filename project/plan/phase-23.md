# Phase 23 — appkit data-path composition from IKIGENBA_ROOT + fail-loud guard

*Realizes design Decision 11 (env contract) and Decision 5 (state/cache boundary). Depends on Phase 22.*

This phase is **additive** to appkit — it adds the new path-composition seam without
removing any verb yet (verb removal is Phase 31), so the suite stays green throughout.

appkit `config` grows `composeDataPaths(getenv, up, app) (db, gen string)`, mirroring the
existing `composeURLs`: when `IKIGENBA_ROOT` is set it composes
`${ROOT}/<app>/state/<app>.db` and `${ROOT}/<app>/cache/<app>.db.generation`; when unset it
falls back to the `./tmp/<app>.db` dev default; an explicit `<APP>_DB_PATH` /
`<APP>_GENERATION_PATH` overrides the composed value. Startup composition is wired to call it.
A production-misconfiguration guard fails startup when `IKIGENBA_DOMAIN` is set but
`IKIGENBA_ROOT` is unset, rather than silently composing a `./tmp` path.

**Done when:**
- A new appkit `config` unit test, id-tagged `R-8FUU-NRQT`, asserts `composeDataPaths` across all
  three cases: `IKIGENBA_ROOT=/opt` → `/opt/<app>/state/<app>.db` + `/opt/<app>/cache/<app>.db.generation`;
  explicit `<APP>_DB_PATH` overrides; neither set → `./tmp/<app>.db`.
- A test tagged `R-485J-7TWG` asserts the resolved DB path is under `state/` and the generation path
  under `cache/` with the production env applied (the two land in distinct tiers).
- A test tagged `R-8H2R-1JHI` asserts startup returns an error (no silent `./tmp` fallback) when
  `IKIGENBA_DOMAIN` is set and `IKIGENBA_ROOT` is unset.
- `bin/test` exits 0.
