# Task: Initialize agent created_at timestamp

## Target
Bug fix: `ik_agent_create()` does not initialize `created_at` field, causing:
- All forked agents have `created_at = 0`
- `Ctrl+Down` navigation fails (compares `created_at > 0`, always false)
- Database records show `created_at = 0` instead of actual creation time

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Source (patterns)
- READ: src/agent.h (ik_agent_ctx_t struct, created_at field at line 58)
- READ: src/agent.c (ik_agent_create function lines 31-138)
- READ: src/repl.c lines 515-539 (ik_repl_nav_child - see how created_at comparison fails)

## Pre-read Tests (patterns)
- READ: tests/unit/agent/agent_test.c (note: time.h already included at line 12)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task
Add `agent->created_at = time(NULL);` to `ik_agent_create()` after struct allocation.

**Root cause:**
In `src/agent.c`, `ik_agent_create()` allocates the agent with `talloc_zero_()` which sets all fields to 0, but never sets `created_at` to the current time.

**Impact:**
- `ik_repl_nav_child()` at line 529 compares `a->created_at > newest_time` where `newest_time` starts at 0
- Since `created_at` is also 0, the condition `0 > 0` is false
- No child is ever selected, breaking Ctrl+Down navigation

**Fix location:**
`src/agent.c` in `ik_agent_create()`, add after line 44 (after setting `agent->shared = shared;`).
This groups the `created_at` initialization with other identity fields (uuid, name, parent_uuid, shared).

```c
agent->shared = shared;
agent->created_at = time(NULL);  // <-- ADD THIS LINE
```

**Note:** `<time.h>` is already included at line 18 of src/agent.c - no new include needed.

## TDD Cycle

### Red
1. Modify test file: `tests/unit/agent/agent_test.c`

   Add this test before the `agent_suite()` function (around line 677):
   ```c
   // Test agent->created_at is set to current time
   START_TEST(test_agent_create_sets_created_at)
   {
       TALLOC_CTX *ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
       ck_assert_ptr_nonnull(shared);

       int64_t before = time(NULL);

       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, NULL, &agent);

       int64_t after = time(NULL);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(agent);
       ck_assert_int_ge(agent->created_at, before);
       ck_assert_int_le(agent->created_at, after);

       talloc_free(ctx);
   }
   END_TEST
   ```

2. Register the test in `agent_suite()` - add this line in the `tc_core` section:
   ```c
   tcase_add_test(tc_core, test_agent_create_sets_created_at);
   ```

3. Run `make check` - expect test failure (created_at will be 0, assertion `created_at >= before` fails)

### Green
1. Modify `src/agent.c` in `ik_agent_create()`:
   After line 44 (`agent->shared = shared;`), add:
   ```c
   agent->created_at = time(NULL);
   ```

2. Run `make check` - expect pass

### Refactor
1. No refactoring needed - single line addition
2. Run `make check` - verify still passing
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- Working tree is clean
- New agents have `created_at` set to current Unix timestamp
- Ctrl+Down navigation works (finds children by most recent created_at)

## Verification
After implementing, manually verify with:
1. Start ikigai
2. Fork an agent (`/fork`)
3. Use `Ctrl+Down` to navigate to child - should work now
4. Check `/agents` output - created_at should show actual timestamp
