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

### Default Assumption: It's Dead

**If cflow says unreachable from main AND you find no pointer assignments in `src/`, the function is almost certainly dead.**

These two checks cover all the ways C code can invoke a function:
1. Direct call → cflow would find it
2. Pointer/callback → assignment would exist in source

When both checks say "not used", be skeptical of any argument that the function is needed. The burden of proof is on showing it IS used, not on proving it ISN'T. Tests failing doesn't automatically mean the function is live - tests can be wrong, tests can be dead too.

### Tests Don't Count (But Be Careful)

Tests exist to verify production code works. A test calling a function doesn't make that function "used" - it just means someone wrote a test for it. If the function is dead in production, the test is dead too.

**The question is always: does PRODUCTION code (`src/`) need this function?**

### The Circular Dependency Trap

Here's a subtle failure mode to watch for:

1. Function `foo()` is truly dead (no real production usage)
2. Production code has `bar()` which internally calls `foo()`
3. Someone wrote `test_bar()` specifically to get coverage on `foo()`
4. `bar()` itself is ALSO only called from tests

When you remove `foo()`:
- `test_bar()` fails
- `test_bar()` doesn't directly reference `foo()` (it calls `bar()`)
- You might conclude: "indirect production usage, NOT DEAD"

**But that's wrong!** The test exercises `bar()`, and `bar()` calls `foo()`, but `bar()` is ALSO dead code. The test was written to cover `foo()` through a production wrapper, creating circular reasoning.

**The fix:** When a test fails without directly referencing `{{function}}`, identify what production function the test calls. Then verify THAT function is reachable from `main()` or command handlers. If the intermediary is also unreachable, you've found a cluster of dead code - delete it all.

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
This is the critical case - but DON'T assume it proves the function is live. You must dig deeper:

1. **Identify the intermediary**: What production function does the test call that leads to `{{function}}`?
2. **Verify the intermediary is reachable**: Run `cflow --main main src/*.c` and check if that intermediary function appears in the output. Also check command handlers.
3. **Interpret the result**:
   - Intermediary IS reachable from main → `{{function}}` is NOT dead (genuine production path)
   - Intermediary is NOT reachable from main → Both are dead. Delete `{{function}}`, the intermediary, AND the test. Re-run `make check`.

**Why this matters:** A test might exercise production code that calls `{{function}}`, but if that production code is ALSO unreachable from main, then the test was written just to get coverage on dead code. You've found a cluster of dead code - remove it all together.

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
                          └─ NO → What production function does test call?
                                  Is THAT function reachable from main/handlers?
                                  ├─ YES → NOT DEAD (genuine production path)
                                  └─ NO → DEAD CLUSTER (delete function, caller, and test)
```

## What To Do

### If Function Is Dead

1. Delete the function from `{{file}}`
2. Delete declaration from header
3. Delete any tests that directly tested it
4. If you found a dead cluster (intermediary functions that only exist to call `{{function}}`), delete those too along with their tests
5. Clean up empty TCases and test files
6. Verify: `make bin/ikigai` passes, `make check` passes

### If Function Is NOT Dead

1. Add `{{function}}` to `.claude/data/dead-code-false-positives.txt`
2. Revert all changes: `jj restore`
3. Return DONE - this is a successful outcome (we learned something)

## Confidence Calibration

**When static analysis is clear, trust it.**

If cflow says unreachable AND no pointer assignments exist, there is no mechanism by which C code can call this function. Period. These two checks are exhaustive:
- Direct calls → cflow finds them
- Indirect calls → require pointer assignment in source

A failing test is not evidence the function is live. It's evidence that *something* calls the function - but that something might itself be dead code, or might be test-only code.

**When to mark false positive:** Only when you find concrete evidence the function IS used - a pointer assignment, a vtable entry, a callback registration. Not because a test failed. Not because you're uncertain. Uncertainty in the face of clear static analysis should resolve toward "it's dead."

**When in genuine doubt:** If you find ambiguous evidence (complex macro usage, generated code, external linkage you can't trace), then mark as false positive. But "a test fails and I don't understand why" is not genuine doubt - that's a signal to investigate the test, not to give up.

## Important Reminders

- **Trust static analysis** - If cflow + grep for assignments both say "not used", the function is dead. Don't second-guess clear evidence.
- **Verify the call chain** - If a test fails without directly referencing the function, trace the path. A test exercising dead production code is itself dead.
- **Investigate, don't capitulate** - When something unexpected happens (test fails, build breaks), understand WHY before concluding the function is needed.
- **False positives require evidence** - Mark as false positive only when you find concrete proof of usage (pointer assignment, vtable, callback). Not because tests fail.

## Acceptance

DONE when either:
1. Function confirmed dead: removed, all builds pass, all tests pass
2. Function confirmed NOT dead: recorded as false positive, changes reverted
