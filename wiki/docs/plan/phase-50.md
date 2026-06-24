# Phase 50 — Alias-aware path entry for the read lookups (`page` / `claims`)

*Realizes design Decision 29 (alias-aware path entry for the read lookups). Depends
on Phase 42/48 (`AliasStore` keyed on `Normalize`, the D25 `Resolver`), Phase 46
(`SubjectStore.GetByPath` direct, type-checked), and Phase 43/48 (merge mints the
forward-routing alias). Does **not** touch `SubjectStore.GetByPath` (D11) or the
merge path resolvers (D27) — the read/merge asymmetry is deliberate.*

The path-keyed read verbs are alias-blind: `pathPageService.PageByPath`
(`cmd/wiki/main.go`) and `pathClaimService.ListBySubject` (`cmd/wiki/main.go`)
resolve their `type/slug` input with `SubjectStore.GetByPath`, which is
subjects-only. So after a merge folds `L` into `W`, `page` / `claims` on `L`'s old
path return not-found / empty instead of forwarding to `W` — even though the alias
`Normalize(L) → W` exists. This phase adds the alias fallback at the read entry,
mirroring what `Resolver.ResolveByName` already does for the name-keyed surfaces,
so a folded-away path serves the survivor's canonical page and claims.

**End state.**

- `internal/wiki/resolver.go` — new `Resolver.ResolveByPath(ctx, path) (Subject,
  error)`: try `SubjectStore.GetByPath(path)` (D11 semantics unchanged — exact
  token, exact type); on `ErrSubjectNotFound`, split off the token (the part after
  `"/"`), `AliasStore.GetByNormName(token)`, and on a hit load the survivor via
  `SubjectStore.Get(subject_id)`. Neither a live subject nor an alias → return
  `ErrSubjectNotFound`. Single-hop (the D25 FK + D26 eager repoint guarantee the
  survivor is never itself an alias — no chain/cycle logic). The alias branch is
  token-only (the `aliases` table stores no type) and runs **only after**
  `GetByPath` misses, so it never resolves a wrong-type path to a live subject.
- `cmd/wiki/main.go` — `pathPageService` and `pathClaimService` gain a
  `*wiki.Resolver` (built over the read pool) and call `resolver.ResolveByPath`
  in place of `subjects.GetByPath`; the existing `ErrSubjectNotFound → sql.ErrNoRows`
  mapping, the `PageWithLinks`/`RenderFooter` page render, and the claims listing
  are otherwise unchanged. The forwarded result is **byte-identical** to a direct
  lookup of the survivor's path — the served fields derive from the resolved
  subject — and the `page` result keeps exactly `{subject, title, body}` (no
  redirect marker; D10 R-03GW-PX5K preserved). The merge resolvers
  (`mergePathResolver`, `specMergePathResolver`) are **not** changed — they stay
  subjects-only per D27.

**Done when:** the suite is green (per design *Conventions*) and these design
Verification ids are covered by clearly-named tests — exercised on the `Resolver`
seam against a real temp SQLite (`foreign_keys=ON`), with alias rows installed
directly via `AliasStore` (no merge needed):

- **R-AF1X-PG7K** (D29, real SQLite) — a path whose token matches no live subject
  but equals an `aliases.norm_name` resolves to the survivor via a single
  `SubjectStore.Get` (single hop).
- **R-AG2Y-PH8L** (D29, real SQLite) — a path matching a live subject by exact token
  and type resolves directly (alias table not consulted); a right-token/wrong-type
  path with no alias returns `ErrSubjectNotFound` (D11 type discipline preserved —
  the fallback never resolves a wrong-type path to a live subject).
- **R-AH3Z-PJ9M** (D29, real SQLite) — a path whose token is neither a live subject
  nor an alias, and a malformed/empty-token path (`entity/`), both return
  `ErrSubjectNotFound`, so `page`/`claims` still report a clean not-found on a
  genuinely unknown path.
- **R-AL5R-PL1P** (D29, real SQLite) — forwarding is invisible and symmetric:
  `ResolveByPath(<folded path>)` and `ResolveByPath(<survivor path>)` return the same
  survivor, so `PageByPath` renders the byte-identical `{subject: Path(W), title,
  body}` and `ListBySubject` lists `W`'s claims for the folded path exactly as for
  `W`'s own path — no redirect marker, no field beyond the `{subject, title, body}`
  shape.

The read/merge asymmetry needs no new test here: it is preserved by construction
(`GetByPath` is untouched) and already guarded green by D27 R-E01B-X6IA (a `merge`
input matching no subject → not-found) and D11 R-DT53-3OJL (subjects-only,
type-checked resolution).
