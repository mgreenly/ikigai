# Phase 02 — bump/ship emit v-prefixed SemVer + convert VERSION files

*Realizes design Decision 4 (Version production: bump/ship emit v-prefixed SemVer). Depends on Phase 01 (the v-everywhere / SemVer 2.0 contract it must produce).*

The version *producers* are brought in line with the `v`-everywhere contract.
`bin/bump` reads `<svc>/VERSION`, increments the requested field carrying the `v`,
writes back a `v`-prefixed result, and rejects a `VERSION` file that is not valid
`v`-prefixed SemVer. `bin/ship` builds `main` static for `linux/amd64`
(`GOWORK=off`) and stamps the artifact with the full version — the `VERSION` core
plus a `+<SHA>` build-metadata suffix from the built commit — so the binary's
`version` output and the produced `libexec/<svc>-v<full>` name agree by
construction. As part of adopting the new contract, the twelve committed
`VERSION` files (currently bare, e.g. `0.6.0`) are converted to their
`v`-prefixed form so the new `bump` accepts them.

**Done when:** `bin/test` exits 0 and:
- R-44HU-2IOD — `bump patch` on `v0.7.1` writes `v0.7.2`; `bump` against a bare
  `0.7.1` fails with non-zero exit (covered in `bin/bump.test.sh`).
- R-45PQ-GAF2 — `ship` stamps the artifact so the built binary's `version` output
  and the staged `libexec/` filename both equal `v<core>+<sha>` for the built
  commit (covered in `bin/ship.test.sh` + a real `go build` smoke).
- R-439X-OQXO — **shell slice:** `bin/bump.test.sh` proves `bump` refuses a bare /
  partial / non-SemVer `VERSION` with a non-zero exit (the opsctl slice is in
  Phase 01).
- Every committed `<svc>/VERSION` matches `validVersion` (exact match count: all
  twelve are `v`-prefixed valid SemVer) — verified by `bin/bump.test.sh` or a
  dedicated `bin/*.test.sh` check, not a prose claim.
