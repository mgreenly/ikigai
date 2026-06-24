# Phase 45 — Rewrite Normalize to the [a-z0-9-] transform

*Realizes design Decision 3 (the data model — the `Normalize` function). Depends on no earlier phase.*

In `internal/wiki` (`data_model.go`), `Normalize` produces the canonical
`NormName`: NFKC → lowercase → strip diacritics → map every rune not in
`[a-z0-9]` to `-` → collapse runs of `-` to one → trim leading/trailing `-`. The
exported `Normalize` is the single normalization entry point; the diacritic
folding stays (it precedes the charset map, so `Salaì` → `salai`, not `sala`).
The result contains only `[a-z0-9-]` with no leading/trailing/repeated `-`, is
idempotent, and is `""` for input with no alphanumeric content.

Existing callers (`plannedSubject`, `links`, `eval`, `List` search-term
normalization, alias storage) continue to call `Normalize`/`normalize` unchanged;
their stored/compared values simply take the new form. Existing tests that assert
the old spacey/punctuated `NormName` are updated to the new form as part of this
phase.

No path or `slug` changes here — both `slug()` copies still exist and, because
they now operate on a `[a-z0-9-]` `NormName`, return it unchanged (the symptom is
already gone); the deletions land in Phase 46.

**Done when:** R-RU0J-77HX, R-RV8F-KZ8M, R-RXO8-CIQ0, R-RYW4-QAGP, R-S041-427E,
R-S1BX-HTY3, R-S2JT-VLOS each have a clearly-named test asserting the behavior in
D3's Verification list (charset/comma-stripping, diacritic-before-charset,
single-hyphen collapse, leading/trailing trim, digit preservation, idempotence,
empty-on-content-free), and the suite is green.
