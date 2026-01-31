## Objective

Remove the dead code function `{{function}}` from `{{file}}:{{line}}` - but only if you can PROVE it's truly dead.

## Understanding the Problem

### Why This Is Hard

Static analysis (cflow) says `{{function}}` isn't reachable from `main()`. But cflow only traces direct function calls. This codebase uses:

- **Vtables**: `provider->start_stream = anthropic_start_stream`
- **Callbacks**: `talloc_set_destructor(ctx, my_destructor)`
- **Function pointers**: `handler_func = get_handler(type)`

These patterns mean a function can have ZERO direct callers but still execute at runtime.

### The Core Insight

**For a function to be called via pointer, its name must appear somewhere in `src/` in a non-call context.**

Think about it: to store a function pointer, you write something like:
```c
vtable->method = function_name;     // assignment
.callback = function_name,          // initializer
register(function_name);            // passed as arg
```

In all cases, `function_name` appears WITHOUT parentheses (no `function_name(`). If you grep for the function name and find it ONLY in contexts with `(` after it, those are all calls - and if cflow says there are no calls from main, then those must be test-only calls.

### Tests Don't Count

Tests exist to verify production code works. A test calling a function doesn't make that function "used" - it just means someone wrote a test for it. If the function is dead in production, the test is dead too.

**The question is always: does PRODUCTION code (`src/`) need this function?**

## Evaluation Framework

### Level 1: Static Analysis (fastest)

Search for the function name in `src/` in non-call contexts:

```bash
grep -rn '\b{{function}}\b' src/ | grep -v '{{function}}\s*('
```

**Interpreting results:**
- Only definition/declaration lines → No pointer usage, continue evaluation
- Found in assignment/initializer → Used via pointer, NOT dead
- Found as function argument → Likely callback registration, NOT dead

**Example interpretations:**
```
src/foo.c:10: void {{function}}(int x) {     → Definition, ignore
src/foo.h:5:  void {{function}}(int x);      → Declaration, ignore
src/bar.c:20: vtable->op = {{function}};     → POINTER USAGE - not dead!
src/baz.c:30: {{function}}(42);              → Call (has parens), covered by cflow
```

### Level 2: Build Test (medium cost)

Comment out the function and build production:

```c
#if 0  // testing if dead: {{function}}
<function body>
#endif
```

Also comment out header declaration if present.

Run `make bin/ikigai` (NOT tests).

**Interpreting results:**
- Build fails → Direct dependency exists that cflow missed, NOT dead
- Build succeeds → No direct compile-time dependency, continue

### Level 3: Test Execution (highest cost, most definitive)

Run `make check` with function still commented out.

**Interpreting results:**

**All tests pass** → Function is truly dead. No production code path exercises it.

**Some tests fail** → Analyze each failure:

For each failing test, ask: "Does this test file directly reference `{{function}}`?"

```bash
grep -w '{{function}}' tests/unit/test_foo.c
```

**If YES (test references function):**
The test is testing the dead function directly. Delete the test, re-run `make check`.

**If NO (test doesn't reference function but still fails):**
This is the critical case. The test exercises production code that internally uses `{{function}}` via pointer/vtable. This PROVES the function is NOT dead.

### Decision Tree

```
Can you find {{function}} assigned/passed in src/?
├─ YES → NOT DEAD (pointer usage)
└─ NO → Does production build without it?
         ├─ NO → NOT DEAD (direct dependency)
         └─ YES → Do all tests pass?
                  ├─ YES → DEAD (delete it)
                  └─ NO → For each failing test:
                          Does test directly call {{function}}?
                          ├─ YES → Delete test, retry
                          └─ NO → NOT DEAD (indirect pointer usage)
```

## What To Do

### If Function Is Dead

1. Delete the function from `{{file}}`
2. Delete declaration from header
3. Delete any tests that directly tested it
4. Clean up empty TCases and test files
5. Verify: `make bin/ikigai` passes, `make check` passes

### If Function Is NOT Dead

1. Add `{{function}}` to `.claude/data/dead-code-false-positives.txt`
2. Revert all changes: `jj restore`
3. Return DONE - this is a successful outcome (we learned something)

## Safety First

**This process is intentionally expensive. That's okay.**

The cost of a wrong deletion is severe: broken runtime behavior that might not be detected for weeks. The cost of a wrong false positive is trivial: we skip one function that could have been deleted.

**When in doubt, mark as false positive and move on.**

A conservative false positive list is far better than accidentally breaking production. We can always revisit false positives later with more sophisticated analysis. We cannot easily recover from a deletion that passes all tests but breaks real-world usage patterns.

## Important Reminders

- **Read the code** - Don't just grep mechanically. Understand what the function does and how it might be used.
- **Trust the tests** - If a test fails and doesn't reference the function, production code needs it.
- **Both outcomes are valid** - Proving a function is NOT dead is just as valuable as removing dead code.
- **When uncertain, mark false positive** - Wrong deletions are catastrophic. Wrong false positives are harmless.

## Acceptance

DONE when either:
1. Function confirmed dead: removed, all builds pass, all tests pass
2. Function confirmed NOT dead: recorded as false positive, changes reverted
