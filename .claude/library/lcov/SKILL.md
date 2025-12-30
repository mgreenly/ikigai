---
name: lcov
description: LCOV skill for the ikigai project
---

# LCOV

## Description
Coverage tooling: finding gaps, reading coverage.info, using exclusion markers.

## Finding Coverage Gaps

Run coverage build first:
```bash
make BUILD=coverage check
```

Then find gaps:

```bash
# Uncovered lines (DA = data line)
grep "^DA:" reports/coverage/coverage.info | grep ",0$"

# Uncovered branches (BRDA = branch data)
grep "^BRDA:" reports/coverage/coverage.info | grep ",0$"

# Uncovered functions (FN = function)
grep "^FNDA:0," reports/coverage/coverage.info
```

## Understanding coverage.info Format

```
SF:/path/to/file.c          # Source file
FN:42,function_name         # Function at line 42
FNDA:5,function_name        # Function called 5 times
DA:45,3                     # Line 45 executed 3 times
DA:46,0                     # Line 46 never executed (GAP!)
BRDA:50,0,0,2               # Line 50, block 0, branch 0, taken 2 times
BRDA:50,0,1,0               # Line 50, block 0, branch 1, never taken (GAP!)
end_of_record
```

Key patterns:
- `DA:line,0` = uncovered line
- `BRDA:line,block,branch,0` = uncovered branch
- `FNDA:0,name` = uncovered function

## Coverage Files

### Full Coverage (`make coverage`)

| File | Purpose |
|------|---------|
| `reports/coverage/coverage.info` | Primary data (parse with grep) |
| `reports/coverage/summary.txt` | Human-readable summary |

### Per-Test Coverage (`make coverage TEST=<test_path>`)

Example: `make coverage TEST=unit/agent/agent_test` produces:
- `reports/coverage/unit/agent/agent_test.coverage.info` - LCOV data
- `reports/coverage/unit/agent/agent_test.coverage.txt` - Summary

**Do NOT generate HTML reports** - slow and unnecessary.

## LCOV Exclusion Markers

Use sparingly and only for true invariants:

| Marker | Effect | Use For |
|--------|--------|---------|
| `LCOV_EXCL_LINE` | Exclude line | Rare |
| `LCOV_EXCL_BR_LINE` | Exclude branch | assert(), PANIC() |
| `LCOV_EXCL_START` | Begin block | wrapper.c only |
| `LCOV_EXCL_STOP` | End block | wrapper.c only |

### Correct Usage

```c
assert(ptr != NULL);                    // LCOV_EXCL_BR_LINE
if (!ptr) PANIC("Invariant violated");  // LCOV_EXCL_BR_LINE
```

**Important:** These markers are NOT reliably honored inside `static` functions. See `style.md` "Avoid Static Functions" rule - inline code instead of using static helpers.

### Never Exclude

- Runtime error handling
- Library error returns
- System call failures
- "Should never happen" branches

If it can happen at runtime, test it.

## Quick Workflow

```bash
# 1. Run coverage
make BUILD=coverage check

# 2. Check summary
cat reports/coverage/summary.txt

# 3. Find specific gaps
grep "^DA:" reports/coverage/coverage.info | grep ",0$" | head -20

# 4. Find file with most gaps
grep "^SF:" reports/coverage/coverage.info   # List all files
grep -A 1000 "SF:.*myfile.c" reports/coverage/coverage.info | grep -m 1 -B 1000 "end_of_record" | grep ",0$"
```

## References

- `project/lcov_exclusion_strategy.md` - Full exclusion policy
- LCOV documentation: https://github.com/linux-test-project/lcov
