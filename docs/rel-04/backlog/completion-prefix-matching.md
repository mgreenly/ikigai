# Completion Prefix Matching

## Problem

Command completion shows commands that don't start with the typed prefix.

**Input:** `/m`

**Expected completions:**
```
  mark   Create a checkpoint for rollback (usage: /mark [label])
  model   Switch LLM model (usage: /model <name>)
```

**Actual completions:**
```
  mark   Create a checkpoint for rollback (usage: /mark [label])
  model   Switch LLM model (usage: /model <name>)
  system   Set system message (usage: /system <text>)
```

"system" incorrectly appears because it contains 'm' (at position 5: syste**m**).

## Root Cause

The fzy library performs **subsequence matching**, not **prefix matching**.

The `has_match()` function in `vendor/fzy/match.c` checks if all characters of the search string appear anywhere in the candidate, in order but not necessarily consecutive or at the start:

```
has_match("m", "system") → TRUE (finds 'm' at position 5)
```

This is correct behavior for fuzzy file finding (fzy's original purpose), but wrong for command completion where users expect prefix matching.

## Why This Matters

- Users expect `/m` to show only commands starting with 'm'
- Showing unrelated commands is confusing
- Standard shell completion behavior is prefix-based

## Proposed Fix

Add a prefix check before the fuzzy match in the completion filtering logic. The fzy wrapper should enforce that candidates start with the search string before considering them as matches.

Two options:

**Option A:** Add prefix check in `ik_fzy_filter()` before calling `has_match()`:
- Check `strncasecmp(search, candidate, strlen(search)) == 0`
- Only then use fzy for scoring/ranking

**Option B:** Add a `prefix_required` parameter to `ik_fzy_filter()`:
- When true, enforce prefix matching
- When false, use pure fuzzy matching (for future file completion, etc.)

Option A is simpler if command completion is the only use case. Option B is more flexible.

## Scope

- Primary change: `src/fzy_wrapper.c` in `ik_fzy_filter()`
- Do NOT modify vendored fzy code (`src/vendor/fzy/`)
- Update tests for completion filtering behavior

## Related

- Completion creation: `src/completion.c` (`ik_completion_create_for_commands()`)
- fzy wrapper: `src/fzy_wrapper.c`
- Vendored fzy: `src/vendor/fzy/match.c` (do not modify)
