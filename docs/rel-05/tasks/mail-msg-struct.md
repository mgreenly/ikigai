# Task: Create ik_mail_msg_t Structure

## Target
Phase 3: Inter-Agent Mailboxes - Step 1 (Message value object)

Supports User Stories:
- 29 (send mail) - message structure for creating messages
- 30 (list inbox) - fields for display (id, from, preview, read status)
- 31 (read mail) - fields for full display (body, timestamp, from)
- 45 (mail persisted to DB) - fields map to database columns

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/context-struct.md

### Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (design document, especially Data Model section)
- docs/memory.md (talloc ownership patterns)

### Pre-read Source (patterns)
- src/msg.h (ik_msg_t structure - simple struct with string fields)
- src/db/message.h (ik_msg_create_tool_result - factory function pattern)
- src/db/message.c (talloc string allocation, PANIC on OOM)
- src/repl.h (ik_mark_t structure - simple value object pattern)

### Pre-read Tests (patterns)
- tests/unit/db/message_test.c (database message tests, fixture patterns)
- tests/unit/msg/msg_test.c (simple struct creation tests if exists)

## Pre-conditions
- `make check` passes
- Phase 2 (agent-spawned-sub-agents) complete
- No `src/mail/` directory exists yet
- No `ik_mail_msg_t` type exists yet

## Task
Create the `ik_mail_msg_t` structure as a value object representing a single mail message. This structure holds all fields needed for the mailbox system.

The message structure is a **value object** - it has no behavior, just data. Messages are owned by their containing inbox (future task), which owns the message via talloc parent relationship.

**Structure definition (from design doc):**
```c
typedef struct ik_mail_msg {
    int64_t id;                 // Unique message ID (from database)
    char *from_agent_id;        // Sender: "0/", "1/", "0/0", etc.
    char *to_agent_id;          // Recipient: "1/", "0/", etc.
    char *body;                 // Message content (never NULL, may be empty)
    int64_t timestamp;          // Unix timestamp (seconds since epoch)
    bool read;                  // Has recipient read it?
} ik_mail_msg_t;
```

**Factory function:**
```c
// Create a mail message (value object)
// parent: talloc context (typically inbox or test context)
// id: unique message ID (0 for new unsaved messages)
// from_agent_id: sender agent ID string
// to_agent_id: recipient agent ID string
// body: message content
// timestamp: Unix timestamp
// read: read status
res_t ik_mail_msg_create(TALLOC_CTX *parent,
                          int64_t id,
                          const char *from_agent_id,
                          const char *to_agent_id,
                          const char *body,
                          int64_t timestamp,
                          bool read,
                          ik_mail_msg_t **out);
```

**Key design decisions:**
- No threading/replies in v1 (flat messages only)
- Timestamps are Unix epoch integers (int64_t)
- All strings owned by the message struct (talloc children)
- Message freed automatically when parent freed

## TDD Cycle

### Red
1. Create directory `src/mail/` for mail module

2. Create `src/mail/msg.h`:
   ```c
   #ifndef IK_MAIL_MSG_H
   #define IK_MAIL_MSG_H

   #include "../error.h"

   #include <stdbool.h>
   #include <stdint.h>
   #include <talloc.h>

   // Mail message value object
   // Represents a single message in the inter-agent mailbox system
   // Ownership: owned by parent context (typically inbox)
   typedef struct ik_mail_msg {
       int64_t id;              // Unique message ID (from database, 0 if unsaved)
       char *from_agent_id;     // Sender: "0/", "1/", "0/0", etc.
       char *to_agent_id;       // Recipient: "1/", "0/", etc.
       char *body;              // Message content
       int64_t timestamp;       // Unix timestamp (seconds since epoch)
       bool read;               // Has recipient read this message?
   } ik_mail_msg_t;

   // Create a mail message
   //
   // Allocates message struct and copies all string fields.
   // All strings become children of the message (freed with message).
   //
   // @param parent        Talloc context (message owner)
   // @param id            Message ID (0 for new unsaved messages)
   // @param from_agent_id Sender agent ID (must not be NULL)
   // @param to_agent_id   Recipient agent ID (must not be NULL)
   // @param body          Message body (must not be NULL, empty string OK)
   // @param timestamp     Unix timestamp
   // @param read          Read status
   // @param out           Receives allocated message
   // @return              OK on success, message also returned via out
   res_t ik_mail_msg_create(TALLOC_CTX *parent,
                             int64_t id,
                             const char *from_agent_id,
                             const char *to_agent_id,
                             const char *body,
                             int64_t timestamp,
                             bool read,
                             ik_mail_msg_t **out);

   #endif // IK_MAIL_MSG_H
   ```

3. Create `tests/unit/mail/msg_test.c`:
   ```c
   #include "../../../src/mail/msg.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // Test fixture
   static TALLOC_CTX *ctx;

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
   }

   // Test: Create message with all fields
   START_TEST(test_mail_msg_create_basic)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 42, "0/", "1/", "Hello", 1700000000, false, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(msg);
       ck_assert_int_eq(msg->id, 42);
       ck_assert_str_eq(msg->from_agent_id, "0/");
       ck_assert_str_eq(msg->to_agent_id, "1/");
       ck_assert_str_eq(msg->body, "Hello");
       ck_assert_int_eq(msg->timestamp, 1700000000);
       ck_assert(!msg->read);
   }
   END_TEST

   // Test: Create message with read=true
   START_TEST(test_mail_msg_create_read_flag)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 1, "1/", "0/", "Reply", 1700000001, true, &msg);

       ck_assert(is_ok(&res));
       ck_assert(msg->read);
   }
   END_TEST

   // Test: Create message with empty body (allowed)
   START_TEST(test_mail_msg_create_empty_body)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 0, "0/", "1/", "", 1700000000, false, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(msg);
       ck_assert_str_eq(msg->body, "");
   }
   END_TEST

   // Test: Create message with id=0 (new unsaved message)
   START_TEST(test_mail_msg_create_zero_id)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 0, "0/", "1/", "New message", 1700000000, false, &msg);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(msg->id, 0);
   }
   END_TEST

   // Test: Create message with sub-agent IDs
   START_TEST(test_mail_msg_create_subagent_ids)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 5, "0/0", "0/", "Sub-agent report", 1700000000, false, &msg);

       ck_assert(is_ok(&res));
       ck_assert_str_eq(msg->from_agent_id, "0/0");
       ck_assert_str_eq(msg->to_agent_id, "0/");
   }
   END_TEST

   // Test: Message is child of provided parent (talloc ownership)
   START_TEST(test_mail_msg_talloc_ownership)
   {
       TALLOC_CTX *parent = talloc_new(ctx);
       ck_assert_ptr_nonnull(parent);

       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(parent, 1, "0/", "1/", "Test", 1700000000, false, &msg);
       ck_assert(is_ok(&res));

       // Verify msg is child of parent
       ck_assert_ptr_eq(talloc_parent(msg), parent);

       // Free parent should free msg (no crash, no leak)
       talloc_free(parent);
   }
   END_TEST

   // Test: String fields are owned by message (talloc children)
   START_TEST(test_mail_msg_string_ownership)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 1, "0/", "1/", "Body text", 1700000000, false, &msg);
       ck_assert(is_ok(&res));

       // Verify strings are children of msg
       ck_assert_ptr_eq(talloc_parent(msg->from_agent_id), msg);
       ck_assert_ptr_eq(talloc_parent(msg->to_agent_id), msg);
       ck_assert_ptr_eq(talloc_parent(msg->body), msg);
   }
   END_TEST

   // Test: Strings are copied (modification of original doesn't affect message)
   START_TEST(test_mail_msg_strings_copied)
   {
       char from[] = "0/";
       char to[] = "1/";
       char body[] = "Original";

       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 1, from, to, body, 1700000000, false, &msg);
       ck_assert(is_ok(&res));

       // Modify originals
       from[0] = 'X';
       to[0] = 'Y';
       body[0] = 'Z';

       // Message strings should be unchanged
       ck_assert_str_eq(msg->from_agent_id, "0/");
       ck_assert_str_eq(msg->to_agent_id, "1/");
       ck_assert_str_eq(msg->body, "Original");
   }
   END_TEST

   // Test: Large message body
   START_TEST(test_mail_msg_large_body)
   {
       // Create a 10KB body
       char *large_body = talloc_array(ctx, char, 10240);
       ck_assert_ptr_nonnull(large_body);
       memset(large_body, 'A', 10239);
       large_body[10239] = '\0';

       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 1, "0/", "1/", large_body, 1700000000, false, &msg);

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(strlen(msg->body), 10239);
   }
   END_TEST

   // Suite configuration
   static Suite *mail_msg_suite(void)
   {
       Suite *s = suite_create("MailMsg");

       TCase *tc_create = tcase_create("Create");
       tcase_add_checked_fixture(tc_create, setup, teardown);
       tcase_add_test(tc_create, test_mail_msg_create_basic);
       tcase_add_test(tc_create, test_mail_msg_create_read_flag);
       tcase_add_test(tc_create, test_mail_msg_create_empty_body);
       tcase_add_test(tc_create, test_mail_msg_create_zero_id);
       tcase_add_test(tc_create, test_mail_msg_create_subagent_ids);
       suite_add_tcase(s, tc_create);

       TCase *tc_ownership = tcase_create("Ownership");
       tcase_add_checked_fixture(tc_ownership, setup, teardown);
       tcase_add_test(tc_ownership, test_mail_msg_talloc_ownership);
       tcase_add_test(tc_ownership, test_mail_msg_string_ownership);
       tcase_add_test(tc_ownership, test_mail_msg_strings_copied);
       tcase_add_test(tc_ownership, test_mail_msg_large_body);
       suite_add_tcase(s, tc_ownership);

       return s;
   }

   int main(void)
   {
       int number_failed;
       Suite *s = mail_msg_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

4. Create stub `src/mail/msg.c`:
   ```c
   #include "msg.h"

   res_t ik_mail_msg_create(TALLOC_CTX *parent,
                             int64_t id,
                             const char *from_agent_id,
                             const char *to_agent_id,
                             const char *body,
                             int64_t timestamp,
                             bool read,
                             ik_mail_msg_t **out)
   {
       (void)parent;
       (void)id;
       (void)from_agent_id;
       (void)to_agent_id;
       (void)body;
       (void)timestamp;
       (void)read;
       (void)out;
       return OK(NULL);
   }
   ```

5. Update Makefile:
   - Add `src/mail/msg.c` to `MODULE_SOURCES`
   - Verify `tests/unit/mail/msg_test.c` is picked up by wildcard

6. Run `make check` - expect test failures (stub returns NULL)

### Green
1. Implement `src/mail/msg.c`:
   ```c
   #include "msg.h"

   #include "../panic.h"
   #include "../wrapper.h"

   #include <assert.h>

   res_t ik_mail_msg_create(TALLOC_CTX *parent,
                             int64_t id,
                             const char *from_agent_id,
                             const char *to_agent_id,
                             const char *body,
                             int64_t timestamp,
                             bool read,
                             ik_mail_msg_t **out)
   {
       assert(parent != NULL);        // LCOV_EXCL_BR_LINE
       assert(from_agent_id != NULL); // LCOV_EXCL_BR_LINE
       assert(to_agent_id != NULL);   // LCOV_EXCL_BR_LINE
       assert(body != NULL);          // LCOV_EXCL_BR_LINE
       assert(out != NULL);           // LCOV_EXCL_BR_LINE

       ik_mail_msg_t *msg = talloc_zero_(parent, sizeof(ik_mail_msg_t));
       if (msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       msg->id = id;
       msg->timestamp = timestamp;
       msg->read = read;

       msg->from_agent_id = talloc_strdup(msg, from_agent_id);
       if (msg->from_agent_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       msg->to_agent_id = talloc_strdup(msg, to_agent_id);
       if (msg->to_agent_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       msg->body = talloc_strdup(msg, body);
       if (msg->body == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       *out = msg;
       return OK(msg);
   }
   ```

2. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide (own header first, then project, then system)
2. Verify `// comments` style used (not `/* */`)
3. Verify `int64_t` used for numeric types (not `long`)
4. Run `make lint` - verify clean
5. Run `make coverage` - verify 100% coverage on new code
6. Verify no memory leaks with valgrind: `make check-valgrind`

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on `src/mail/msg.c`
- `ik_mail_msg_t` struct defined with all required fields
- `ik_mail_msg_create()` factory function implemented
- Test file with comprehensive coverage:
  - Basic creation
  - All field values verified
  - Talloc ownership verified
  - String copying verified
  - Edge cases (empty body, zero id, large body)
- Directory structure created: `src/mail/`, `tests/unit/mail/`
- No changes to existing code (pure addition)
