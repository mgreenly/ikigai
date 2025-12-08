# Task: Add Inbox to Agent Context

## Target
Phase 3: Inter-Agent Mailboxes - Step 3 (Inbox integration with agent context)

Supports User Stories:
- 30 (list inbox) - handler retrieves current agent's inbox via `agent->inbox`
- 35 (separator shows unread) - separator layer queries `agent->inbox->unread_count`
- 37 (notification on idle) - idle transition checks `agent->inbox->unread_count > 0`

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (design document, especially Notification System section)
- docs/memory.md (talloc ownership patterns)

## Pre-read Source (patterns)
- src/mail/inbox.h (ik_inbox_t structure, ik_inbox_create())
- src/mail/inbox.c (inbox implementation)
- src/agent.h (current agent context structure - target for modification)
- src/agent.c (agent creation - where inbox initialization goes)
- src/repl.h (shows how agent context is used, ik_repl_state_t for idle detection)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c (agent context tests, fixture patterns)
- tests/unit/mail/inbox_test.c (inbox operation tests)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `ik_inbox_t` struct defined with fields: messages, count, unread_count, capacity
- `ik_inbox_create()` factory function implemented in src/mail/inbox.h and inbox.c
- `ik_inbox_add()`, `ik_inbox_get_all()`, `ik_inbox_get_by_id()`, `ik_inbox_mark_read()` operations implemented
- Test file at tests/unit/mail/inbox_test.c with comprehensive coverage
- `ik_agent_ctx_t` exists with identity fields (agent_id, numeric_id) and all Phase 1 fields
- Agent context is child of repl_ctx in talloc hierarchy

## Task
Add `ik_inbox_t *inbox` and `bool mail_notification_pending` fields to `ik_agent_ctx_t`. This integrates the mail module into the agent module, enabling per-agent message storage.

**From design document (docs/backlog/inter-agent-mailboxes.md):**
```
Ownership: Inbox owned by agent context. Messages owned by inbox.
```

**Notification debounce field (from design doc):**
```c
typedef struct ik_agent_ctx {
    // ... other fields
    bool mail_notification_pending;  // True if notified but not yet checked
} ik_agent_ctx_t;
```

**Key design decisions:**
- Inbox created during agent initialization
- Inbox freed automatically via talloc when agent freed
- `mail_notification_pending` starts as false
- `mail_notification_pending` is set to true when notification injected
- `mail_notification_pending` is reset to false when agent uses mail tool with `action: inbox` or `action: read`

**Ownership hierarchy:**
```
repl_ctx (coordinator)
  └─> agent (per-agent state)
        ├─> inbox (owned by agent)
        │     └─> messages[] (owned by inbox)
        └─> other fields...
```

**User story requirements:**
- Story 30: `/mail` command needs access to `agent->inbox` to list messages
- Story 35: Separator layer needs `agent->inbox->unread_count` for `[mail:N]` display
- Story 37: Idle transition needs to check `agent->inbox->unread_count > 0` and `!agent->mail_notification_pending` before injecting notification

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add include for mail inbox:
     ```c
     #include "mail/inbox.h"
     ```
   - Add fields to `ik_agent_ctx_t`:
     ```c
     // Mail inbox (per-agent)
     // Each agent has its own inbox for receiving messages from other agents.
     // Inbox is created during agent init and freed automatically via talloc.
     ik_inbox_t *inbox;

     // Notification debounce flag (per-agent)
     // Set to true when mail notification injected, reset when agent checks mail.
     // Prevents repeated notifications for same unread messages.
     bool mail_notification_pending;
     ```

2. Update `tests/unit/agent/agent_test.c` - add new tests:
   ```c
   // ============================================================
   // Inbox integration tests
   // ============================================================

   // Test: Agent has inbox after creation
   START_TEST(test_agent_has_inbox)
   {
       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, 0, &agent);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(agent->inbox);
   }
   END_TEST

   // Test: Inbox is empty on creation
   START_TEST(test_agent_inbox_initially_empty)
   {
       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, 0, &agent);

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(agent->inbox->count, 0);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);
   }
   END_TEST

   // Test: Inbox is talloc child of agent
   START_TEST(test_agent_inbox_ownership)
   {
       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, 0, &agent);

       ck_assert(is_ok(&res));
       ck_assert_ptr_eq(talloc_parent(agent->inbox), agent);
   }
   END_TEST

   // Test: Inbox freed when agent freed (no crash, no leak)
   START_TEST(test_agent_inbox_freed_with_agent)
   {
       TALLOC_CTX *parent = talloc_new(ctx);
       ck_assert_ptr_nonnull(parent);

       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(parent, shared, 0, &agent);
       ck_assert(is_ok(&res));

       // Add a message to inbox to ensure cleanup handles non-empty case
       ik_mail_msg_t *msg = NULL;
       res = ik_mail_msg_create(agent, 1, "1/", "0/", "Test message", 1700000000, false, &msg);
       ck_assert(is_ok(&res));
       ik_inbox_add(agent->inbox, msg);

       // Free parent should free agent and inbox (no crash, no leak)
       talloc_free(parent);
   }
   END_TEST

   // Test: mail_notification_pending starts as false
   START_TEST(test_agent_notification_pending_initially_false)
   {
       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, 0, &agent);

       ck_assert(is_ok(&res));
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: mail_notification_pending can be set and cleared
   START_TEST(test_agent_notification_pending_settable)
   {
       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, 0, &agent);
       ck_assert(is_ok(&res));

       // Set to true (simulates notification injection)
       agent->mail_notification_pending = true;
       ck_assert(agent->mail_notification_pending);

       // Clear (simulates agent checking mail)
       agent->mail_notification_pending = false;
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: Inbox operations work through agent
   START_TEST(test_agent_inbox_operations)
   {
       ik_agent_ctx_t *agent = NULL;
       res_t res = ik_agent_create(ctx, shared, 0, &agent);
       ck_assert(is_ok(&res));

       // Create and add a message
       ik_mail_msg_t *msg = NULL;
       res = ik_mail_msg_create(agent, 42, "1/", "0/", "Hello agent 0/", 1700000000, false, &msg);
       ck_assert(is_ok(&res));

       res = ik_inbox_add(agent->inbox, msg);
       ck_assert(is_ok(&res));

       // Verify message is in inbox
       ck_assert_uint_eq(agent->inbox->count, 1);
       ck_assert_uint_eq(agent->inbox->unread_count, 1);

       // Find by ID
       ik_mail_msg_t *found = ik_inbox_get_by_id(agent->inbox, 42);
       ck_assert_ptr_eq(found, msg);

       // Mark as read
       res = ik_inbox_mark_read(agent->inbox, msg);
       ck_assert(is_ok(&res));
       ck_assert_uint_eq(agent->inbox->unread_count, 0);
   }
   END_TEST

   // Test: Multiple agents have separate inboxes
   START_TEST(test_multiple_agents_separate_inboxes)
   {
       ik_agent_ctx_t *agent0 = NULL;
       ik_agent_ctx_t *agent1 = NULL;

       res_t res = ik_agent_create(ctx, shared, 0, &agent0);
       ck_assert(is_ok(&res));

       res = ik_agent_create(ctx, shared, 1, &agent1);
       ck_assert(is_ok(&res));

       // Different inbox pointers
       ck_assert_ptr_ne(agent0->inbox, agent1->inbox);

       // Add message to agent0's inbox
       ik_mail_msg_t *msg = NULL;
       res = ik_mail_msg_create(agent0, 1, "1/", "0/", "For agent 0", 1700000000, false, &msg);
       ck_assert(is_ok(&res));
       ik_inbox_add(agent0->inbox, msg);

       // Only agent0 has the message
       ck_assert_uint_eq(agent0->inbox->count, 1);
       ck_assert_uint_eq(agent1->inbox->count, 0);
   }
   END_TEST
   ```

3. Add test case to suite:
   ```c
   TCase *tc_inbox = tcase_create("Inbox");
   tcase_add_checked_fixture(tc_inbox, setup, teardown);
   tcase_add_test(tc_inbox, test_agent_has_inbox);
   tcase_add_test(tc_inbox, test_agent_inbox_initially_empty);
   tcase_add_test(tc_inbox, test_agent_inbox_ownership);
   tcase_add_test(tc_inbox, test_agent_inbox_freed_with_agent);
   tcase_add_test(tc_inbox, test_agent_notification_pending_initially_false);
   tcase_add_test(tc_inbox, test_agent_notification_pending_settable);
   tcase_add_test(tc_inbox, test_agent_inbox_operations);
   tcase_add_test(tc_inbox, test_multiple_agents_separate_inboxes);
   suite_add_tcase(s, tc_inbox);
   ```

4. Add required includes to test file:
   ```c
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   ```

5. Run `make check` - expect failures (inbox field doesn't exist yet)

### Green
1. Update `src/agent.h`:
   - Add include after existing includes (maintain alphabetical order within project includes):
     ```c
     #include "mail/inbox.h"
     ```
   - Add fields to struct (place after existing per-agent fields, before thread fields if present):
     ```c
     // Mail inbox (per-agent)
     // Each agent has its own inbox for receiving messages from other agents.
     // Inbox is created during agent init and freed automatically via talloc.
     ik_inbox_t *inbox;

     // Notification debounce flag (per-agent)
     // Set to true when mail notification injected, reset when agent checks mail.
     // Prevents repeated notifications for same unread messages.
     bool mail_notification_pending;
     ```

2. Update `src/agent.c`:
   - No additional includes needed (mail/inbox.h included via agent.h)
   - Add inbox initialization in `ik_agent_create()` after other field initialization:
     ```c
     // Create agent inbox
     agent->inbox = ik_inbox_create(agent);
     // Note: ik_inbox_create() PANICs on OOM, no NULL check needed

     // Initialize notification debounce flag
     agent->mail_notification_pending = false;
     ```

3. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`agent.h`)
   - Project headers next (alphabetically)
   - System headers last

2. Verify `// comments` style used (not `/* */`)

3. Verify LCOV_EXCL_BR_LINE comments on assert/PANIC statements if any added

4. Run `make lint` - verify clean

5. Run `make coverage` - verify 100% coverage on modified code

6. Run `make check-valgrind` - verify no memory leaks

7. Verify inbox ownership is correct:
   - Inbox is talloc child of agent
   - Messages added to inbox become children of inbox's messages array
   - Freeing agent frees inbox and all messages

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on modified files
- `ik_agent_ctx_t` has `ik_inbox_t *inbox` field
- `ik_agent_ctx_t` has `bool mail_notification_pending` field
- `ik_agent_create()` creates inbox and initializes notification flag
- Inbox is talloc child of agent (proper ownership)
- Agent tests verify inbox integration:
  - Inbox exists after creation
  - Inbox is initially empty
  - Inbox ownership is correct
  - Inbox freed with agent
  - Notification flag starts false
  - Inbox operations work through agent
  - Multiple agents have separate inboxes
- No changes to inbox.h or inbox.c (pure integration)
- src/agent.h includes mail/inbox.h

## Notes

### Future Integration Points (not implemented in this task)

1. **Story 30 (list inbox)**: `/mail` command handler will access `repl->agent->inbox` and call `ik_inbox_get_all()` to retrieve messages.

2. **Story 35 (separator shows unread)**: Separator layer render function will check `agent->inbox->unread_count` and display `[mail:N]` when > 0.

3. **Story 37 (notification on idle)**: State transition to IDLE will check:
   ```c
   if (agent->inbox->unread_count > 0 && !agent->mail_notification_pending) {
       inject_notification(agent);
       agent->mail_notification_pending = true;
   }
   ```

4. **Clearing notification flag**: Mail tool handler will reset flag:
   ```c
   if (action == INBOX || action == READ) {
       agent->mail_notification_pending = false;
   }
   ```

### Testing Considerations

1. **Test fixture**: Agent tests need access to shared context for `ik_agent_create()`. Follow existing test patterns in `tests/unit/agent/agent_test.c`.

2. **Memory testing**: Use valgrind to verify no leaks when:
   - Agent with empty inbox is freed
   - Agent with messages in inbox is freed
   - Multiple agents with inboxes are created and freed

3. **Thread safety**: The `mail_notification_pending` flag is a simple bool accessed from main thread only. No mutex needed. If future phases add background notification checking, consider `atomic_bool`.

### Error Handling

- `ik_inbox_create()` PANICs on OOM (consistent with other agent subsystems)
- No error return from inbox initialization (follows agent create pattern)
- Inbox operations that can fail return `res_t` (already implemented in inbox.c)
