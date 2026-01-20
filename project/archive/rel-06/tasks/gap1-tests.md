# Task: Comprehensive Tests for Agent Restoration System

## Target
Gap 1 + Gap 5 merged: Startup Agent Restoration Loop (PR2) - Test Coverage

## Macro Context

This task creates comprehensive tests for the agent restoration system that was implemented in previous Gap 1 tasks.

**What this task tests:**

The entire agent restoration system, including:
- `ik_agent_restore()` - Creates agent context from DB row (uses DB data instead of generating new)
- `ik_repl_restore_agents()` - Orchestrates startup restoration for all running agents
- Integration of these components for multi-agent persistence across restarts

**Why comprehensive tests are needed:**

Multi-agent persistence is a critical feature. Without proper test coverage:
- Regression bugs could silently break agent restoration
- Edge cases (deep ancestry, fork points, clear events) might not be handled correctly
- Parent-child hierarchy corruption could go undetected
- Conversation/scrollback reconstruction errors could cause data loss

**Test categories:**

1. **Unit Tests** - Test individual functions in isolation:
   - `ik_agent_restore()` tests in `tests/unit/agent/agent_test.c`
   - `ik_repl_restore_agents()` tests in new `tests/unit/repl/agent_restore_test.c`

2. **Integration Tests** - Test the complete restoration flow:
   - Multi-agent scenarios requiring database interaction
   - End-to-end hierarchy preservation
   - Fork point boundary enforcement

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- rel-06/gap.md (Testing Considerations section at the end - contains the complete test specification)

## Pre-read Source

**Reference test files to understand patterns:**
- tests/unit/agent/agent_test.c (READ - existing agent tests, shows test structure and patterns)
- tests/unit/db/agent_registry_test.c (READ - DB-related tests with transaction rollback pattern)
- tests/unit/db/agent_replay_test.c (READ - replay tests with helper functions for inserting messages)
- tests/test_utils_helper.h (READ - test utilities and database helpers)

**Source files being tested:**
- src/agent.h (READ - `ik_agent_restore()` declaration)
- src/agent.c (READ - `ik_agent_restore()` implementation)
- src/repl/agent_restore.h (READ - `ik_repl_restore_agents()` declaration)
- src/repl/agent_restore.c (READ - `ik_repl_restore_agents()` implementation)
- src/db/agent.h (READ - `ik_db_agent_row_t` struct and `ik_db_agent_list_running()`)

## Test Files to MODIFY
- tests/unit/agent/agent_test.c (MODIFY - add `ik_agent_restore()` unit tests)

## Test Files to CREATE
- tests/unit/repl/agent_restore_test.c (CREATE - tests for `ik_repl_restore_agents()`)
- tests/integration/agent_restore_test.c (CREATE - integration tests for multi-agent restoration)

## Makefile Updates
- Makefile (MODIFY - add test targets for new test files)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior tasks completed: gap1-delete-obsolete.md (all Gap 1 implementation tasks complete)
- `ik_agent_restore()` exists and is functional
- `ik_repl_restore_agents()` exists and is functional

## Task

### Part 1: Add unit tests to tests/unit/agent/agent_test.c

Add the following tests for `ik_agent_restore()`:

```c
// ========== ik_agent_restore() Tests ==========

// Test: ik_agent_restore() creates agent from DB row successfully
START_TEST(test_agent_restore_creates_from_db_row)
{
    // Create mock DB row with valid data
    // Call ik_agent_restore()
    // Verify agent is created successfully
    // Verify all components initialized (scrollback, layer_cake, conversation, etc.)
}
END_TEST

// Test: ik_agent_restore() uses row UUID, not generated
START_TEST(test_agent_restore_uses_row_uuid_not_generated)
{
    // Create DB row with specific UUID "test-uuid-123"
    // Call ik_agent_restore()
    // Verify agent->uuid == "test-uuid-123"
}
END_TEST

// Test: ik_agent_restore() sets fork_message_id from row
START_TEST(test_agent_restore_sets_fork_message_id)
{
    // Create DB row with fork_message_id = "42"
    // Call ik_agent_restore()
    // Verify agent->fork_message_id == 42
}
END_TEST

// Test: ik_agent_restore() sets parent_uuid from row
START_TEST(test_agent_restore_sets_parent_uuid)
{
    // Create DB row with parent_uuid = "parent-uuid-456"
    // Call ik_agent_restore()
    // Verify agent->parent_uuid == "parent-uuid-456"
}
END_TEST

// Test: ik_agent_restore() sets created_at from row
START_TEST(test_agent_restore_sets_created_at)
{
    // Create DB row with created_at = 1234567890
    // Call ik_agent_restore()
    // Verify agent->created_at == 1234567890
}
END_TEST

// Test: ik_agent_restore() sets name from row if present
START_TEST(test_agent_restore_sets_name_if_present)
{
    // Create DB row with name = "My Agent"
    // Call ik_agent_restore()
    // Verify agent->name == "My Agent"
}
END_TEST

// Test: ik_agent_restore() sets name to NULL if not present in row
START_TEST(test_agent_restore_null_name_if_not_present)
{
    // Create DB row with name = NULL
    // Call ik_agent_restore()
    // Verify agent->name == NULL
}
END_TEST
```

### Part 2: Create tests/unit/repl/agent_restore_test.c

Create new test file for `ik_repl_restore_agents()`:

```c
/**
 * @file agent_restore_test.c
 * @brief Tests for agent startup restoration functionality
 *
 * Tests for ik_repl_restore_agents() which restores all running agents
 * from the database on startup.
 */

#include "../../../src/repl/agent_restore.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// Follow the pattern from agent_registry_test.c for DB setup

// ========== Test Cases ==========

// Test: restore_agents queries running agents from DB
START_TEST(test_restore_agents_queries_running_agents)
{
    // Setup: Insert multiple agents with status='running'
    // Call ik_repl_restore_agents()
    // Verify all running agents are restored (except Agent 0)
}
END_TEST

// Test: restore_agents sorts by created_at (oldest first)
START_TEST(test_restore_agents_sorts_by_created_at)
{
    // Setup: Insert agents with out-of-order created_at timestamps
    // Call ik_repl_restore_agents()
    // Verify agents were processed oldest-first
}
END_TEST

// Test: restore_agents skips Agent 0 (parent_uuid=NULL), restores others
START_TEST(test_restore_agents_skips_none_restores_all_running)
{
    // Setup: Insert Agent 0 (root) and child agents
    // Call ik_repl_restore_agents()
    // Verify Agent 0 skipped, children restored
}
END_TEST

// Test: restore_agents handles Agent 0 specially (already created)
START_TEST(test_restore_agents_handles_agent0_specially)
{
    // Setup: Agent 0 already in repl->agents[0]
    // Insert Agent 0 record in DB
    // Call ik_repl_restore_agents()
    // Verify Agent 0 not duplicated
}
END_TEST

// Test: restore_agents populates conversation from replay
START_TEST(test_restore_agents_populates_conversation)
{
    // Setup: Insert agent with message history in DB
    // Call ik_repl_restore_agents()
    // Verify agent->conversation contains correct messages
}
END_TEST

// Test: restore_agents populates scrollback from replay
START_TEST(test_restore_agents_populates_scrollback)
{
    // Setup: Insert agent with message history in DB
    // Call ik_repl_restore_agents()
    // Verify agent->scrollback rendered correctly
}
END_TEST

// Test: restore_agents restores marks from replay context
START_TEST(test_restore_agents_restores_marks)
{
    // Setup: Insert agent with mark events in DB
    // Call ik_repl_restore_agents()
    // Verify agent->marks contains correct marks
}
END_TEST

// Test: restore_agents handles agent with empty history
START_TEST(test_restore_agents_handles_empty_history)
{
    // Setup: Insert agent with no messages
    // Call ik_repl_restore_agents()
    // Verify agent restored with empty conversation/scrollback
}
END_TEST

// Test: restore_agents handles restore failure gracefully
START_TEST(test_restore_agents_handles_restore_failure_gracefully)
{
    // Setup: Create conditions that cause restore to fail for one agent
    // Call ik_repl_restore_agents()
    // Verify other agents still restored, failed agent marked dead
}
END_TEST
```

### Part 3: Create tests/integration/agent_restore_test.c

Create integration test file for complete restoration scenarios:

```c
/**
 * @file agent_restore_test.c
 * @brief Integration tests for multi-agent restoration
 *
 * Tests the complete agent restoration flow including:
 * - Multi-agent hierarchy preservation
 * - Fork point boundary enforcement
 * - Clear event handling
 */

#include "../../src/repl/agent_restore.h"
#include "../../src/db/agent.h"
#include "../../src/db/agent_replay.h"
#include "../../src/db/connection.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/agent.h"
#include "../../src/repl.h"
#include "../../src/error.h"
#include "../test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// ========== Integration Test Cases ==========

// Test: Multiple agents survive restart with hierarchy preserved
START_TEST(test_multi_agent_restart_preserves_hierarchy)
{
    // Setup:
    // 1. Insert parent agent (Agent 0)
    // 2. Insert child agent forked from parent
    // 3. Insert messages for both
    //
    // Simulate restart:
    // 4. Create fresh repl context with Agent 0
    // 5. Call ik_repl_restore_agents()
    //
    // Verify:
    // - Child agent exists in repl->agents[]
    // - Child's parent_uuid points to Agent 0
    // - Child's conversation contains parent's pre-fork messages
    // - Child's conversation contains child's own messages
}
END_TEST

// Test: Forked agent survives restart with correct history
START_TEST(test_forked_agent_survives_restart)
{
    // Setup:
    // 1. Create parent with messages: [A, B, C]
    // 2. Fork child after message B (fork_message_id points to B)
    // 3. Add parent messages: [D, E] (post-fork)
    // 4. Add child messages: [X, Y]
    //
    // Simulate restart and restore
    //
    // Verify:
    // - Child sees: [A, B, X, Y] (not D, E)
    // - Parent sees: [A, B, C, D, E] (all its messages)
}
END_TEST

// Test: Killed agents not restored (status != 'running')
START_TEST(test_killed_agent_not_restored)
{
    // Setup:
    // 1. Insert agent with status='dead'
    // 2. Insert agent with status='running'
    //
    // Call ik_repl_restore_agents()
    //
    // Verify:
    // - Only running agent restored
    // - Dead agent not in repl->agents[]
}
END_TEST

// Test: Fork points respected on restore - child doesn't see parent's post-fork messages
START_TEST(test_fork_points_respected_on_restore)
{
    // This is the critical test for fork boundary enforcement
    //
    // Setup scenario:
    // - Parent: msg1, msg2, msg3 (fork here), msg4, msg5
    // - Child forked at msg3, adds: child_msg1, child_msg2
    //
    // After restore:
    // - Child's conversation should have: msg1, msg2, msg3, child_msg1, child_msg2
    // - Child should NOT have: msg4, msg5
}
END_TEST

// Test: Clear events respected - don't walk past clear
START_TEST(test_clear_events_respected_on_restore)
{
    // Setup:
    // - Agent with: msg1, msg2, clear, msg3, msg4
    //
    // After restore:
    // - Agent's conversation should have: msg3, msg4
    // - Agent should NOT have: msg1, msg2
}
END_TEST

// Test: Deep ancestry - grandchild accessing grandparent context
START_TEST(test_deep_ancestry_on_restore)
{
    // Setup 3-level hierarchy:
    // - Grandparent: gp_msg1, gp_msg2
    // - Parent (forked from grandparent): p_msg1
    // - Child (forked from parent): c_msg1
    //
    // After restore, child should see:
    // - gp_msg1, gp_msg2 (from grandparent, up to fork point)
    // - p_msg1 (from parent, up to fork point)
    // - c_msg1 (own messages)
}
END_TEST

// Test: Dependency ordering - parents created before children
START_TEST(test_dependency_ordering_on_restore)
{
    // Setup:
    // - Insert agents with shuffled created_at values
    // - Child has earlier created_at than parent (edge case, shouldn't happen but test robustness)
    //
    // After restore:
    // - Verify parents always exist before children are processed
    // - Or verify error handling if dependency cannot be satisfied
}
END_TEST
```

### Part 4: Update Makefile

Add test targets for the new test files. Look at existing test target patterns and add:

```makefile
# Agent restore unit tests
agent_restore_test: tests/unit/repl/agent_restore_test.c $(MODULE_SOURCES) $(TEST_UTILS)
	$(CC) $(CFLAGS) $(TEST_CFLAGS) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)

# Agent restore integration tests
agent_restore_integration_test: tests/integration/agent_restore_test.c $(MODULE_SOURCES) $(TEST_UTILS)
	$(CC) $(CFLAGS) $(TEST_CFLAGS) -o $@ $^ $(LDFLAGS) $(TEST_LDFLAGS)
```

Add the new test executables to the `check` target.

## TDD Cycle

### Red
1. Create test files with test stubs
2. Run `make` - should compile tests
3. Run individual tests - should fail (tests check actual behavior)

### Green
1. If tests fail due to implementation bugs, fix the implementation
2. Run tests until all pass
3. Run `make check` - all tests should pass

### Refactor
1. Review test coverage - ensure all scenarios from gap.md "Testing Considerations" are covered
2. Add any missing edge case tests
3. Run `make lint` - should pass

## Sub-agent Usage

Because this task involves writing many test cases, use sub-agents for:
- Writing individual test functions
- Running `make` and `make check` for feedback
- Searching for existing test patterns to follow

**Recommendation:** Use sub-agents to write test implementations in parallel if they are independent.

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- 100% coverage of new `ik_agent_restore()` code paths
- 100% coverage of new `ik_repl_restore_agents()` code paths
- All scenarios from gap.md "Testing Considerations" are tested:
  - Multiple agents survive restart
  - Parent-child hierarchy preserved
  - Each agent's conversation intact
  - Each agent's scrollback rendered correctly
  - Fork points respected (child doesn't see parent's post-fork messages)
  - Clear events respected (don't walk past clear)
  - Deep ancestry (grandchild accessing grandparent context)
  - Dependency ordering (parents created before children)
- Working tree is clean (all changes committed)

## Deviations

If you need to deviate from this plan (e.g., different test structure, additional test utilities needed, test file organization changes), document the deviation and reasoning in your final report.

**Expected deviations:**
1. **Test helper functions:** You may need to create helper functions for setting up test scenarios (inserting agents, messages, etc.). Use the patterns from `agent_replay_test.c`.
2. **Mock objects:** You may need to mock certain components. Document how mocking is done.
3. **Test organization:** If the test count is large, consider splitting into multiple test files by category.
