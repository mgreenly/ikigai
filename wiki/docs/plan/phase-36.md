# Phase 36 — The extract eval harness (`internal/eval`): dataset + runner

*Realizes design Decision 20 (the extract evaluation harness: dataset + runner). Depends on Phase 04 (the `extract` package — `Extractor`, `Extract`, `DocumentHeader`, `ExtractedSubject`) and Phase 35 (the post-D19 call-site world the eval runs in).*

A new reusable package `internal/eval` loads a gold dataset and runs the **real** production extract over each case. It owns no scoring (Phase 37) and no CLI (Phase 38). The observable end state:

- `Case`, `GoldSubject` types (D20 shape): a case carries `Name`, a `Difficulty` from the closed set `{easy,medium,hard}`, an `extract.DocumentHeader` whose `ReceivedAt` comes from the gold file (deterministic, never wall-clock), the verbatim `Text`, and `[]GoldSubject` (each `Type`/`Name`/`Claims`).
- `LoadCase(dir)` parses `<dir>/document.txt` (verbatim source) + `<dir>/gold.json` (stdlib JSON: header + difficulty + subjects), validating at the boundary and failing loudly on a missing/malformed `gold.json`, an empty/missing `document.txt`, a difficulty outside the closed set, a gold subject with a type outside `{entity,event,concept}` / empty name / zero claims, or a malformed `received_at`.
- `LoadDataset(root)` returns one `Case` per immediate subdirectory of `root`, each loaded through `LoadCase`.
- `Run(ctx, ex, c)` calls `ex.Extract(ctx, c.Header, c.Text)` and returns its subjects **unchanged** — no second prompt, no second parser. It takes an already-constructed `*extract.Extractor` so it is unit-testable behind a capturing mock provider.

**Done when:** R-VXAT-MMTX (well-formed case loads with verbatim text, gold `received_at`, parsed difficulty, and all gold subjects/claims), R-VYIQ-0EKM (each boundary violation fails loudly), R-VZQM-E6BB (`LoadDataset` discovers one case per subdirectory), and R-W26F-5PSP (`Run` feeds the real extractor the case header+text verbatim — capturing mock confirms extract's production prompt + the case source and fixed received-on date — returning subjects unchanged) are each covered by clearly-named tests, with `Run` exercised against a mock-backed extractor (no live LLM), and the suite is green.
