# Phase 01 — SemVer 2.0 version identity & ordering

*Realizes design Decision 3 (SemVer 2.0 version identity & ordering). Depends on no earlier phase.*

opsctl gains `golang.org/x/mod/semver` (added to `opsctl/go.mod`) as the sole
version authority and the hand-rolled `lessVersion` is removed. The observable end
state in the `opsctl` package: a boundary gate `validVersion` that accepts only a
`v`-prefixed string with all three numeric fields present and rejects bare,
partial, and non-SemVer input; and a `compareVersion` that orders by SemVer §11
precedence (build metadata ignored), with `libexec/` file mtime as the tiebreak
for precedence-equal builds. Every opsctl input boundary that takes a version
routes through the gate and fails loudly on bad input.

**Done when:** `bin/test` exits 0 and these design Verification ids are each
covered by a clearly-named test:
- R-3X6F-RW87 — `v0.7.10` orders after `v0.7.9` (numeric, not lexicographic).
- R-3YEC-5NYW — `v1.0.0-rc.1` < `v1.0.0` and `v1.0.0-rc.2` < `v1.0.0-rc.10`.
- R-40U4-X7GA — `v0.7.1+aaaa` and `v0.7.1+bbbb` compare precedence-equal.
- R-4221-AZ6Z — precedence-equal builds tiebroken by `libexec/` mtime (real fs).
- R-439X-OQXO — **opsctl slice:** the boundary gate rejects a bare (`0.7.1`),
  partial (`v1`, `v1.2`), and non-SemVer string with a non-zero exit (the
  `bin/bump.test.sh` slice of this id is carried by Phase 02).
