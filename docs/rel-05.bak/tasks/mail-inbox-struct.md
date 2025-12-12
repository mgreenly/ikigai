# Task: Create ik_inbox_t Structure with Operations

## Target
Phase 3: Inter-Agent Mailboxes - Step 2 (Inbox container with operations)

Supports User Stories:
- 30 (list inbox) - needs messages array, sorted retrieval (unread first)
- 32 (inbox empty) - needs count == 0 handling
- 35 (separator shows unread) - needs unread_count for display
- 36 (hides when zero) - needs unread_count == 0 check

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/arena-allocator.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (design document, especially Data Model section)
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md

## Pre-read Source (patterns)
- src/mail/msg.h (ik_mail_msg_t structure - message contained in inbox)
- src/mail/msg.c (factory function pattern)
- src/history.h (ik_history_t - similar container with array management)
- src/history.c (array growth, talloc ownership patterns)
- src/marks.c (talloc_realloc pattern for array growth)
- src/repl.h (ik_mark_t **marks, size_t mark_count pattern)

## Pre-read Tests (patterns)
- tests/unit/mail/msg_test.c (mail message tests, fixture pattern)
- tests/unit/history/core_test.c (container operation tests, comprehensive coverage)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- `ik_mail_msg_t` struct defined with fields: id, from_agent_id, to_agent_id, body, timestamp, read
- `ik_mail_msg_create()` factory function implemented in src/mail/msg.h and msg.c
- Test file at tests/unit/mail/msg_test.c
- Directory structure exists: `src/mail/`, `tests/unit/mail/`

## Task
Create the `ik_inbox_t` structure as a container for mail messages with operations for managing the inbox. The inbox is a dynamic array of messages with a cached unread count for efficient status bar display.

**Structure definition (from design doc):**
```c
typedef struct ik_inbox {
    ik_mail_msg_t **messages;   // Array of message pointers
    size_t count;               // Total messages in inbox
    size_t unread_count;        // Cached count of unread messages
    size_t capacity;            // Current allocated capacity
} ik_inbox_t;
```

**Operations:**
1. `ik_inbox_create()` - Create empty inbox
2. `ik_inbox_add()` - Add message (updates unread_count if !read)
3. `ik_inbox_get_all()` - Get all messages sorted (unread first, then by timestamp desc)
4. `ik_inbox_get_by_id()` - Find message by ID
5. `ik_inbox_mark_read()` - Mark message as read (decrements unread_count)

**Key design decisions:**
- Inbox owns messages (talloc parent relationship)
- Messages stored in flat array, sorted on retrieval (not on insert)
- unread_count cached (not recalculated each time)
- get_all returns pointer to internal array (caller doesn't own, valid until next modification)
- mark_read updates both message flag and cached count atomically
- Initial capacity of 16, doubles when full (similar to history pattern)

**Sorting requirement (from user story 30):**
```
Handler sorts messages: unread first, then by timestamp descending
```

## TDD Cycle

### Red
1. Create `src/mail/inbox.h`:
   ```c
   #ifndef IK_MAIL_INBOX_H
   #define IK_MAIL_INBOX_H

   #include "../error.h"
   #include "msg.h"

   #include <stddef.h>
   #include <stdint.h>
   #include <talloc.h>

   // Mail inbox container
   // Owns all contained messages via talloc parent relationship
   // Maintains cached unread_count for efficient status bar display
   typedef struct ik_inbox {
       ik_mail_msg_t **messages;   // Array of message pointers (talloc'd)
       size_t count;               // Total messages in inbox
       size_t unread_count;        // Cached count of unread messages
       size_t capacity;            // Current allocated capacity
   } ik_inbox_t;

   // Create an empty inbox
   //
   // Allocates inbox structure with initial capacity.
   // Inbox will grow automatically when messages are added.
   //
   // @param parent  Talloc context (inbox owner)
   // @return        Pointer to allocated inbox (never NULL - PANICs on OOM)
   //
   // Assertions:
   // - parent must not be NULL
   ik_inbox_t *ik_inbox_create(TALLOC_CTX *parent);

   // Add a message to the inbox
   //
   // Takes ownership of message (reparents to inbox).
   // Updates unread_count if message is unread.
   // Grows array if at capacity.
   //
   // @param inbox   Inbox to add message to
   // @param msg     Message to add (ownership transferred to inbox)
   // @return        OK(NULL) on success
   //
   // Assertions:
   // - inbox must not be NULL
   // - msg must not be NULL
   res_t ik_inbox_add(ik_inbox_t *inbox, ik_mail_msg_t *msg);

   // Get all messages sorted for display
   //
   // Returns messages sorted: unread first, then by timestamp descending.
   // Returned array is internal - valid until next inbox modification.
   // Caller must not free or modify the returned array.
   //
   // @param inbox      Inbox to get messages from
   // @param count_out  Receives number of messages (may be NULL)
   // @return           Pointer to internal sorted array, or NULL if empty
   //
   // Assertions:
   // - inbox must not be NULL
   ik_mail_msg_t **ik_inbox_get_all(ik_inbox_t *inbox, size_t *count_out);

   // Find a message by ID
   //
   // Searches inbox for message with matching ID.
   //
   // @param inbox  Inbox to search
   // @param id     Message ID to find
   // @return       Pointer to message, or NULL if not found
   //
   // Assertions:
   // - inbox must not be NULL
   ik_mail_msg_t *ik_inbox_get_by_id(const ik_inbox_t *inbox, int64_t id);

   // Mark a message as read
   //
   // Sets message read flag to true and decrements unread_count.
   // No-op if message is already read.
   //
   // @param inbox  Inbox containing message
   // @param msg    Message to mark as read
   // @return       OK(NULL) on success, ERR if message not in inbox
   //
   // Assertions:
   // - inbox must not be NULL
   // - msg must not be NULL
   res_t ik_inbox_mark_read(ik_inbox_t *inbox, ik_mail_msg_t *msg);

   #endif // IK_MAIL_INBOX_H
   ```

2. Create `tests/unit/mail/inbox_test.c`:
   ```c
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // Test fixture
   static TALLOC_CTX *ctx;
   static ik_inbox_t *inbox;

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);
       inbox = NULL;
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
   }

   // Helper to create a test message
   static ik_mail_msg_t *make_msg(int64_t id, const char *from, const char *to,
                                   const char *body, int64_t ts, bool read)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, id, from, to, body, ts, read, &msg);
       ck_assert(is_ok(&res));
       return msg;
   }

   // ============================================================
   // Create tests
   // ============================================================

   START_TEST(test_inbox_create_basic)
   {
       inbox = ik_inbox_create(ctx);

       ck_assert_ptr_nonnull(inbox);
       ck_assert_uint_eq(inbox->count, 0);
       ck_assert_uint_eq(inbox->unread_count, 0);
       ck_assert_uint_gt(inbox->capacity, 0);
       ck_assert_ptr_nonnull(inbox->messages);
   }
   END_TEST

   START_TEST(test_inbox_talloc_ownership)
   {
       TALLOC_CTX *parent = talloc_new(ctx);
       ck_assert_ptr_nonnull(parent);

       inbox = ik_inbox_create(parent);
       ck_assert_ptr_eq(talloc_parent(inbox), parent);

       // Free parent should free inbox (no crash, no leak)
       talloc_free(parent);
   }
   END_TEST

   // ============================================================
   // Add tests
   // ============================================================

   START_TEST(test_inbox_add_single_unread)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg = make_msg(1, "0/", "1/", "Hello", 1700000000, false);

       res_t res = ik_inbox_add(inbox, msg);

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(inbox->count, 1);
       ck_assert_uint_eq(inbox->unread_count, 1);
   }
   END_TEST

   START_TEST(test_inbox_add_single_read)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg = make_msg(1, "0/", "1/", "Hello", 1700000000, true);

       res_t res = ik_inbox_add(inbox, msg);

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(inbox->count, 1);
       ck_assert_uint_eq(inbox->unread_count, 0);
   }
   END_TEST

   START_TEST(test_inbox_add_multiple_mixed)
   {
       inbox = ik_inbox_create(ctx);

       res_t res = ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Msg1", 1700000001, false));
       ck_assert(is_ok(&res));

       res = ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Msg2", 1700000002, true));
       ck_assert(is_ok(&res));

       res = ik_inbox_add(inbox, make_msg(3, "0/", "1/", "Msg3", 1700000003, false));
       ck_assert(is_ok(&res));

       ck_assert_uint_eq(inbox->count, 3);
       ck_assert_uint_eq(inbox->unread_count, 2);
   }
   END_TEST

   START_TEST(test_inbox_add_reparents_message)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg = make_msg(1, "0/", "1/", "Hello", 1700000000, false);

       res_t res = ik_inbox_add(inbox, msg);
       ck_assert(is_ok(&res));

       // Message should now be child of inbox (via messages array)
       // The message array owns the messages
       ck_assert_ptr_eq(talloc_parent(msg), inbox->messages);
   }
   END_TEST

   START_TEST(test_inbox_add_grows_capacity)
   {
       inbox = ik_inbox_create(ctx);
       size_t initial_capacity = inbox->capacity;

       // Add more messages than initial capacity
       for (size_t i = 0; i < initial_capacity + 5; i++) {
           ik_mail_msg_t *msg = make_msg((int64_t)i, "0/", "1/", "Test", 1700000000 + (int64_t)i, false);
           res_t res = ik_inbox_add(inbox, msg);
           ck_assert(is_ok(&res));
       }

       ck_assert_uint_eq(inbox->count, initial_capacity + 5);
       ck_assert_uint_gt(inbox->capacity, initial_capacity);
   }
   END_TEST

   // ============================================================
   // Get all (sorted) tests
   // ============================================================

   START_TEST(test_inbox_get_all_empty)
   {
       inbox = ik_inbox_create(ctx);

       size_t count = 99;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);

       ck_assert_ptr_null(msgs);
       ck_assert_uint_eq(count, 0);
   }
   END_TEST

   START_TEST(test_inbox_get_all_single)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Hello", 1700000000, false));

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);

       ck_assert_ptr_nonnull(msgs);
       ck_assert_uint_eq(count, 1);
       ck_assert_int_eq(msgs[0]->id, 1);
   }
   END_TEST

   START_TEST(test_inbox_get_all_sorted_unread_first)
   {
       inbox = ik_inbox_create(ctx);

       // Add in order: read, unread, read, unread
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Read1", 1700000001, true));
       ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Unread1", 1700000002, false));
       ik_inbox_add(inbox, make_msg(3, "0/", "1/", "Read2", 1700000003, true));
       ik_inbox_add(inbox, make_msg(4, "0/", "1/", "Unread2", 1700000004, false));

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);

       ck_assert_uint_eq(count, 4);
       // Unread messages should come first
       ck_assert(!msgs[0]->read);
       ck_assert(!msgs[1]->read);
       // Read messages should come after
       ck_assert(msgs[2]->read);
       ck_assert(msgs[3]->read);
   }
   END_TEST

   START_TEST(test_inbox_get_all_sorted_by_timestamp_desc)
   {
       inbox = ik_inbox_create(ctx);

       // Add messages with different timestamps (not in order)
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Oldest", 1700000001, false));
       ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Newest", 1700000003, false));
       ik_inbox_add(inbox, make_msg(3, "0/", "1/", "Middle", 1700000002, false));

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);

       ck_assert_uint_eq(count, 3);
       // Should be sorted by timestamp descending
       ck_assert_int_eq(msgs[0]->id, 2);  // Newest (ts 3)
       ck_assert_int_eq(msgs[1]->id, 3);  // Middle (ts 2)
       ck_assert_int_eq(msgs[2]->id, 1);  // Oldest (ts 1)
   }
   END_TEST

   START_TEST(test_inbox_get_all_sorted_combined)
   {
       inbox = ik_inbox_create(ctx);

       // Mix of read/unread with various timestamps
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Read-Old", 1700000001, true));
       ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Unread-Old", 1700000002, false));
       ik_inbox_add(inbox, make_msg(3, "0/", "1/", "Read-New", 1700000004, true));
       ik_inbox_add(inbox, make_msg(4, "0/", "1/", "Unread-New", 1700000003, false));

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);

       ck_assert_uint_eq(count, 4);
       // Unread first, sorted by timestamp desc
       ck_assert_int_eq(msgs[0]->id, 4);  // Unread-New (ts 3)
       ck_assert_int_eq(msgs[1]->id, 2);  // Unread-Old (ts 2)
       // Read second, sorted by timestamp desc
       ck_assert_int_eq(msgs[2]->id, 3);  // Read-New (ts 4)
       ck_assert_int_eq(msgs[3]->id, 1);  // Read-Old (ts 1)
   }
   END_TEST

   START_TEST(test_inbox_get_all_null_count)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Hello", 1700000000, false));

       // count_out can be NULL
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, NULL);

       ck_assert_ptr_nonnull(msgs);
   }
   END_TEST

   // ============================================================
   // Get by ID tests
   // ============================================================

   START_TEST(test_inbox_get_by_id_found)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(10, "0/", "1/", "Msg10", 1700000001, false));
       ik_inbox_add(inbox, make_msg(20, "0/", "1/", "Msg20", 1700000002, false));
       ik_inbox_add(inbox, make_msg(30, "0/", "1/", "Msg30", 1700000003, false));

       ik_mail_msg_t *found = ik_inbox_get_by_id(inbox, 20);

       ck_assert_ptr_nonnull(found);
       ck_assert_int_eq(found->id, 20);
       ck_assert_str_eq(found->body, "Msg20");
   }
   END_TEST

   START_TEST(test_inbox_get_by_id_not_found)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Msg1", 1700000001, false));

       ik_mail_msg_t *found = ik_inbox_get_by_id(inbox, 999);

       ck_assert_ptr_null(found);
   }
   END_TEST

   START_TEST(test_inbox_get_by_id_empty_inbox)
   {
       inbox = ik_inbox_create(ctx);

       ik_mail_msg_t *found = ik_inbox_get_by_id(inbox, 1);

       ck_assert_ptr_null(found);
   }
   END_TEST

   START_TEST(test_inbox_get_by_id_first_match)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(5, "0/", "1/", "First", 1700000001, false));
       ik_inbox_add(inbox, make_msg(5, "0/", "1/", "Second", 1700000002, false));  // Duplicate ID

       ik_mail_msg_t *found = ik_inbox_get_by_id(inbox, 5);

       ck_assert_ptr_nonnull(found);
       ck_assert_str_eq(found->body, "First");
   }
   END_TEST

   // ============================================================
   // Mark read tests
   // ============================================================

   START_TEST(test_inbox_mark_read_basic)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg = make_msg(1, "0/", "1/", "Hello", 1700000000, false);
       ik_inbox_add(inbox, msg);

       ck_assert_uint_eq(inbox->unread_count, 1);
       ck_assert(!msg->read);

       res_t res = ik_inbox_mark_read(inbox, msg);

       ck_assert(is_ok(&res));
       ck_assert(msg->read);
       ck_assert_uint_eq(inbox->unread_count, 0);
   }
   END_TEST

   START_TEST(test_inbox_mark_read_already_read)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg = make_msg(1, "0/", "1/", "Hello", 1700000000, true);
       ik_inbox_add(inbox, msg);

       ck_assert_uint_eq(inbox->unread_count, 0);

       res_t res = ik_inbox_mark_read(inbox, msg);

       ck_assert(is_ok(&res));
       ck_assert(msg->read);
       ck_assert_uint_eq(inbox->unread_count, 0);  // Should not go negative
   }
   END_TEST

   START_TEST(test_inbox_mark_read_multiple)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg1 = make_msg(1, "0/", "1/", "Msg1", 1700000001, false);
       ik_mail_msg_t *msg2 = make_msg(2, "0/", "1/", "Msg2", 1700000002, false);
       ik_mail_msg_t *msg3 = make_msg(3, "0/", "1/", "Msg3", 1700000003, false);
       ik_inbox_add(inbox, msg1);
       ik_inbox_add(inbox, msg2);
       ik_inbox_add(inbox, msg3);

       ck_assert_uint_eq(inbox->unread_count, 3);

       ik_inbox_mark_read(inbox, msg2);
       ck_assert_uint_eq(inbox->unread_count, 2);

       ik_inbox_mark_read(inbox, msg1);
       ck_assert_uint_eq(inbox->unread_count, 1);

       ik_inbox_mark_read(inbox, msg3);
       ck_assert_uint_eq(inbox->unread_count, 0);
   }
   END_TEST

   START_TEST(test_inbox_mark_read_not_in_inbox)
   {
       inbox = ik_inbox_create(ctx);
       ik_mail_msg_t *msg_in = make_msg(1, "0/", "1/", "In inbox", 1700000000, false);
       ik_mail_msg_t *msg_out = make_msg(2, "0/", "1/", "Not in inbox", 1700000001, false);
       ik_inbox_add(inbox, msg_in);

       res_t res = ik_inbox_mark_read(inbox, msg_out);

       ck_assert(is_err(&res));
       ck_assert(!msg_out->read);  // Should not be modified
   }
   END_TEST

   // ============================================================
   // Edge case tests
   // ============================================================

   START_TEST(test_inbox_all_read)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Msg1", 1700000001, true));
       ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Msg2", 1700000002, true));

       ck_assert_uint_eq(inbox->count, 2);
       ck_assert_uint_eq(inbox->unread_count, 0);

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);
       ck_assert_uint_eq(count, 2);
       // All read, should still be sorted by timestamp desc
       ck_assert_int_eq(msgs[0]->id, 2);
       ck_assert_int_eq(msgs[1]->id, 1);
   }
   END_TEST

   START_TEST(test_inbox_all_unread)
   {
       inbox = ik_inbox_create(ctx);
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "Msg1", 1700000001, false));
       ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Msg2", 1700000002, false));

       ck_assert_uint_eq(inbox->count, 2);
       ck_assert_uint_eq(inbox->unread_count, 2);

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);
       ck_assert_uint_eq(count, 2);
       // All unread, should be sorted by timestamp desc
       ck_assert_int_eq(msgs[0]->id, 2);
       ck_assert_int_eq(msgs[1]->id, 1);
   }
   END_TEST

   START_TEST(test_inbox_same_timestamp)
   {
       inbox = ik_inbox_create(ctx);
       // Same timestamp - order should be stable (by insertion order or ID)
       ik_inbox_add(inbox, make_msg(1, "0/", "1/", "First", 1700000000, false));
       ik_inbox_add(inbox, make_msg(2, "0/", "1/", "Second", 1700000000, false));

       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(inbox, &count);
       ck_assert_uint_eq(count, 2);
       // With same timestamp, order is implementation-defined but should be consistent
       ck_assert(msgs[0]->id == 1 || msgs[0]->id == 2);
       ck_assert(msgs[1]->id == 1 || msgs[1]->id == 2);
       ck_assert(msgs[0]->id != msgs[1]->id);
   }
   END_TEST

   // ============================================================
   // Suite configuration
   // ============================================================

   static Suite *inbox_suite(void)
   {
       Suite *s = suite_create("MailInbox");

       TCase *tc_create = tcase_create("Create");
       tcase_add_checked_fixture(tc_create, setup, teardown);
       tcase_add_test(tc_create, test_inbox_create_basic);
       tcase_add_test(tc_create, test_inbox_talloc_ownership);
       suite_add_tcase(s, tc_create);

       TCase *tc_add = tcase_create("Add");
       tcase_add_checked_fixture(tc_add, setup, teardown);
       tcase_add_test(tc_add, test_inbox_add_single_unread);
       tcase_add_test(tc_add, test_inbox_add_single_read);
       tcase_add_test(tc_add, test_inbox_add_multiple_mixed);
       tcase_add_test(tc_add, test_inbox_add_reparents_message);
       tcase_add_test(tc_add, test_inbox_add_grows_capacity);
       suite_add_tcase(s, tc_add);

       TCase *tc_get_all = tcase_create("GetAll");
       tcase_add_checked_fixture(tc_get_all, setup, teardown);
       tcase_add_test(tc_get_all, test_inbox_get_all_empty);
       tcase_add_test(tc_get_all, test_inbox_get_all_single);
       tcase_add_test(tc_get_all, test_inbox_get_all_sorted_unread_first);
       tcase_add_test(tc_get_all, test_inbox_get_all_sorted_by_timestamp_desc);
       tcase_add_test(tc_get_all, test_inbox_get_all_sorted_combined);
       tcase_add_test(tc_get_all, test_inbox_get_all_null_count);
       suite_add_tcase(s, tc_get_all);

       TCase *tc_get_by_id = tcase_create("GetById");
       tcase_add_checked_fixture(tc_get_by_id, setup, teardown);
       tcase_add_test(tc_get_by_id, test_inbox_get_by_id_found);
       tcase_add_test(tc_get_by_id, test_inbox_get_by_id_not_found);
       tcase_add_test(tc_get_by_id, test_inbox_get_by_id_empty_inbox);
       tcase_add_test(tc_get_by_id, test_inbox_get_by_id_first_match);
       suite_add_tcase(s, tc_get_by_id);

       TCase *tc_mark_read = tcase_create("MarkRead");
       tcase_add_checked_fixture(tc_mark_read, setup, teardown);
       tcase_add_test(tc_mark_read, test_inbox_mark_read_basic);
       tcase_add_test(tc_mark_read, test_inbox_mark_read_already_read);
       tcase_add_test(tc_mark_read, test_inbox_mark_read_multiple);
       tcase_add_test(tc_mark_read, test_inbox_mark_read_not_in_inbox);
       suite_add_tcase(s, tc_mark_read);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_inbox_all_read);
       tcase_add_test(tc_edge, test_inbox_all_unread);
       tcase_add_test(tc_edge, test_inbox_same_timestamp);
       suite_add_tcase(s, tc_edge);

       return s;
   }

   int main(void)
   {
       int number_failed;
       Suite *s = inbox_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Create stub `src/mail/inbox.c`:
   ```c
   #include "inbox.h"

   ik_inbox_t *ik_inbox_create(TALLOC_CTX *parent)
   {
       (void)parent;
       return NULL;
   }

   res_t ik_inbox_add(ik_inbox_t *inbox, ik_mail_msg_t *msg)
   {
       (void)inbox;
       (void)msg;
       return OK(NULL);
   }

   ik_mail_msg_t **ik_inbox_get_all(ik_inbox_t *inbox, size_t *count_out)
   {
       (void)inbox;
       (void)count_out;
       return NULL;
   }

   ik_mail_msg_t *ik_inbox_get_by_id(const ik_inbox_t *inbox, int64_t id)
   {
       (void)inbox;
       (void)id;
       return NULL;
   }

   res_t ik_inbox_mark_read(ik_inbox_t *inbox, ik_mail_msg_t *msg)
   {
       (void)inbox;
       (void)msg;
       return OK(NULL);
   }
   ```

4. Update Makefile:
   - Add `src/mail/inbox.c` to `MODULE_SOURCES`
   - Verify `tests/unit/mail/inbox_test.c` is picked up by wildcard

5. Run `make check` - expect test failures (stubs return NULL/incorrect values)

### Green
1. Implement `src/mail/inbox.c`:
   ```c
   #include "inbox.h"

   #include "../panic.h"
   #include "../wrapper.h"

   #include <assert.h>
   #include <stdlib.h>
   #include <talloc.h>

   #define INBOX_INITIAL_CAPACITY 16

   ik_inbox_t *ik_inbox_create(TALLOC_CTX *parent)
   {
       assert(parent != NULL);  // LCOV_EXCL_BR_LINE

       ik_inbox_t *inbox = talloc_zero(parent, ik_inbox_t);
       if (inbox == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       inbox->count = 0;
       inbox->unread_count = 0;
       inbox->capacity = INBOX_INITIAL_CAPACITY;

       inbox->messages = talloc_array(inbox, ik_mail_msg_t *, (unsigned int)inbox->capacity);
       if (inbox->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       return inbox;
   }

   res_t ik_inbox_add(ik_inbox_t *inbox, ik_mail_msg_t *msg)
   {
       assert(inbox != NULL);  // LCOV_EXCL_BR_LINE
       assert(msg != NULL);    // LCOV_EXCL_BR_LINE

       // Grow array if needed
       if (inbox->count >= inbox->capacity) {
           size_t new_capacity = inbox->capacity * 2;
           ik_mail_msg_t **new_messages = talloc_realloc(inbox, inbox->messages,
                                                          ik_mail_msg_t *, (unsigned int)new_capacity);
           if (new_messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

           inbox->messages = new_messages;
           inbox->capacity = new_capacity;
       }

       // Take ownership of message
       talloc_steal(inbox->messages, msg);

       // Add to array
       inbox->messages[inbox->count] = msg;
       inbox->count++;

       // Update unread count
       if (!msg->read) {
           inbox->unread_count++;
       }

       return OK(NULL);
   }

   // Comparison function for qsort: unread first, then by timestamp descending
   static int compare_messages(const void *a, const void *b)
   {
       const ik_mail_msg_t *msg_a = *(const ik_mail_msg_t **)a;
       const ik_mail_msg_t *msg_b = *(const ik_mail_msg_t **)b;

       // Unread messages come first
       if (msg_a->read != msg_b->read) {
           return msg_a->read ? 1 : -1;
       }

       // Within same read status, sort by timestamp descending (newer first)
       if (msg_a->timestamp > msg_b->timestamp) return -1;
       if (msg_a->timestamp < msg_b->timestamp) return 1;
       return 0;
   }

   ik_mail_msg_t **ik_inbox_get_all(ik_inbox_t *inbox, size_t *count_out)
   {
       assert(inbox != NULL);  // LCOV_EXCL_BR_LINE

       if (count_out != NULL) {
           *count_out = inbox->count;
       }

       if (inbox->count == 0) {
           return NULL;
       }

       // Sort messages in place (modifies internal array)
       qsort_(inbox->messages, inbox->count, sizeof(ik_mail_msg_t *), compare_messages);

       return inbox->messages;
   }

   ik_mail_msg_t *ik_inbox_get_by_id(const ik_inbox_t *inbox, int64_t id)
   {
       assert(inbox != NULL);  // LCOV_EXCL_BR_LINE

       for (size_t i = 0; i < inbox->count; i++) {
           if (inbox->messages[i]->id == id) {
               return inbox->messages[i];
           }
       }

       return NULL;
   }

   res_t ik_inbox_mark_read(ik_inbox_t *inbox, ik_mail_msg_t *msg)
   {
       assert(inbox != NULL);  // LCOV_EXCL_BR_LINE
       assert(msg != NULL);    // LCOV_EXCL_BR_LINE

       // Verify message is in this inbox
       bool found = false;
       for (size_t i = 0; i < inbox->count; i++) {
           if (inbox->messages[i] == msg) {
               found = true;
               break;
           }
       }

       if (!found) {
           return ERR(inbox, INVALID_ARG, "Message not in inbox");
       }

       // Mark as read if not already
       if (!msg->read) {
           msg->read = true;
           inbox->unread_count--;
       }

       return OK(NULL);
   }
   ```

2. Add `qsort_` wrapper to `src/wrapper.h` (if not already present):
   ```c
   // qsort wrapper for mockability
   void qsort_(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *));
   ```

3. Add `qsort_` implementation to `src/wrapper.c` (if not already present):
   ```c
   #pragma weak qsort_
   void qsort_(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *))
   {
       qsort(base, nmemb, size, compar);
   }
   ```

4. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`inbox.h`)
   - Project headers next (`../panic.h`, `../wrapper.h`)
   - System headers last (`<assert.h>`, `<stdlib.h>`, `<talloc.h>`)

2. Verify `// comments` style used (not `/* */`)

3. Verify `int64_t` used for numeric types (not `long`)

4. Verify LCOV_EXCL_BR_LINE comments on assert statements

5. Run `make lint` - verify clean

6. Run `make coverage` - verify 100% coverage on new code

7. Run `make check-valgrind` - verify no memory leaks

8. Consider: Should `ik_inbox_get_all()` create a copy instead of sorting in place?
   - Current design: sorts in place for efficiency
   - Trade-off: Multiple calls may re-sort unnecessarily
   - Decision: Keep in-place for v1, optimize later if needed

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on `src/mail/inbox.c`
- `ik_inbox_t` struct defined with fields: messages, count, unread_count, capacity
- Operations implemented:
  - `ik_inbox_create()` - creates empty inbox with initial capacity
  - `ik_inbox_add()` - adds message, takes ownership, updates unread_count
  - `ik_inbox_get_all()` - returns sorted array (unread first, then timestamp desc)
  - `ik_inbox_get_by_id()` - finds message by ID
  - `ik_inbox_mark_read()` - marks message read, decrements unread_count
- Test file with comprehensive coverage:
  - Create tests (basic, ownership)
  - Add tests (unread, read, multiple, growth, reparenting)
  - Get all tests (empty, single, sorting variations)
  - Get by ID tests (found, not found, empty, duplicates)
  - Mark read tests (basic, already read, multiple, not in inbox)
  - Edge cases (all read, all unread, same timestamp)
- Directory structure: `src/mail/inbox.h`, `src/mail/inbox.c`
- No changes to existing code beyond Makefile and wrapper additions
- Working tree is clean (all changes committed)
