# Phase 47 — Blackhole empty-normalization content at the ingest boundary

*Realizes design Decision 28 (blackhole empty-normalization content). Depends on Phase 45.*

In `internal/wiki` (`service.go`), `planIntegration` guards the top of its
per-extracted-item loop: when `Normalize(item.Name) == ""` the item is skipped
(`continue`) before `plannedSubject` is called. The blackholed item produces no
subject row (no `norm_name = ""` ever inserted), its claim bodies are never added
to the plan, and no page is compiled for it. Sibling items with non-empty
normalization integrate normally — subject created, page compiled.

`Normalize` remains the pure function from Phase 45 (still returns `""`); this is
the only place an empty normalization is acted on. No placeholder/sentinel
identity is invented, and the whole job is not failed over one bad item.

**Done when:** R-Z5JL-2IBS (a content-free-name item creates no subject; no
`norm_name = ""` row), R-Z6RH-GA2H (that item's claims are not persisted to any
subject), and R-Z7ZD-U1T6 (a sibling non-empty item in the same extraction is
still created with its page) each have a clearly-named test, and the suite is
green.
