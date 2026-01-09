# Reminders for rel-08

## Test Signature Fixes Applied

**Fixed:** `tests/unit/terminal/terminal_csi_u_test.c`
- Added NULL logger parameter to all 11 `ik_term_init()` calls
- Signature changed from `ik_term_init(ctx, &term)` to `ik_term_init(ctx, NULL, &term)`

**Fixed:** `tests/unit/db/connection_test.c`
- Added `"share/ikigai"` data_dir parameter to all 11 `ik_db_init()` calls
- Signature changed from `ik_db_init(ctx, conn_str, &db)` to `ik_db_init(ctx, conn_str, "share/ikigai", &db)`

## Missing Test Implementations

**File:** `tests/unit/repl/agent_restore_core_tests.h`

The following 7 test functions are declared but not implemented. They have been temporarily commented out to allow compilation:

1. `test_restore_agents_queries_running_agents`
2. `test_restore_agents_sorts_by_created_at`
3. `test_restore_agents_skips_none_restores_all_running`
4. `test_restore_agents_handles_agent0_specially`
5. `test_restore_agents_populates_conversation`
6. `test_restore_agents_populates_scrollback`
7. `test_restore_agents_handles_mark_events`

**Action Required:**
- Implement these test functions in the appropriate test file
- Uncomment the declarations in `agent_restore_core_tests.h`
- Add corresponding test case additions to the suite setup

**Why this matters:**
These tests appear to be for core agent restore functionality. Missing tests mean untested code paths, which violates the 100% coverage requirement.

**Created:** 2026-01-06
**Priority:** High (blocks 100% test coverage)
