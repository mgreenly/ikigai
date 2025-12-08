# Task: Inject Mail Notification on IDLE Transition

## Target
Phase 3: Inter-Agent Mailboxes - Step 13 (Notification injection on IDLE with unread mail)

Supports User Stories:
- 37 (notification on IDLE with unread) - Notification injected when agent transitions to IDLE with unread mail
- 38 (notification not repeated) - `mail_notification_pending` flag prevents duplicate notifications
- 39 (notification visible in scrollback) - Notification appears in scrollback with dim styling

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/mocking.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Notification System section - trigger, format, debounce)
- docs/rel-05/user-stories/37-notification-on-idle-with-unread.md (complete walkthrough)
- docs/rel-05/user-stories/38-notification-not-repeated.md (debounce behavior)
- docs/rel-05/user-stories/39-notification-visible-in-scrollback.md (visibility and styling)
- docs/memory.md (talloc ownership patterns)
- docs/return_values.md (res_t patterns)

### Pre-read Source (patterns)
- src/repl.h (ik_repl_ctx_t, ik_repl_state_t, state transitions)
- src/repl.c (ik_repl_transition_to_idle() - hook point for notification)
- src/agent.h (ik_agent_ctx_t - inbox field, mail_notification_pending flag)
- src/mail/inbox.h (ik_inbox_t - unread_count field)
- src/openai/client.h (ik_openai_conversation_add_msg, ik_openai_msg_create)
- src/event_render.h (ik_event_render - renders to scrollback)
- src/scrollback.h (ik_scrollback_append_line)
- src/msg.h (ik_msg_t - message structure)

### Pre-read Tests (patterns)
- tests/unit/repl/repl_scroll_test.c (repl context test patterns)
- tests/unit/mail/inbox_test.c (inbox fixture patterns)
- tests/unit/layer/separator_mail_test.c (mail notification test patterns)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- `make check` passes
- `make lint` passes
- mail-separator-unread.md complete: Separator shows [mail:N] when unread > 0
- mail-tool-handler.md complete: All mail operations working (inbox, read, send)
- `ik_agent_ctx_t` has `mail_notification_pending` bool field
- `ik_inbox_t` has `unread_count` field
- `ik_repl_transition_to_idle()` exists and is called when LLM response completes
- `ik_openai_conversation_add_msg()` can add messages to conversation
- `ik_event_render()` can render events to scrollback
- Current agent accessible via `repl->current_agent` (from Phase 1)
- State machine transitions: WAITING_FOR_LLM -> IDLE (via ik_repl_transition_to_idle)

## Task
Implement mail notification injection that automatically notifies the agent (and user) when the agent transitions to IDLE state and has unread mail. The notification is injected as a `role: user` message into the conversation and rendered to scrollback for visibility.

**Core behavior (from user story 37):**
1. Agent completes LLM response (no tool call pending)
2. Agent transitions to IDLE state via `ik_repl_transition_to_idle()`
3. IDLE transition handler checks `agent->inbox->unread_count`
4. `unread_count > 0` (agent has unread mail)
5. Handler checks `agent->mail_notification_pending` flag
6. Flag is false (no pending notification)
7. Handler creates notification message with correct grammar:
   - `[Notification: You have N unread messages in your inbox]` (plural for N > 1)
   - `[Notification: You have 1 unread message in your inbox]` (singular for N == 1)
8. Notification injected as `role: user` message into conversation
9. Notification rendered to scrollback (visible to user)
10. `mail_notification_pending` set to true (prevents repeat)
11. Notification included in next LLM context (agent can act on it)

**Notification format specification:**
```
[Notification: You have 2 unread messages in your inbox]
[Notification: You have 1 unread message in your inbox]
```

**Format rules:**
- Prefix: `[Notification: `
- Body: `You have {N} unread message{s} in your inbox`
- Grammar: "message" (singular) when N == 1, "messages" (plural) when N > 1
- Suffix: `]`
- No trailing newline in content (scrollback handles line breaks)

**Debounce behavior (from user story 38):**
- Check `mail_notification_pending` flag before injecting
- If flag is true, skip notification (already notified)
- Set flag to true after successful injection
- Flag is cleared when agent checks mail:
  - `action: inbox` in mail tool clears flag
  - `action: read` in mail tool clears flag
  - `/mail` or `/mail read` commands clear flag

**Scrollback visibility (from user story 39):**
- Notification rendered via `ik_event_render()` as kind "notification"
- Event renderer applies dim styling to `[Notification: ...]` prefix
- Notification visible in scrollback history (can scroll up to see)
- Notification included in LLM context (agent sees it to act)

**State transition hook location:**

The notification injection happens in `ik_repl_transition_to_idle()`:

```c
void ik_repl_transition_to_idle(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);

    // State transition (existing code)
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM);
    repl->state = IK_REPL_STATE_IDLE;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // UI updates (existing code)
    repl->spinner_state.visible = false;
    repl->input_buffer_visible = true;

    // NEW: Mail notification injection
    ik_repl_check_mail_notification(repl);
}
```

**Notification injection logic:**

```c
// Check if mail notification should be injected and inject if needed
// Called on IDLE transition after LLM response completes
//
// Conditions for injection:
// 1. Current agent exists
// 2. Agent has inbox with unread_count > 0
// 3. mail_notification_pending is false
//
// Actions on injection:
// 1. Create notification message text
// 2. Add as user message to conversation
// 3. Render to scrollback
// 4. Set mail_notification_pending = true
void ik_repl_check_mail_notification(ik_repl_ctx_t *repl);
```

**Key design decisions:**

1. **User role for notification**: Notification uses `role: user` so it appears as input that prompts the agent to respond. This allows autonomous agents to act on the notification.

2. **Conversation + scrollback**: Notification is added to both:
   - Conversation: For LLM context (agent can reason about it)
   - Scrollback: For user visibility (user sees what triggered agent)

3. **Dim styling**: Notifications use dim ANSI styling to distinguish from regular user input while remaining visible. The event renderer handles this based on the "notification" kind.

4. **Single injection point**: Only inject on transition TO IDLE, not while idle or on other transitions. This ensures exactly one notification per unread mail batch.

5. **Flag reset location**: The `mail_notification_pending` flag is cleared in the mail tool handler (inbox/read actions) and mail command handler (/mail, /mail read). This is already implemented in mail-tool-handler.md.

**Edge cases:**

1. **No current agent**: Skip notification (defensive check)
2. **Null inbox**: Skip notification (defensive check)
3. **Zero unread**: Skip notification (normal case)
4. **Pending true**: Skip notification (already notified)
5. **Conversation null**: Log warning, skip (should not happen in normal flow)
6. **Very high count**: Display actual number (no truncation)

## TDD Cycle

### Red
1. Create `src/mail/notification.h`:
   ```c
   #ifndef IK_MAIL_NOTIFICATION_H
   #define IK_MAIL_NOTIFICATION_H

   #include "../error.h"

   #include <talloc.h>
   #include <stddef.h>
   #include <stdbool.h>

   // Forward declarations
   struct ik_repl_ctx_t;
   struct ik_agent_ctx_t;

   /**
    * Check if mail notification should be injected and inject if needed.
    *
    * Called on IDLE state transition after LLM response completes.
    * Checks if current agent has unread mail and notification is not pending.
    * If conditions met, injects notification message into conversation and scrollback.
    *
    * Conditions for injection:
    * 1. repl->current_agent is not NULL
    * 2. agent->inbox is not NULL
    * 3. agent->inbox->unread_count > 0
    * 4. agent->mail_notification_pending is false
    *
    * Actions on successful injection:
    * 1. Creates notification message with count and correct grammar
    * 2. Adds message to conversation as role: user
    * 3. Renders message to scrollback as kind: notification
    * 4. Sets agent->mail_notification_pending = true
    *
    * @param repl  REPL context (provides current_agent, conversation, scrollback)
    */
   void ik_repl_check_mail_notification(struct ik_repl_ctx_t *repl);

   /**
    * Build notification message text for given unread count.
    *
    * Creates message with correct singular/plural grammar:
    * - Count 1: "[Notification: You have 1 unread message in your inbox]"
    * - Count N: "[Notification: You have N unread messages in your inbox]"
    *
    * @param ctx    Talloc context for result allocation
    * @param count  Number of unread messages (must be > 0)
    * @return       Allocated message string (owned by ctx)
    *
    * Note: Panics on OOM. Returns NULL if count == 0 (defensive).
    */
   char *ik_mail_notification_build_message(TALLOC_CTX *ctx, size_t count);

   /**
    * Check if notification should be injected for given agent.
    *
    * Pure predicate function for testability.
    *
    * @param agent  Agent context to check
    * @return       true if notification should be injected
    *
    * Returns true iff:
    * - agent is not NULL
    * - agent->inbox is not NULL
    * - agent->inbox->unread_count > 0
    * - agent->mail_notification_pending is false
    */
   bool ik_mail_notification_should_inject(const struct ik_agent_ctx_t *agent);

   #endif // IK_MAIL_NOTIFICATION_H
   ```

2. Create `tests/unit/mail/notification_test.c`:
   ```c
   /**
    * @file notification_test.c
    * @brief Tests for mail notification injection
    *
    * Tests the notification injection system:
    * - Message format with correct grammar
    * - Injection conditions (unread > 0, pending flag)
    * - Conversation and scrollback updates
    * - Debounce behavior (pending flag)
    */

   #include "../../../src/mail/notification.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/agent.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_agent_ctx_t *agent;

   // Helper to create mock agent with inbox
   static ik_agent_ctx_t *create_mock_agent(TALLOC_CTX *parent, const char *agent_id)
   {
       ik_agent_ctx_t *a = talloc_zero(parent, ik_agent_ctx_t);
       if (a == NULL) return NULL;

       a->agent_id = talloc_strdup(a, agent_id);
       a->inbox = ik_inbox_create(a);
       a->mail_notification_pending = false;

       return a;
   }

   // Helper to add unread messages to inbox
   static void add_unread(ik_agent_ctx_t *a, size_t count)
   {
       for (size_t i = 0; i < count; i++) {
           ik_mail_msg_t *msg = NULL;
           res_t res = ik_mail_msg_create(ctx, (int64_t)i + 1, "1/", a->agent_id,
                                           "Test message", 1700000000 + (int64_t)i,
                                           false, &msg);
           ck_assert(is_ok(&res));
           ik_inbox_add(a->inbox, msg);
       }
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       agent = create_mock_agent(ctx, "0/");
       ck_assert_ptr_nonnull(agent);
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
       agent = NULL;
   }

   // ========== Message Format Tests ==========

   // Test: Message format with 1 unread (singular)
   START_TEST(test_notification_message_singular)
   {
       char *msg = ik_mail_notification_build_message(ctx, 1);

       ck_assert_ptr_nonnull(msg);
       ck_assert_str_eq(msg, "[Notification: You have 1 unread message in your inbox]");
   }
   END_TEST

   // Test: Message format with 2 unread (plural)
   START_TEST(test_notification_message_plural_two)
   {
       char *msg = ik_mail_notification_build_message(ctx, 2);

       ck_assert_ptr_nonnull(msg);
       ck_assert_str_eq(msg, "[Notification: You have 2 unread messages in your inbox]");
   }
   END_TEST

   // Test: Message format with many unread
   START_TEST(test_notification_message_plural_many)
   {
       char *msg = ik_mail_notification_build_message(ctx, 99);

       ck_assert_ptr_nonnull(msg);
       ck_assert_str_eq(msg, "[Notification: You have 99 unread messages in your inbox]");
   }
   END_TEST

   // Test: Message format with high count
   START_TEST(test_notification_message_high_count)
   {
       char *msg = ik_mail_notification_build_message(ctx, 1000);

       ck_assert_ptr_nonnull(msg);
       ck_assert_str_eq(msg, "[Notification: You have 1000 unread messages in your inbox]");
   }
   END_TEST

   // Test: Message format with zero returns NULL (defensive)
   START_TEST(test_notification_message_zero_returns_null)
   {
       char *msg = ik_mail_notification_build_message(ctx, 0);

       ck_assert_ptr_null(msg);
   }
   END_TEST

   // Test: Message is owned by provided context
   START_TEST(test_notification_message_ownership)
   {
       TALLOC_CTX *child = talloc_new(ctx);
       char *msg = ik_mail_notification_build_message(child, 1);

       ck_assert_ptr_nonnull(msg);
       ck_assert_ptr_eq(talloc_parent(msg), child);

       talloc_free(child);
   }
   END_TEST

   // Test: Message has correct prefix
   START_TEST(test_notification_message_prefix)
   {
       char *msg = ik_mail_notification_build_message(ctx, 5);

       ck_assert_ptr_nonnull(msg);
       ck_assert(strncmp(msg, "[Notification: ", 15) == 0);
   }
   END_TEST

   // Test: Message has correct suffix
   START_TEST(test_notification_message_suffix)
   {
       char *msg = ik_mail_notification_build_message(ctx, 5);

       ck_assert_ptr_nonnull(msg);
       size_t len = strlen(msg);
       ck_assert_uint_gt(len, 0);
       ck_assert(msg[len - 1] == ']');
   }
   END_TEST

   // ========== Should Inject Predicate Tests ==========

   // Test: Should inject when unread > 0 and pending false
   START_TEST(test_should_inject_true)
   {
       add_unread(agent, 1);
       agent->mail_notification_pending = false;

       ck_assert(ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: Should not inject when unread == 0
   START_TEST(test_should_inject_false_no_unread)
   {
       ck_assert_uint_eq(agent->inbox->unread_count, 0);
       agent->mail_notification_pending = false;

       ck_assert(!ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: Should not inject when pending true
   START_TEST(test_should_inject_false_pending)
   {
       add_unread(agent, 1);
       agent->mail_notification_pending = true;

       ck_assert(!ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: Should not inject when agent is NULL
   START_TEST(test_should_inject_false_null_agent)
   {
       ck_assert(!ik_mail_notification_should_inject(NULL));
   }
   END_TEST

   // Test: Should not inject when inbox is NULL
   START_TEST(test_should_inject_false_null_inbox)
   {
       ik_inbox_t *saved = agent->inbox;
       agent->inbox = NULL;

       ck_assert(!ik_mail_notification_should_inject(agent));

       agent->inbox = saved;
   }
   END_TEST

   // Test: Should inject with many unread
   START_TEST(test_should_inject_many_unread)
   {
       add_unread(agent, 50);
       agent->mail_notification_pending = false;

       ck_assert(ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // ========== Debounce Behavior Tests ==========

   // Test: Pending flag starts false
   START_TEST(test_pending_flag_initial_false)
   {
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: After injection, should_inject returns false
   START_TEST(test_debounce_after_injection)
   {
       add_unread(agent, 1);
       ck_assert(ik_mail_notification_should_inject(agent));

       // Simulate injection by setting pending
       agent->mail_notification_pending = true;

       ck_assert(!ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: After flag reset, should_inject returns true again
   START_TEST(test_debounce_after_reset)
   {
       add_unread(agent, 2);

       // First: should inject
       ck_assert(ik_mail_notification_should_inject(agent));

       // Simulate injection
       agent->mail_notification_pending = true;
       ck_assert(!ik_mail_notification_should_inject(agent));

       // Simulate mail check (flag reset)
       agent->mail_notification_pending = false;
       ck_assert(ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: New mail after pending flag set still blocked
   START_TEST(test_debounce_new_mail_while_pending)
   {
       add_unread(agent, 1);
       agent->mail_notification_pending = true;

       // More mail arrives
       add_unread(agent, 2);
       ck_assert_uint_eq(agent->inbox->unread_count, 3);

       // Still blocked due to pending flag
       ck_assert(!ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: Reading all mail but not resetting flag still blocks
   START_TEST(test_debounce_read_all_still_blocked)
   {
       add_unread(agent, 1);
       agent->mail_notification_pending = true;

       // Read all mail (unread_count = 0)
       ik_mail_msg_t *msg = ik_inbox_get_by_id(agent->inbox, 1);
       ik_inbox_mark_read(agent->inbox, msg);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);

       // No notification needed (no unread), regardless of pending
       ck_assert(!ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // ========== Edge Case Tests ==========

   // Test: Agent with read mail only
   START_TEST(test_read_mail_only)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 1, "1/", agent->agent_id,
                                       "Already read", 1700000000, true, &msg);
       ck_assert(is_ok(&res));
       ik_inbox_add(agent->inbox, msg);

       ck_assert_uint_eq(agent->inbox->count, 1);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);

       ck_assert(!ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: Mixed read and unread mail
   START_TEST(test_mixed_read_unread)
   {
       // Add read message
       ik_mail_msg_t *msg1 = NULL;
       ik_mail_msg_create(ctx, 1, "1/", agent->agent_id, "Read", 1700000000, true, &msg1);
       ik_inbox_add(agent->inbox, msg1);

       // Add unread message
       ik_mail_msg_t *msg2 = NULL;
       ik_mail_msg_create(ctx, 2, "1/", agent->agent_id, "Unread", 1700000001, false, &msg2);
       ik_inbox_add(agent->inbox, msg2);

       ck_assert_uint_eq(agent->inbox->count, 2);
       ck_assert_uint_eq(agent->inbox->unread_count, 1);

       ck_assert(ik_mail_notification_should_inject(agent));
   }
   END_TEST

   // Test: Message count accuracy in notification
   START_TEST(test_count_accuracy)
   {
       add_unread(agent, 7);
       ck_assert_uint_eq(agent->inbox->unread_count, 7);

       char *msg = ik_mail_notification_build_message(ctx, agent->inbox->unread_count);
       ck_assert_str_eq(msg, "[Notification: You have 7 unread messages in your inbox]");
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *notification_suite(void)
   {
       Suite *s = suite_create("MailNotification");

       TCase *tc_format = tcase_create("MessageFormat");
       tcase_add_checked_fixture(tc_format, setup, teardown);
       tcase_add_test(tc_format, test_notification_message_singular);
       tcase_add_test(tc_format, test_notification_message_plural_two);
       tcase_add_test(tc_format, test_notification_message_plural_many);
       tcase_add_test(tc_format, test_notification_message_high_count);
       tcase_add_test(tc_format, test_notification_message_zero_returns_null);
       tcase_add_test(tc_format, test_notification_message_ownership);
       tcase_add_test(tc_format, test_notification_message_prefix);
       tcase_add_test(tc_format, test_notification_message_suffix);
       suite_add_tcase(s, tc_format);

       TCase *tc_predicate = tcase_create("ShouldInject");
       tcase_add_checked_fixture(tc_predicate, setup, teardown);
       tcase_add_test(tc_predicate, test_should_inject_true);
       tcase_add_test(tc_predicate, test_should_inject_false_no_unread);
       tcase_add_test(tc_predicate, test_should_inject_false_pending);
       tcase_add_test(tc_predicate, test_should_inject_false_null_agent);
       tcase_add_test(tc_predicate, test_should_inject_false_null_inbox);
       tcase_add_test(tc_predicate, test_should_inject_many_unread);
       suite_add_tcase(s, tc_predicate);

       TCase *tc_debounce = tcase_create("Debounce");
       tcase_add_checked_fixture(tc_debounce, setup, teardown);
       tcase_add_test(tc_debounce, test_pending_flag_initial_false);
       tcase_add_test(tc_debounce, test_debounce_after_injection);
       tcase_add_test(tc_debounce, test_debounce_after_reset);
       tcase_add_test(tc_debounce, test_debounce_new_mail_while_pending);
       tcase_add_test(tc_debounce, test_debounce_read_all_still_blocked);
       suite_add_tcase(s, tc_debounce);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_read_mail_only);
       tcase_add_test(tc_edge, test_mixed_read_unread);
       tcase_add_test(tc_edge, test_count_accuracy);
       suite_add_tcase(s, tc_edge);

       return s;
   }

   int main(void)
   {
       Suite *s = notification_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Create `tests/unit/mail/notification_inject_test.c` (integration tests):
   ```c
   /**
    * @file notification_inject_test.c
    * @brief Integration tests for mail notification injection into REPL
    *
    * Tests the full injection flow:
    * - Conversation message addition
    * - Scrollback rendering
    * - Pending flag update
    * - IDLE transition hook
    */

   #include "../../../src/mail/notification.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/repl.h"
   #include "../../../src/agent.h"
   #include "../../../src/scrollback.h"
   #include "../../../src/openai/client.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_repl_ctx_t *repl;
   static ik_agent_ctx_t *agent;

   static ik_agent_ctx_t *create_mock_agent(TALLOC_CTX *parent, const char *agent_id)
   {
       ik_agent_ctx_t *a = talloc_zero(parent, ik_agent_ctx_t);
       if (a == NULL) return NULL;

       a->agent_id = talloc_strdup(a, agent_id);
       a->inbox = ik_inbox_create(a);
       a->mail_notification_pending = false;

       return a;
   }

   static void add_unread(ik_agent_ctx_t *a, size_t count)
   {
       for (size_t i = 0; i < count; i++) {
           ik_mail_msg_t *msg = NULL;
           ik_mail_msg_create(ctx, (int64_t)i + 1, "1/", a->agent_id,
                               "Test message", 1700000000 + (int64_t)i, false, &msg);
           ik_inbox_add(a->inbox, msg);
       }
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       // Create mock REPL context
       repl = talloc_zero(ctx, ik_repl_ctx_t);
       ck_assert_ptr_nonnull(repl);

       // Create scrollback
       repl->scrollback = ik_scrollback_create(ctx, 80);
       ck_assert_ptr_nonnull(repl->scrollback);

       // Create conversation
       res_t conv_res = ik_openai_conversation_create(ctx);
       ck_assert(is_ok(&conv_res));
       repl->conversation = conv_res.val;

       // Create agent
       agent = create_mock_agent(ctx, "0/");
       ck_assert_ptr_nonnull(agent);
       repl->current_agent = agent;

       // Set initial state to WAITING_FOR_LLM (transition target is IDLE)
       repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
       repl = NULL;
       agent = NULL;
   }

   // ========== Conversation Integration Tests ==========

   // Test: Notification adds message to conversation
   START_TEST(test_inject_adds_to_conversation)
   {
       add_unread(agent, 1);
       size_t initial_count = repl->conversation->message_count;

       ik_repl_check_mail_notification(repl);

       ck_assert_uint_eq(repl->conversation->message_count, initial_count + 1);
   }
   END_TEST

   // Test: Notification message has user role
   START_TEST(test_inject_message_role)
   {
       add_unread(agent, 1);

       ik_repl_check_mail_notification(repl);

       ik_msg_t *msg = repl->conversation->messages[repl->conversation->message_count - 1];
       ck_assert_str_eq(msg->kind, "user");
   }
   END_TEST

   // Test: Notification message has correct content
   START_TEST(test_inject_message_content)
   {
       add_unread(agent, 3);

       ik_repl_check_mail_notification(repl);

       ik_msg_t *msg = repl->conversation->messages[repl->conversation->message_count - 1];
       ck_assert_str_eq(msg->content, "[Notification: You have 3 unread messages in your inbox]");
   }
   END_TEST

   // Test: No message added when no unread
   START_TEST(test_inject_no_message_when_no_unread)
   {
       ck_assert_uint_eq(agent->inbox->unread_count, 0);
       size_t initial_count = repl->conversation->message_count;

       ik_repl_check_mail_notification(repl);

       ck_assert_uint_eq(repl->conversation->message_count, initial_count);
   }
   END_TEST

   // Test: No message added when pending
   START_TEST(test_inject_no_message_when_pending)
   {
       add_unread(agent, 1);
       agent->mail_notification_pending = true;
       size_t initial_count = repl->conversation->message_count;

       ik_repl_check_mail_notification(repl);

       ck_assert_uint_eq(repl->conversation->message_count, initial_count);
   }
   END_TEST

   // ========== Scrollback Integration Tests ==========

   // Test: Notification renders to scrollback
   START_TEST(test_inject_renders_to_scrollback)
   {
       add_unread(agent, 2);
       size_t initial_lines = ik_scrollback_get_line_count(repl->scrollback);

       ik_repl_check_mail_notification(repl);

       ck_assert_uint_gt(ik_scrollback_get_line_count(repl->scrollback), initial_lines);
   }
   END_TEST

   // Test: Scrollback contains notification text
   START_TEST(test_inject_scrollback_content)
   {
       add_unread(agent, 5);

       ik_repl_check_mail_notification(repl);

       // Get last line from scrollback
       size_t line_count = ik_scrollback_get_line_count(repl->scrollback);
       ck_assert_uint_gt(line_count, 0);

       const char *text = NULL;
       size_t length = 0;
       res_t res = ik_scrollback_get_line_text(repl->scrollback, line_count - 1, &text, &length);
       ck_assert(is_ok(&res));

       // Should contain notification
       ck_assert(strstr(text, "[Notification:") != NULL);
       ck_assert(strstr(text, "5 unread messages") != NULL);
   }
   END_TEST

   // Test: No scrollback change when no unread
   START_TEST(test_inject_no_scrollback_when_no_unread)
   {
       size_t initial_lines = ik_scrollback_get_line_count(repl->scrollback);

       ik_repl_check_mail_notification(repl);

       ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), initial_lines);
   }
   END_TEST

   // ========== Pending Flag Tests ==========

   // Test: Flag set after injection
   START_TEST(test_inject_sets_pending_flag)
   {
       add_unread(agent, 1);
       ck_assert(!agent->mail_notification_pending);

       ik_repl_check_mail_notification(repl);

       ck_assert(agent->mail_notification_pending);
   }
   END_TEST

   // Test: Flag not changed when no injection
   START_TEST(test_inject_flag_unchanged_when_no_inject)
   {
       // No unread
       ck_assert(!agent->mail_notification_pending);

       ik_repl_check_mail_notification(repl);

       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: Flag not changed when already pending
   START_TEST(test_inject_flag_unchanged_when_pending)
   {
       add_unread(agent, 1);
       agent->mail_notification_pending = true;

       ik_repl_check_mail_notification(repl);

       ck_assert(agent->mail_notification_pending);
   }
   END_TEST

   // ========== Null Safety Tests ==========

   // Test: Null repl handled gracefully
   START_TEST(test_inject_null_repl)
   {
       // Should not crash
       ik_repl_check_mail_notification(NULL);
   }
   END_TEST

   // Test: Null current_agent handled gracefully
   START_TEST(test_inject_null_agent)
   {
       repl->current_agent = NULL;
       size_t initial_count = repl->conversation->message_count;

       ik_repl_check_mail_notification(repl);

       ck_assert_uint_eq(repl->conversation->message_count, initial_count);
   }
   END_TEST

   // Test: Null conversation handled gracefully
   START_TEST(test_inject_null_conversation)
   {
       add_unread(agent, 1);
       repl->conversation = NULL;

       // Should not crash, should still set pending flag
       ik_repl_check_mail_notification(repl);

       // Pending flag should still be set (partial success)
       // Or implementation may skip entirely - either is acceptable
   }
   END_TEST

   // Test: Null scrollback handled gracefully
   START_TEST(test_inject_null_scrollback)
   {
       add_unread(agent, 1);
       repl->scrollback = NULL;

       // Should not crash
       ik_repl_check_mail_notification(repl);
   }
   END_TEST

   // ========== Multiple Call Tests ==========

   // Test: Second call with same unread blocked
   START_TEST(test_inject_second_call_blocked)
   {
       add_unread(agent, 2);

       ik_repl_check_mail_notification(repl);
       size_t count_after_first = repl->conversation->message_count;

       ik_repl_check_mail_notification(repl);
       ck_assert_uint_eq(repl->conversation->message_count, count_after_first);
   }
   END_TEST

   // Test: After flag reset, injection works again
   START_TEST(test_inject_after_flag_reset)
   {
       add_unread(agent, 1);

       ik_repl_check_mail_notification(repl);
       size_t count_after_first = repl->conversation->message_count;
       ck_assert(agent->mail_notification_pending);

       // Reset flag (simulates mail check)
       agent->mail_notification_pending = false;

       // Add more mail
       add_unread(agent, 1);

       ik_repl_check_mail_notification(repl);
       ck_assert_uint_eq(repl->conversation->message_count, count_after_first + 1);
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *notification_inject_suite(void)
   {
       Suite *s = suite_create("MailNotificationInject");

       TCase *tc_conv = tcase_create("Conversation");
       tcase_add_checked_fixture(tc_conv, setup, teardown);
       tcase_add_test(tc_conv, test_inject_adds_to_conversation);
       tcase_add_test(tc_conv, test_inject_message_role);
       tcase_add_test(tc_conv, test_inject_message_content);
       tcase_add_test(tc_conv, test_inject_no_message_when_no_unread);
       tcase_add_test(tc_conv, test_inject_no_message_when_pending);
       suite_add_tcase(s, tc_conv);

       TCase *tc_scroll = tcase_create("Scrollback");
       tcase_add_checked_fixture(tc_scroll, setup, teardown);
       tcase_add_test(tc_scroll, test_inject_renders_to_scrollback);
       tcase_add_test(tc_scroll, test_inject_scrollback_content);
       tcase_add_test(tc_scroll, test_inject_no_scrollback_when_no_unread);
       suite_add_tcase(s, tc_scroll);

       TCase *tc_flag = tcase_create("PendingFlag");
       tcase_add_checked_fixture(tc_flag, setup, teardown);
       tcase_add_test(tc_flag, test_inject_sets_pending_flag);
       tcase_add_test(tc_flag, test_inject_flag_unchanged_when_no_inject);
       tcase_add_test(tc_flag, test_inject_flag_unchanged_when_pending);
       suite_add_tcase(s, tc_flag);

       TCase *tc_null = tcase_create("NullSafety");
       tcase_add_checked_fixture(tc_null, setup, teardown);
       tcase_add_test(tc_null, test_inject_null_repl);
       tcase_add_test(tc_null, test_inject_null_agent);
       tcase_add_test(tc_null, test_inject_null_conversation);
       tcase_add_test(tc_null, test_inject_null_scrollback);
       suite_add_tcase(s, tc_null);

       TCase *tc_multi = tcase_create("MultipleCalls");
       tcase_add_checked_fixture(tc_multi, setup, teardown);
       tcase_add_test(tc_multi, test_inject_second_call_blocked);
       tcase_add_test(tc_multi, test_inject_after_flag_reset);
       suite_add_tcase(s, tc_multi);

       return s;
   }

   int main(void)
   {
       Suite *s = notification_inject_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

4. Create stub `src/mail/notification.c`:
   ```c
   #include "notification.h"

   #include "../agent.h"
   #include "../repl.h"

   char *ik_mail_notification_build_message(TALLOC_CTX *ctx, size_t count)
   {
       (void)ctx;
       (void)count;
       return NULL;
   }

   bool ik_mail_notification_should_inject(const ik_agent_ctx_t *agent)
   {
       (void)agent;
       return false;
   }

   void ik_repl_check_mail_notification(ik_repl_ctx_t *repl)
   {
       (void)repl;
   }
   ```

5. Update Makefile:
   - Add `src/mail/notification.c` to `MODULE_SOURCES`
   - Verify `tests/unit/mail/notification_test.c` is picked up by wildcard
   - Verify `tests/unit/mail/notification_inject_test.c` is picked up by wildcard

6. Run `make check` - expect test failures (stubs don't implement logic)

### Green
1. Implement `src/mail/notification.c`:
   ```c
   #include "notification.h"

   #include "inbox.h"

   #include "../agent.h"
   #include "../event_render.h"
   #include "../openai/client.h"
   #include "../panic.h"
   #include "../repl.h"

   #include <assert.h>
   #include <stdio.h>
   #include <string.h>
   #include <talloc.h>

   char *ik_mail_notification_build_message(TALLOC_CTX *ctx, size_t count)
   {
       assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

       // Defensive: return NULL for zero count
       if (count == 0) {
           return NULL;
       }

       // Build message with correct singular/plural grammar
       const char *plural_suffix = (count == 1) ? "" : "s";

       char *msg = talloc_asprintf(ctx,
           "[Notification: You have %zu unread message%s in your inbox]",
           count, plural_suffix);

       if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       return msg;
   }

   bool ik_mail_notification_should_inject(const ik_agent_ctx_t *agent)
   {
       // Null checks
       if (agent == NULL) {
           return false;
       }

       if (agent->inbox == NULL) {
           return false;
       }

       // Check conditions
       if (agent->inbox->unread_count == 0) {
           return false;
       }

       if (agent->mail_notification_pending) {
           return false;
       }

       return true;
   }

   void ik_repl_check_mail_notification(ik_repl_ctx_t *repl)
   {
       // Null safety
       if (repl == NULL) {
           return;
       }

       ik_agent_ctx_t *agent = repl->current_agent;

       // Check if notification should be injected
       if (!ik_mail_notification_should_inject(agent)) {
           return;
       }

       // Build notification message
       char *notification_text = ik_mail_notification_build_message(
           repl, agent->inbox->unread_count);

       if (notification_text == NULL) {  // LCOV_EXCL_BR_LINE
           return;  // LCOV_EXCL_LINE (count was 0, should_inject would have failed)
       }

       // Add to conversation as user message (for LLM context)
       if (repl->conversation != NULL) {
           res_t msg_res = ik_openai_msg_create(repl->conversation, "user", notification_text);
           if (is_ok(&msg_res)) {
               ik_msg_t *msg = msg_res.val;
               ik_openai_conversation_add_msg(repl->conversation, msg);
           }
           // Note: If msg creation fails, continue to scrollback (best effort)
       }

       // Render to scrollback (for user visibility)
       if (repl->scrollback != NULL) {
           // Use "notification" kind for dim styling
           ik_event_render(repl->scrollback, "notification", notification_text, NULL);
       }

       // Set pending flag to prevent repeat notification
       agent->mail_notification_pending = true;

       // Cleanup
       talloc_free(notification_text);
   }
   ```

2. Update `src/event_render.c` to handle "notification" kind:
   ```c
   // Add to the kind handling switch/if chain:

   // Notification events (mail notifications)
   if (strcmp(kind, "notification") == 0) {
       if (content == NULL || strlen(content) == 0) {
           return OK(NULL);
       }

       // Render with dim styling
       // Format: \033[2m{content}\033[0m (dim on/off)
       size_t content_len = strlen(content);
       size_t styled_len = content_len + 8;  // \033[2m + \033[0m
       char *styled = talloc_size(scrollback, styled_len + 1);
       if (styled == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       snprintf(styled, styled_len + 1, "\033[2m%s\033[0m", content);

       res_t append_res = ik_scrollback_append_line(scrollback, styled, strlen(styled));
       talloc_free(styled);

       return append_res;
   }
   ```

3. Update `src/event_render.h` to document "notification" kind:
   ```c
   /**
    * Render an event to scrollback buffer
    *
    * Universal renderer that handles all event types:
    * - "user": Render content as-is
    * - "assistant": Render content as-is
    * - "system": Render content as-is
    * - "notification": Render with dim styling (mail notifications)
    * - "mark": Render as "/mark LABEL" where LABEL from data_json
    * - "rewind": Render nothing (result shown elsewhere)
    * - "clear": Render nothing (clears scrollback)
    * ...
    */
   ```

4. Update `src/event_render.c` `ik_event_renders_visible()`:
   ```c
   bool ik_event_renders_visible(const char *kind)
   {
       if (kind == NULL) return false;

       // Add notification to visible events
       if (strcmp(kind, "notification") == 0) return true;

       // ... existing checks ...
   }
   ```

5. Update `src/repl.c` `ik_repl_transition_to_idle()`:
   ```c
   #include "mail/notification.h"  // Add to includes

   void ik_repl_transition_to_idle(ik_repl_ctx_t *repl)
   {
       assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

       // Update state with mutex protection for thread safety
       pthread_mutex_lock_(&repl->tool_thread_mutex);
       assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */
       repl->state = IK_REPL_STATE_IDLE;
       pthread_mutex_unlock_(&repl->tool_thread_mutex);

       // Hide spinner, show input
       repl->spinner_state.visible = false;
       repl->input_buffer_visible = true;

       // Check for mail notification injection
       ik_repl_check_mail_notification(repl);
   }
   ```

6. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`notification.h`)
   - Sibling headers next (`inbox.h`)
   - Project headers next (`../agent.h`, `../event_render.h`, `../openai/client.h`, `../panic.h`, `../repl.h`)
   - System headers last (`<assert.h>`, `<stdio.h>`, `<string.h>`, `<talloc.h>`)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments

4. Run `make lint` - verify clean

5. Run `make coverage` - verify 100% coverage on new code

6. Run `make check-valgrind` - verify no memory leaks

7. Consider: Should notification use a different event kind?
   - Current: "notification" with dim styling
   - Alternative: "user" kind with special prefix detection
   - Decision: Use dedicated "notification" kind for cleaner separation

8. Consider: Should notification text be configurable?
   - Current: Hardcoded format
   - Alternative: Config option for notification format
   - Decision: Hardcoded for v1, can add config later

9. Manual verification:
   - Start ikigai with debug mode
   - Send mail to current agent from another agent
   - Let agent complete a task and go IDLE
   - Verify notification appears in scrollback (dimmed)
   - Verify notification in conversation (agent can respond)
   - Verify second IDLE doesn't repeat notification
   - Check mail, then get more mail, verify new notification works

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on modified code in:
  - `src/mail/notification.c`
  - `src/event_render.c` (notification kind handling)
  - `src/repl.c` (transition hook)
- Notification injected when agent transitions to IDLE with unread mail
- Notification format:
  - Singular: `[Notification: You have 1 unread message in your inbox]`
  - Plural: `[Notification: You have N unread messages in your inbox]`
- Notification added to conversation as `role: user` message
- Notification rendered to scrollback with dim styling
- `mail_notification_pending` flag set after injection
- No notification when:
  - `unread_count == 0`
  - `mail_notification_pending == true`
  - `current_agent == NULL`
  - `inbox == NULL`
- Tests cover:
  - Message format (singular/plural grammar)
  - Should inject predicate (all conditions)
  - Debounce behavior (pending flag)
  - Conversation integration (message added, role, content)
  - Scrollback integration (renders, content)
  - Null safety (all null pointer cases)
  - Multiple calls (debounce, reset)
- src/mail/notification.h exists with declarations
- src/mail/notification.c exists with implementation
- event_render.c updated to handle "notification" kind
- repl.c updated to call notification check on IDLE transition

## Notes

### Notification Flow Sequence

```
User sends message
    |
    v
LLM processing (WAITING_FOR_LLM state)
    |
    v
LLM response complete (no tool calls)
    |
    v
ik_repl_transition_to_idle() called
    |
    v
ik_repl_check_mail_notification(repl)
    |
    +---> Check ik_mail_notification_should_inject(agent)
    |         |
    |         +---> agent != NULL?
    |         +---> agent->inbox != NULL?
    |         +---> agent->inbox->unread_count > 0?
    |         +---> !agent->mail_notification_pending?
    |
    +---> If true:
    |         |
    |         +---> Build notification message
    |         +---> Add to conversation (role: user)
    |         +---> Render to scrollback (kind: notification)
    |         +---> Set agent->mail_notification_pending = true
    |
    v
State is IDLE, input buffer visible
```

### Why User Role?

The notification uses `role: user` for the conversation message because:

1. **Agent response trigger**: The LLM expects user messages as input to respond to. A user role message prompts the agent to potentially act on the notification.

2. **Autonomous sub-agents**: Sub-agents with research-focused system prompts can autonomously check their inbox when they see this notification in their context.

3. **Clear attribution**: The `[Notification: ...]` prefix clearly marks it as system-generated, distinguishing it from actual human input while maintaining the user role for LLM interaction.

### Dim Styling Implementation

The dim styling uses ANSI escape code `\033[2m` (dim/faint):
- `\033[2m` - Enable dim mode
- `{content}` - Notification text
- `\033[0m` - Reset all attributes

This provides visual de-emphasis without completely hiding the notification. In terminals that don't support dim, the text appears normally (graceful degradation).

### Flag Reset Locations

The `mail_notification_pending` flag is cleared in these locations (already implemented in previous tasks):

1. **mail-tool-handler.md**: `exec_mail_inbox()` clears flag
2. **mail-tool-handler.md**: `exec_mail_read()` clears flag
3. **mail-cmd-register.md**: `/mail` command handler clears flag
4. **mail-cmd-register.md**: `/mail read` command handler clears flag

This ensures the flag is reset whenever the agent actually checks their mail, allowing a new notification on the next IDLE if more mail arrives.

### Integration with Tool Loop

The notification is only injected on the final IDLE transition, not during tool loops:

```
User: "Find all .c files"
    |
    v
LLM: Tool call (glob *.c)      <- State: WAITING_FOR_LLM -> EXECUTING_TOOL
    |
    v
Tool result                     <- State: EXECUTING_TOOL -> WAITING_FOR_LLM
    |
    v
LLM: "Found 47 .c files..."    <- State: WAITING_FOR_LLM
    |
    v
Response complete (finish_reason: stop)
    |
    v
ik_repl_transition_to_idle()   <- Notification check HERE
```

This ensures the notification appears after the agent has completed its work, not in the middle of a tool loop.

### Memory Ownership

```
repl (TALLOC_CTX)
  |
  +-> notification_text (temporary, freed after use)
  |
  +-> conversation
  |     |
  |     +-> messages[]
  |           |
  |           +-> notification message (owned by conversation)
  |
  +-> scrollback
        |
        +-> text_buffer (contains styled notification)
```

The notification text is allocated temporarily, copied into the conversation message and scrollback, then freed. The conversation and scrollback own their copies.

### Testing Strategy

Tests are organized into:

1. **Message format tests** (`notification_test.c`):
   - Singular/plural grammar
   - High counts
   - Zero count defensive check
   - Prefix/suffix format
   - Memory ownership

2. **Predicate tests** (`notification_test.c`):
   - All true conditions
   - Each false condition independently
   - Null pointer handling

3. **Debounce tests** (`notification_test.c`):
   - Initial flag state
   - Flag blocking subsequent injections
   - Flag reset allowing new injection
   - New mail while pending

4. **Integration tests** (`notification_inject_test.c`):
   - Conversation message addition
   - Scrollback rendering
   - Pending flag updates
   - Null safety for all repl fields
   - Multiple call scenarios

### Relationship to Other Tasks

```
mail-inbox-struct.md       (defines ik_inbox_t with unread_count)
        |
mail-agent-inbox.md        (adds inbox and mail_notification_pending to agent)
        |
mail-tool-handler.md       (clears mail_notification_pending on inbox/read)
        |
mail-separator-unread.md   (displays [mail:N] in separator)
        |
mail-notification-inject.md (this task - injects notification on IDLE)
        |
        v
(future) mail-notification-style.md (optional: enhanced notification styling)
```

### Error Handling Philosophy

The notification injection follows a "best effort" approach:

1. **Null checks fail silently**: If any required component is null, skip injection without error
2. **Partial success acceptable**: If conversation add fails but scrollback succeeds, continue
3. **Always set pending flag**: Even on partial success, set flag to prevent repeated attempts
4. **No user-visible errors**: Notification failure should not disrupt normal operation

This approach prioritizes stability over completeness - a missed notification is better than a crash or error message.

### Future Considerations

1. **Notification sound**: Could add terminal bell (`\a`) for audible notification
2. **Configurable format**: Allow customization of notification text via config
3. **Priority levels**: Urgent mail could trigger immediate notification, not just on IDLE
4. **Notification history**: Track notification timestamps for debugging/logging
5. **Dismissable notifications**: Allow user to dismiss/acknowledge notifications
