# Task: Mail Tool Execution Handler

## Target
Phase 3: Inter-Agent Mailboxes - Step 11 (Tool execution handler for LLM mail access)

Supports User Stories:
- 40 (agent checks inbox via tool) - Agent uses `mail` tool with `action: inbox`
- 41 (agent reads message via tool) - Agent uses `mail` tool with `action: read`
- 42 (agent sends mail via tool) - Agent uses `mail` tool with `action: send`

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/errors.md
- .agents/skills/mocking.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Agent Interface section - tool result JSON formats)
- docs/memory.md (talloc ownership patterns)
- docs/naming.md (ik_ prefix conventions)
- docs/return_values.md (res_t patterns, tool result conventions)
- docs/error_handling.md (error JSON format)

## Pre-read Source (patterns)
- src/tool.h (ik_tool_dispatch, ik_tool_arg_get_string, ik_tool_arg_get_int declarations)
- src/tool.c (existing tool dispatch patterns, JSON argument parsing, result building)
- src/mail/send.h (ik_mail_send() - high-level send operation)
- src/mail/list.h (ik_mail_list() - high-level list operation for command, different from tool)
- src/mail/read.h (ik_mail_read() - high-level read operation)
- src/mail/inbox.h (ik_inbox_t, ik_inbox_get_all() for inbox tool action)
- src/mail/msg.h (ik_mail_msg_t structure)
- src/agent.h (ik_agent_ctx_t - inbox field, mail_notification_pending flag)
- src/openai/client.c (yyjson JSON building patterns)
- src/config.c (yyjson nested object/array construction)
- src/wrapper.h (time_ wrapper, ISO 8601 timestamp formatting)

## Pre-read Tests (patterns)
- tests/unit/tool/tool_test.c (tool dispatch tests, mock patterns)
- tests/unit/tool/tool_dispatch_test.c (dispatcher test patterns if exists)
- tests/unit/mail/send_test.c (mail operation mocks)
- tests/unit/mail/read_test.c (mail operation mocks)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- `ik_tool_build_mail_schema()` implemented (from mail-tool-schema.md)
- Mail tool added to `ik_tool_build_all()` (returns 6 tools)
- Tool schema tests pass (verify mail schema structure)
- `ik_tool_dispatch()` exists and routes tools by name
- `ik_tool_arg_get_string()` and `ik_tool_arg_get_int()` utilities exist
- `ik_mail_send()` implemented for sending mail
- `ik_mail_read()` implemented for reading single messages
- `ik_inbox_get_all()` returns sorted messages for inbox listing
- `ik_agent_ctx_t` has `inbox` field and `mail_notification_pending` flag
- REPL context provides access to current agent via `repl->current_agent`

## Task
Implement `ik_tool_exec_mail()` function that handles mail tool execution for all three actions: inbox, read, and send. This function is called by the tool dispatcher when the LLM requests to use the mail tool.

**Function signature:**
```c
// Execute the mail tool with given action and parameters
//
// Parses the action from arguments JSON and dispatches to the
// appropriate handler. Returns a JSON result string in the format
// expected by the LLM for function calling.
//
// @param ctx       Talloc context for result allocation
// @param repl      REPL context (provides current_agent, db_ctx, etc.)
// @param arguments JSON string containing tool arguments
// @return          res_t with JSON result string in val on success
//
// Arguments JSON format:
// - {"action": "inbox"}
// - {"action": "read", "id": 5}
// - {"action": "send", "to": "1/", "body": "Message text"}
//
// Result formats (see backlog for full specification):
// - inbox: {"messages": [...], "unread_count": N}
// - read:  {"id": N, "from": "...", "timestamp": "...", "body": "..."}
// - send:  {"sent": true, "to": "...", "id": N}
// - error: {"error": "Error message"}
res_t ik_tool_exec_mail(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, const char *arguments);
```

**Tool Result Formats (from user stories):**

**User Story 40 - Inbox Action:**
```json
{
  "messages": [
    {"id": 5, "from": "1/", "unread": true, "preview": "Found 3 OAuth patterns..."},
    {"id": 4, "from": "2/", "unread": true, "preview": "Build complete, all..."}
  ],
  "unread_count": 2
}
```

**User Story 41 - Read Action:**
```json
{
  "id": 5,
  "from": "1/",
  "timestamp": "2024-01-15T10:30:00Z",
  "body": "Found 3 OAuth patterns worth considering..."
}
```

**User Story 42 - Send Action:**
```json
{
  "sent": true,
  "to": "0/",
  "id": 7
}
```

**Error Results:**
```json
{"error": "Agent 99/ not found"}
{"error": "Message body cannot be empty"}
{"error": "Message #5 not found"}
{"error": "Missing required parameter: action"}
{"error": "Unknown action: foo"}
{"error": "Missing required parameter: id"}
{"error": "Missing required parameter: to"}
{"error": "Missing required parameter: body"}
```

**Key Design Decisions:**

1. **Action Dispatch Pattern**: Parse `action` from arguments JSON first, then dispatch to action-specific handlers:
   - `exec_mail_inbox()` - List inbox messages
   - `exec_mail_read()` - Read single message by ID
   - `exec_mail_send()` - Send message to recipient

2. **Result Building**: Use yyjson_mut for building result JSON. All results are returned as a JSON string.

3. **Error Format**: All errors return `{"error": "message"}` format, consistent with other tools.

4. **Timestamp Format**: Use ISO 8601 format (`YYYY-MM-DDTHH:MM:SSZ`) for timestamps in read results.

5. **Preview Truncation**: For inbox listing, truncate body to first ~50 characters with "..." if needed (matching command output preview length).

6. **notification_pending Flag**: Clear this flag when executing inbox or read actions (agent is checking their mail).

7. **Parameter Validation Order**:
   - Check action is present
   - Check action is valid (inbox/read/send)
   - Check action-specific required parameters
   - Validate parameters (recipient exists, body non-empty, message exists)

**Integration with Tool Dispatcher:**

The dispatcher in `ik_tool_dispatch()` will be updated to route "mail" tool calls:

```c
// In ik_tool_dispatch():
if (strcmp(tool_name, "mail") == 0) {
    return ik_tool_exec_mail(ctx, repl, arguments);
}
```

## TDD Cycle

### Red
1. Update `src/tool.h` - Add declaration:
   ```c
   /**
    * Execute the mail tool.
    *
    * Handles mail tool actions: inbox, read, send.
    * Parses action from JSON arguments and dispatches to appropriate handler.
    *
    * @param ctx       Talloc context for result allocation
    * @param repl      REPL context (provides current_agent, db_ctx)
    * @param arguments JSON string containing action and parameters
    * @return          res_t with JSON result string on success
    *
    * Result formats:
    * - inbox: {"messages": [...], "unread_count": N}
    * - read:  {"id": N, "from": "...", "timestamp": "...", "body": "..."}
    * - send:  {"sent": true, "to": "...", "id": N}
    * - error: {"error": "message"}
    *
    * Error conditions:
    * - Missing action: {"error": "Missing required parameter: action"}
    * - Unknown action: {"error": "Unknown action: foo"}
    * - Invalid JSON: {"error": "Invalid JSON arguments"}
    * - Action-specific errors propagated
    */
   res_t ik_tool_exec_mail(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, const char *arguments);
   ```

2. Create `tests/unit/tool/mail_tool_test.c`:
   ```c
   /**
    * @file mail_tool_test.c
    * @brief Tests for mail tool execution handler
    *
    * Tests the ik_tool_exec_mail() function which handles:
    * - inbox action: list messages with unread_count
    * - read action: read single message, mark as read
    * - send action: send message to recipient
    * - error handling for invalid inputs
    */

   #include "../../../src/tool.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/agent.h"
   #include "../../../src/error.h"
   #include "../../../src/wrapper.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>
   #include <yyjson.h>

   // ========== Mock Infrastructure ==========

   // Mock state
   static int64_t mock_time_value = 1705312200;  // 2024-01-15T10:30:00Z
   static bool mock_db_should_fail = false;
   static int64_t mock_db_next_id = 1;
   static int mock_db_insert_call_count = 0;
   static int mock_db_mark_read_call_count = 0;

   // Mock time wrapper
   time_t time_(time_t *tloc)
   {
       if (tloc != NULL) {
           *tloc = (time_t)mock_time_value;
       }
       return (time_t)mock_time_value;
   }

   // Mock database insert (for send)
   res_t ik_db_mail_insert(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                            const char *from_agent_id, const char *to_agent_id,
                            const char *body, int64_t timestamp, int64_t *id_out)
   {
       (void)db;
       (void)session_id;
       (void)from_agent_id;
       (void)to_agent_id;
       (void)body;
       (void)timestamp;

       mock_db_insert_call_count++;

       if (mock_db_should_fail) {
           *id_out = 0;
           return ERR(ctx, IO, "Mock database error");
       }

       *id_out = mock_db_next_id++;
       return OK(NULL);
   }

   // Mock database mark read
   res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t id)
   {
       (void)db;
       (void)id;

       mock_db_mark_read_call_count++;

       if (mock_db_should_fail) {
           return ERR(NULL, IO, "Mock database error");
       }

       return OK(NULL);
   }

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_repl_ctx_t *repl;
   static ik_agent_ctx_t *agent;

   // Mock agents array
   #define MAX_TEST_AGENTS 4
   static ik_agent_ctx_t *test_agents[MAX_TEST_AGENTS];
   static size_t test_agent_count = 0;

   static void reset_mocks(void)
   {
       mock_time_value = 1705312200;  // 2024-01-15T10:30:00Z
       mock_db_should_fail = false;
       mock_db_next_id = 1;
       mock_db_insert_call_count = 0;
       mock_db_mark_read_call_count = 0;
   }

   static ik_agent_ctx_t *create_mock_agent(TALLOC_CTX *parent, const char *agent_id)
   {
       ik_agent_ctx_t *a = talloc_zero(parent, ik_agent_ctx_t);
       if (a == NULL) return NULL;

       a->agent_id = talloc_strdup(a, agent_id);
       a->inbox = ik_inbox_create(a);
       a->mail_notification_pending = false;

       return a;
   }

   // Helper to create a test message and add to inbox
   static void add_msg(ik_agent_ctx_t *a, int64_t id, const char *from,
                       const char *body, int64_t ts, bool read)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, id, from, a->agent_id,
                                       body, ts, read, &msg);
       ck_assert(is_ok(&res));
       res = ik_inbox_add(a->inbox, msg);
       ck_assert(is_ok(&res));
   }

   // Helper to parse result JSON and verify it's valid
   static yyjson_doc *parse_result(const char *json)
   {
       ck_assert_ptr_nonnull(json);
       yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
       ck_assert_ptr_nonnull(doc);
       return doc;
   }

   // Helper to get error message from result
   static const char *get_error_msg(const char *json)
   {
       yyjson_doc *doc = parse_result(json);
       yyjson_val *root = yyjson_doc_get_root(doc);
       yyjson_val *error = yyjson_obj_get(root, "error");
       if (error == NULL) {
           yyjson_doc_free(doc);
           return NULL;
       }
       const char *msg = yyjson_get_str(error);
       // Note: msg is owned by doc, caller should use before freeing doc
       return msg;
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       reset_mocks();

       // Create mock REPL context
       repl = talloc_zero(ctx, ik_repl_ctx_t);
       ck_assert_ptr_nonnull(repl);
       repl->current_session_id = 1;
       repl->db_ctx = (ik_db_ctx_t *)0xDEADBEEF;  // Mock pointer

       // Create agents: 0/, 1/, 2/
       test_agent_count = 3;
       for (size_t i = 0; i < test_agent_count; i++) {
           char agent_id[8];
           snprintf(agent_id, sizeof(agent_id), "%zu/", i);
           test_agents[i] = create_mock_agent(ctx, agent_id);
           ck_assert_ptr_nonnull(test_agents[i]);
       }

       // Set current agent to 0/
       agent = test_agents[0];
       repl->current_agent = agent;

       // Set up agent array in repl (for send validation)
       repl->agents = test_agents;
       repl->agent_count = test_agent_count;
   }

   static void teardown(void)
   {
       reset_mocks();

       for (size_t i = 0; i < MAX_TEST_AGENTS; i++) {
           test_agents[i] = NULL;
       }
       test_agent_count = 0;

       talloc_free(ctx);
       ctx = NULL;
       repl = NULL;
       agent = NULL;
   }

   // ========== Invalid Input Tests ==========

   // Test: Invalid JSON returns error
   START_TEST(test_mail_invalid_json)
   {
       res_t res = ik_tool_exec_mail(ctx, repl, "not valid json");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "Invalid JSON") != NULL);
   }
   END_TEST

   // Test: Missing action returns error
   START_TEST(test_mail_missing_action)
   {
       res_t res = ik_tool_exec_mail(ctx, repl, "{}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "action") != NULL);
   }
   END_TEST

   // Test: Unknown action returns error
   START_TEST(test_mail_unknown_action)
   {
       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"delete\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "Unknown action") != NULL);
       ck_assert(strstr(result, "delete") != NULL);
   }
   END_TEST

   // Test: Action with wrong type returns error
   START_TEST(test_mail_action_wrong_type)
   {
       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": 123}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
   }
   END_TEST

   // ========== Inbox Action Tests ==========

   // Test: Inbox action with empty inbox
   START_TEST(test_mail_inbox_empty)
   {
       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);

       yyjson_val *messages = yyjson_obj_get(root, "messages");
       ck_assert_ptr_nonnull(messages);
       ck_assert(yyjson_is_arr(messages));
       ck_assert_uint_eq(yyjson_arr_size(messages), 0);

       yyjson_val *unread = yyjson_obj_get(root, "unread_count");
       ck_assert_ptr_nonnull(unread);
       ck_assert_int_eq(yyjson_get_int(unread), 0);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Inbox action with messages
   START_TEST(test_mail_inbox_with_messages)
   {
       add_msg(agent, 5, "1/", "Found 3 OAuth patterns worth considering: 1) Silent refresh...",
               1705312100, false);
       add_msg(agent, 4, "2/", "Build complete, all 847 tests passing.",
               1705312000, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);

       yyjson_val *messages = yyjson_obj_get(root, "messages");
       ck_assert(yyjson_is_arr(messages));
       ck_assert_uint_eq(yyjson_arr_size(messages), 2);

       yyjson_val *unread = yyjson_obj_get(root, "unread_count");
       ck_assert_int_eq(yyjson_get_int(unread), 2);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Inbox message object has correct fields
   START_TEST(test_mail_inbox_message_fields)
   {
       add_msg(agent, 5, "1/", "Test message body here", 1705312100, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);

       yyjson_val *messages = yyjson_obj_get(root, "messages");
       yyjson_val *msg = yyjson_arr_get(messages, 0);
       ck_assert_ptr_nonnull(msg);

       // Check id
       yyjson_val *id = yyjson_obj_get(msg, "id");
       ck_assert_int_eq(yyjson_get_int(id), 5);

       // Check from
       yyjson_val *from = yyjson_obj_get(msg, "from");
       ck_assert_str_eq(yyjson_get_str(from), "1/");

       // Check unread
       yyjson_val *unread = yyjson_obj_get(msg, "unread");
       ck_assert(yyjson_get_bool(unread) == true);

       // Check preview
       yyjson_val *preview = yyjson_obj_get(msg, "preview");
       ck_assert_ptr_nonnull(preview);
       ck_assert(strstr(yyjson_get_str(preview), "Test message") != NULL);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Inbox preview truncated at 50 chars
   START_TEST(test_mail_inbox_preview_truncation)
   {
       const char *long_body = "This is a very long message body that exceeds "
                                "the fifty character preview limit and should be truncated";
       add_msg(agent, 5, "1/", long_body, 1705312100, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);
       yyjson_val *messages = yyjson_obj_get(root, "messages");
       yyjson_val *msg = yyjson_arr_get(messages, 0);
       yyjson_val *preview = yyjson_obj_get(msg, "preview");

       const char *preview_str = yyjson_get_str(preview);
       ck_assert_uint_le(strlen(preview_str), 53);  // 50 chars + "..."
       ck_assert(strstr(preview_str, "...") != NULL);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Inbox with read and unread messages
   START_TEST(test_mail_inbox_read_unread)
   {
       add_msg(agent, 5, "1/", "Unread message", 1705312100, false);
       add_msg(agent, 4, "2/", "Read message", 1705312000, true);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);

       yyjson_val *unread_count = yyjson_obj_get(root, "unread_count");
       ck_assert_int_eq(yyjson_get_int(unread_count), 1);

       yyjson_val *messages = yyjson_obj_get(root, "messages");
       // Unread first (id=5), read second (id=4)
       yyjson_val *msg1 = yyjson_arr_get(messages, 0);
       yyjson_val *msg2 = yyjson_arr_get(messages, 1);

       ck_assert(yyjson_get_bool(yyjson_obj_get(msg1, "unread")) == true);
       ck_assert(yyjson_get_bool(yyjson_obj_get(msg2, "unread")) == false);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Inbox clears notification_pending flag
   START_TEST(test_mail_inbox_clears_notification)
   {
       agent->mail_notification_pending = true;

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // ========== Read Action Tests ==========

   // Test: Read action missing id returns error
   START_TEST(test_mail_read_missing_id)
   {
       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "id") != NULL);
   }
   END_TEST

   // Test: Read action with non-existent message
   START_TEST(test_mail_read_not_found)
   {
       add_msg(agent, 5, "1/", "Test", 1705312100, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": 99}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "not found") != NULL);
   }
   END_TEST

   // Test: Read action success
   START_TEST(test_mail_read_success)
   {
       add_msg(agent, 5, "1/", "Found 3 OAuth patterns worth considering...",
               1705312200, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": 5}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);

       // No error field
       yyjson_val *error = yyjson_obj_get(root, "error");
       ck_assert_ptr_null(error);

       // Check id
       yyjson_val *id = yyjson_obj_get(root, "id");
       ck_assert_int_eq(yyjson_get_int(id), 5);

       // Check from
       yyjson_val *from = yyjson_obj_get(root, "from");
       ck_assert_str_eq(yyjson_get_str(from), "1/");

       // Check body (full, not truncated)
       yyjson_val *body = yyjson_obj_get(root, "body");
       ck_assert_str_eq(yyjson_get_str(body), "Found 3 OAuth patterns worth considering...");

       // Check timestamp (ISO 8601 format)
       yyjson_val *timestamp = yyjson_obj_get(root, "timestamp");
       const char *ts_str = yyjson_get_str(timestamp);
       ck_assert_ptr_nonnull(ts_str);
       // Should be in ISO 8601 format with Z suffix
       ck_assert(strstr(ts_str, "T") != NULL);
       ck_assert(strstr(ts_str, "Z") != NULL);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Read action marks message as read in memory
   START_TEST(test_mail_read_marks_read_memory)
   {
       add_msg(agent, 5, "1/", "Test", 1705312100, false);
       ck_assert_uint_eq(agent->inbox->unread_count, 1);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": 5}");

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(agent->inbox->unread_count, 0);

       ik_mail_msg_t *msg = ik_inbox_get_by_id(agent->inbox, 5);
       ck_assert(msg->read);
   }
   END_TEST

   // Test: Read action marks message as read in database
   START_TEST(test_mail_read_marks_read_database)
   {
       add_msg(agent, 5, "1/", "Test", 1705312100, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": 5}");

       ck_assert(is_ok(&res));
       ck_assert_int_eq(mock_db_mark_read_call_count, 1);
   }
   END_TEST

   // Test: Read action clears notification_pending flag
   START_TEST(test_mail_read_clears_notification)
   {
       agent->mail_notification_pending = true;
       add_msg(agent, 5, "1/", "Test", 1705312100, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": 5}");

       ck_assert(is_ok(&res));
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: Read action with string id (should parse)
   START_TEST(test_mail_read_string_id)
   {
       add_msg(agent, 5, "1/", "Test", 1705312100, false);

       // LLM might send id as string
       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": \"5\"}");

       // Should either work or return sensible error
       ck_assert(is_ok(&res));
       // Check if it found the message or returned proper error
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);
       // Either has id field (success) or error field
       yyjson_val *id = yyjson_obj_get(root, "id");
       yyjson_val *error = yyjson_obj_get(root, "error");
       ck_assert(id != NULL || error != NULL);
       yyjson_doc_free(doc);
   }
   END_TEST

   // ========== Send Action Tests ==========

   // Test: Send action missing to returns error
   START_TEST(test_mail_send_missing_to)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"body\": \"Hello\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "to") != NULL);
   }
   END_TEST

   // Test: Send action missing body returns error
   START_TEST(test_mail_send_missing_body)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "body") != NULL);
   }
   END_TEST

   // Test: Send action empty body returns error
   START_TEST(test_mail_send_empty_body)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\", \"body\": \"\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "empty") != NULL);
   }
   END_TEST

   // Test: Send action to nonexistent agent returns error
   START_TEST(test_mail_send_agent_not_found)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"99/\", \"body\": \"Hello\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
       ck_assert(strstr(result, "99/") != NULL);
       ck_assert(strstr(result, "not found") != NULL);
   }
   END_TEST

   // Test: Send action success
   START_TEST(test_mail_send_success)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\", \"body\": \"Hello from agent 0/\"}");

       ck_assert(is_ok(&res));
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);

       // No error
       yyjson_val *error = yyjson_obj_get(root, "error");
       ck_assert_ptr_null(error);

       // Check sent field
       yyjson_val *sent = yyjson_obj_get(root, "sent");
       ck_assert(yyjson_get_bool(sent) == true);

       // Check to field
       yyjson_val *to = yyjson_obj_get(root, "to");
       ck_assert_str_eq(yyjson_get_str(to), "1/");

       // Check id field
       yyjson_val *id = yyjson_obj_get(root, "id");
       ck_assert(yyjson_is_int(id));
       ck_assert_int_ge(yyjson_get_int(id), 1);

       yyjson_doc_free(doc);
   }
   END_TEST

   // Test: Send action adds to recipient inbox
   START_TEST(test_mail_send_adds_to_inbox)
   {
       ik_agent_ctx_t *recipient = test_agents[1];  // Agent 1/
       ck_assert_uint_eq(recipient->inbox->count, 0);

       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\", \"body\": \"Test message\"}");

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(recipient->inbox->count, 1);
       ck_assert_uint_eq(recipient->inbox->unread_count, 1);
   }
   END_TEST

   // Test: Send action inserts to database
   START_TEST(test_mail_send_database)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\", \"body\": \"Test\"}");

       ck_assert(is_ok(&res));
       ck_assert_int_eq(mock_db_insert_call_count, 1);
   }
   END_TEST

   // Test: Send action from current agent
   START_TEST(test_mail_send_from_current_agent)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\", \"body\": \"Test\"}");

       ck_assert(is_ok(&res));

       // Check recipient's inbox
       ik_agent_ctx_t *recipient = test_agents[1];
       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(recipient->inbox, &count);
       ck_assert_uint_eq(count, 1);
       ck_assert_str_eq(msgs[0]->from_agent_id, "0/");  // Current agent
   }
   END_TEST

   // Test: Send to self allowed
   START_TEST(test_mail_send_to_self)
   {
       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"0/\", \"body\": \"Note to self\"}");

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(agent->inbox->count, 1);
   }
   END_TEST

   // ========== Database Error Tests ==========

   // Test: Database error on send returns error
   START_TEST(test_mail_send_db_error)
   {
       mock_db_should_fail = true;

       res_t res = ik_tool_exec_mail(ctx, repl,
           "{\"action\": \"send\", \"to\": \"1/\", \"body\": \"Test\"}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
   }
   END_TEST

   // Test: Database error on read returns error
   START_TEST(test_mail_read_db_error)
   {
       mock_db_should_fail = true;
       add_msg(agent, 5, "1/", "Test", 1705312100, false);

       res_t res = ik_tool_exec_mail(ctx, repl, "{\"action\": \"read\", \"id\": 5}");

       ck_assert(is_ok(&res));
       const char *result = res.val;
       ck_assert(strstr(result, "\"error\"") != NULL);
   }
   END_TEST

   // ========== Memory Ownership Tests ==========

   // Test: Result string owned by provided context
   START_TEST(test_mail_result_ownership)
   {
       TALLOC_CTX *child = talloc_new(ctx);

       res_t res = ik_tool_exec_mail(child, repl, "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       ck_assert_ptr_eq(talloc_parent(res.val), child);

       talloc_free(child);
   }
   END_TEST

   // Test: No memory leak on multiple calls
   START_TEST(test_mail_no_memory_leak)
   {
       for (int i = 0; i < 100; i++) {
           TALLOC_CTX *tmp = talloc_new(ctx);
           res_t res = ik_tool_exec_mail(tmp, repl, "{\"action\": \"inbox\"}");
           ck_assert(is_ok(&res));
           talloc_free(tmp);
       }
       // If we get here without crash or OOM, test passes
   }
   END_TEST

   // ========== Integration with Dispatcher Tests ==========

   // Test: Dispatcher routes "mail" to ik_tool_exec_mail
   START_TEST(test_dispatcher_routes_mail)
   {
       res_t res = ik_tool_dispatch(ctx, repl, "mail", "{\"action\": \"inbox\"}");

       ck_assert(is_ok(&res));
       // Should return valid JSON with messages array
       yyjson_doc *doc = parse_result(res.val);
       yyjson_val *root = yyjson_doc_get_root(doc);
       yyjson_val *messages = yyjson_obj_get(root, "messages");
       ck_assert_ptr_nonnull(messages);
       yyjson_doc_free(doc);
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *mail_tool_suite(void)
   {
       Suite *s = suite_create("MailTool");

       TCase *tc_invalid = tcase_create("Invalid");
       tcase_add_checked_fixture(tc_invalid, setup, teardown);
       tcase_add_test(tc_invalid, test_mail_invalid_json);
       tcase_add_test(tc_invalid, test_mail_missing_action);
       tcase_add_test(tc_invalid, test_mail_unknown_action);
       tcase_add_test(tc_invalid, test_mail_action_wrong_type);
       suite_add_tcase(s, tc_invalid);

       TCase *tc_inbox = tcase_create("Inbox");
       tcase_add_checked_fixture(tc_inbox, setup, teardown);
       tcase_add_test(tc_inbox, test_mail_inbox_empty);
       tcase_add_test(tc_inbox, test_mail_inbox_with_messages);
       tcase_add_test(tc_inbox, test_mail_inbox_message_fields);
       tcase_add_test(tc_inbox, test_mail_inbox_preview_truncation);
       tcase_add_test(tc_inbox, test_mail_inbox_read_unread);
       tcase_add_test(tc_inbox, test_mail_inbox_clears_notification);
       suite_add_tcase(s, tc_inbox);

       TCase *tc_read = tcase_create("Read");
       tcase_add_checked_fixture(tc_read, setup, teardown);
       tcase_add_test(tc_read, test_mail_read_missing_id);
       tcase_add_test(tc_read, test_mail_read_not_found);
       tcase_add_test(tc_read, test_mail_read_success);
       tcase_add_test(tc_read, test_mail_read_marks_read_memory);
       tcase_add_test(tc_read, test_mail_read_marks_read_database);
       tcase_add_test(tc_read, test_mail_read_clears_notification);
       tcase_add_test(tc_read, test_mail_read_string_id);
       suite_add_tcase(s, tc_read);

       TCase *tc_send = tcase_create("Send");
       tcase_add_checked_fixture(tc_send, setup, teardown);
       tcase_add_test(tc_send, test_mail_send_missing_to);
       tcase_add_test(tc_send, test_mail_send_missing_body);
       tcase_add_test(tc_send, test_mail_send_empty_body);
       tcase_add_test(tc_send, test_mail_send_agent_not_found);
       tcase_add_test(tc_send, test_mail_send_success);
       tcase_add_test(tc_send, test_mail_send_adds_to_inbox);
       tcase_add_test(tc_send, test_mail_send_database);
       tcase_add_test(tc_send, test_mail_send_from_current_agent);
       tcase_add_test(tc_send, test_mail_send_to_self);
       suite_add_tcase(s, tc_send);

       TCase *tc_db_error = tcase_create("DatabaseError");
       tcase_add_checked_fixture(tc_db_error, setup, teardown);
       tcase_add_test(tc_db_error, test_mail_send_db_error);
       tcase_add_test(tc_db_error, test_mail_read_db_error);
       suite_add_tcase(s, tc_db_error);

       TCase *tc_memory = tcase_create("Memory");
       tcase_add_checked_fixture(tc_memory, setup, teardown);
       tcase_add_test(tc_memory, test_mail_result_ownership);
       tcase_add_test(tc_memory, test_mail_no_memory_leak);
       suite_add_tcase(s, tc_memory);

       TCase *tc_dispatch = tcase_create("Dispatcher");
       tcase_add_checked_fixture(tc_dispatch, setup, teardown);
       tcase_add_test(tc_dispatch, test_dispatcher_routes_mail);
       suite_add_tcase(s, tc_dispatch);

       return s;
   }

   int main(void)
   {
       Suite *s = mail_tool_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Add stub to `src/tool.c`:
   ```c
   res_t ik_tool_exec_mail(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, const char *arguments)
   {
       (void)repl;
       (void)arguments;
       return OK(talloc_strdup(ctx, "{\"error\": \"Not implemented\"}"));
   }
   ```

4. Update Makefile:
   - Verify `tests/unit/tool/mail_tool_test.c` is picked up by wildcard

5. Run `make check` - expect test failures (stub returns error)

### Green
1. Implement helper functions in `src/tool.c`:

   ```c
   // Helper: Format Unix timestamp as ISO 8601 string
   static char *format_iso8601(TALLOC_CTX *ctx, int64_t timestamp)
   {
       time_t t = (time_t)timestamp;
       struct tm *tm = gmtime(&t);
       if (tm == NULL) {
           return talloc_strdup(ctx, "1970-01-01T00:00:00Z");
       }

       char buf[32];
       strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
       return talloc_strdup(ctx, buf);
   }

   // Helper: Create preview of message body (first ~50 chars + "...")
   static char *create_mail_preview(TALLOC_CTX *ctx, const char *body)
   {
       assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
       assert(body != NULL);  // LCOV_EXCL_BR_LINE

       size_t len = strlen(body);
       if (len <= 50) {
           return talloc_strdup(ctx, body);
       }

       // Truncate and add "..."
       char *preview = talloc_array(ctx, char, 54);  // 50 + "..." + NUL
       if (preview == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       memcpy(preview, body, 50);
       preview[50] = '.';
       preview[51] = '.';
       preview[52] = '.';
       preview[53] = '\0';

       return preview;
   }

   // Helper: Build error result JSON
   static char *build_mail_error(TALLOC_CTX *ctx, const char *message)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       yyjson_mut_val *root = yyjson_mut_obj(doc);
       if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       yyjson_mut_doc_set_root(doc, root);

       if (!yyjson_mut_obj_add_str(doc, root, "error", message)) {
           PANIC("Failed to add error field");  // LCOV_EXCL_BR_LINE
       }

       size_t len = 0;
       char *json = yyjson_mut_write(doc, 0, &len);
       if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *result = talloc_strdup(ctx, json);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       free(json);
       yyjson_mut_doc_free(doc);

       return result;
   }
   ```

2. Implement `exec_mail_inbox()` helper:
   ```c
   static char *exec_mail_inbox(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
   {
       assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
       assert(agent != NULL);  // LCOV_EXCL_BR_LINE

       // Clear notification_pending flag
       agent->mail_notification_pending = false;

       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       yyjson_mut_val *root = yyjson_mut_obj(doc);
       if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       yyjson_mut_doc_set_root(doc, root);

       // Build messages array
       yyjson_mut_val *messages_arr = yyjson_mut_arr(doc);
       if (messages_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       size_t count;
       ik_mail_msg_t **messages = ik_inbox_get_all(agent->inbox, &count);

       for (size_t i = 0; i < count; i++) {
           ik_mail_msg_t *msg = messages[i];

           yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
           if (msg_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

           if (!yyjson_mut_obj_add_int(doc, msg_obj, "id", msg->id)) {
               PANIC("Failed to add id");  // LCOV_EXCL_BR_LINE
           }
           if (!yyjson_mut_obj_add_str(doc, msg_obj, "from", msg->from_agent_id)) {
               PANIC("Failed to add from");  // LCOV_EXCL_BR_LINE
           }
           if (!yyjson_mut_obj_add_bool(doc, msg_obj, "unread", !msg->read)) {
               PANIC("Failed to add unread");  // LCOV_EXCL_BR_LINE
           }

           char *preview = create_mail_preview(ctx, msg->body);
           if (!yyjson_mut_obj_add_str(doc, msg_obj, "preview", preview)) {
               PANIC("Failed to add preview");  // LCOV_EXCL_BR_LINE
           }

           if (!yyjson_mut_arr_add_val(messages_arr, msg_obj)) {
               PANIC("Failed to add message to array");  // LCOV_EXCL_BR_LINE
           }
       }

       if (!yyjson_mut_obj_add_val(doc, root, "messages", messages_arr)) {
           PANIC("Failed to add messages array");  // LCOV_EXCL_BR_LINE
       }

       if (!yyjson_mut_obj_add_uint(doc, root, "unread_count", agent->inbox->unread_count)) {
           PANIC("Failed to add unread_count");  // LCOV_EXCL_BR_LINE
       }

       size_t len = 0;
       char *json = yyjson_mut_write(doc, 0, &len);
       if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *result = talloc_strdup(ctx, json);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       free(json);
       yyjson_mut_doc_free(doc);

       return result;
   }
   ```

3. Implement `exec_mail_read()` helper:
   ```c
   static char *exec_mail_read(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                                ik_agent_ctx_t *agent, int64_t message_id)
   {
       assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
       assert(repl != NULL);   // LCOV_EXCL_BR_LINE
       assert(agent != NULL);  // LCOV_EXCL_BR_LINE

       // Look up message
       ik_mail_msg_t *msg = ik_inbox_get_by_id(agent->inbox, message_id);
       if (msg == NULL) {
           char error_msg[64];
           snprintf(error_msg, sizeof(error_msg),
                    "Message #%" PRId64 " not found", message_id);
           return build_mail_error(ctx, error_msg);
       }

       // Mark as read in memory
       res_t mark_res = ik_inbox_mark_read(agent->inbox, msg);
       if (is_err(&mark_res)) {  // LCOV_EXCL_BR_LINE
           return build_mail_error(ctx, "Failed to mark message as read");  // LCOV_EXCL_LINE
       }

       // Mark as read in database
       res_t db_res = ik_db_mail_mark_read(repl->db_ctx, message_id);
       if (is_err(&db_res)) {
           return build_mail_error(ctx, "Database error");
       }

       // Clear notification_pending flag
       agent->mail_notification_pending = false;

       // Build result JSON
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       yyjson_mut_val *root = yyjson_mut_obj(doc);
       if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       yyjson_mut_doc_set_root(doc, root);

       if (!yyjson_mut_obj_add_int(doc, root, "id", msg->id)) {
           PANIC("Failed to add id");  // LCOV_EXCL_BR_LINE
       }
       if (!yyjson_mut_obj_add_str(doc, root, "from", msg->from_agent_id)) {
           PANIC("Failed to add from");  // LCOV_EXCL_BR_LINE
       }

       char *timestamp = format_iso8601(ctx, msg->timestamp);
       if (!yyjson_mut_obj_add_str(doc, root, "timestamp", timestamp)) {
           PANIC("Failed to add timestamp");  // LCOV_EXCL_BR_LINE
       }

       if (!yyjson_mut_obj_add_str(doc, root, "body", msg->body)) {
           PANIC("Failed to add body");  // LCOV_EXCL_BR_LINE
       }

       size_t len = 0;
       char *json = yyjson_mut_write(doc, 0, &len);
       if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *result = talloc_strdup(ctx, json);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       free(json);
       yyjson_mut_doc_free(doc);

       return result;
   }
   ```

4. Implement `exec_mail_send()` helper:
   ```c
   static char *exec_mail_send(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                                const char *to_agent_id, const char *body)
   {
       assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
       assert(repl != NULL);         // LCOV_EXCL_BR_LINE
       assert(to_agent_id != NULL);  // LCOV_EXCL_BR_LINE
       assert(body != NULL);         // LCOV_EXCL_BR_LINE

       // Get current agent (sender)
       ik_agent_ctx_t *sender = repl->current_agent;
       if (sender == NULL) {  // LCOV_EXCL_BR_LINE
           return build_mail_error(ctx, "No current agent");  // LCOV_EXCL_LINE
       }

       // Send mail using high-level operation
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, sender->agent_id, to_agent_id, body, &id);
       if (is_err(&res)) {
           // Extract error message from res_t
           // Note: Actual implementation depends on error.h structure
           return build_mail_error(ctx, res.error ? res.error : "Send failed");
       }

       // Build success result JSON
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       yyjson_mut_val *root = yyjson_mut_obj(doc);
       if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       yyjson_mut_doc_set_root(doc, root);

       if (!yyjson_mut_obj_add_bool(doc, root, "sent", true)) {
           PANIC("Failed to add sent");  // LCOV_EXCL_BR_LINE
       }
       if (!yyjson_mut_obj_add_str(doc, root, "to", to_agent_id)) {
           PANIC("Failed to add to");  // LCOV_EXCL_BR_LINE
       }
       if (!yyjson_mut_obj_add_int(doc, root, "id", id)) {
           PANIC("Failed to add id");  // LCOV_EXCL_BR_LINE
       }

       size_t len = 0;
       char *json = yyjson_mut_write(doc, 0, &len);
       if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *result = talloc_strdup(ctx, json);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       free(json);
       yyjson_mut_doc_free(doc);

       return result;
   }
   ```

5. Implement main `ik_tool_exec_mail()`:
   ```c
   res_t ik_tool_exec_mail(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, const char *arguments)
   {
       assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
       assert(repl != NULL);      // LCOV_EXCL_BR_LINE
       assert(arguments != NULL); // LCOV_EXCL_BR_LINE

       // Parse arguments JSON
       yyjson_doc *doc = yyjson_read(arguments, strlen(arguments), 0);
       if (doc == NULL) {
           return OK(build_mail_error(ctx, "Invalid JSON arguments"));
       }

       yyjson_val *root = yyjson_doc_get_root(doc);
       if (root == NULL || !yyjson_is_obj(root)) {
           yyjson_doc_free(doc);
           return OK(build_mail_error(ctx, "Invalid JSON arguments"));
       }

       // Get action
       yyjson_val *action_val = yyjson_obj_get(root, "action");
       if (action_val == NULL) {
           yyjson_doc_free(doc);
           return OK(build_mail_error(ctx, "Missing required parameter: action"));
       }

       if (!yyjson_is_str(action_val)) {
           yyjson_doc_free(doc);
           return OK(build_mail_error(ctx, "Invalid action type"));
       }

       const char *action = yyjson_get_str(action_val);
       ik_agent_ctx_t *agent = repl->current_agent;

       if (agent == NULL) {  // LCOV_EXCL_BR_LINE
           yyjson_doc_free(doc);  // LCOV_EXCL_LINE
           return OK(build_mail_error(ctx, "No current agent"));  // LCOV_EXCL_LINE
       }

       char *result = NULL;

       if (strcmp(action, "inbox") == 0) {
           result = exec_mail_inbox(ctx, agent);
       }
       else if (strcmp(action, "read") == 0) {
           // Get id parameter
           yyjson_val *id_val = yyjson_obj_get(root, "id");
           if (id_val == NULL) {
               yyjson_doc_free(doc);
               return OK(build_mail_error(ctx, "Missing required parameter: id"));
           }

           int64_t id;
           if (yyjson_is_int(id_val)) {
               id = yyjson_get_int(id_val);
           } else if (yyjson_is_str(id_val)) {
               // Handle string ID (LLM might send as string)
               const char *id_str = yyjson_get_str(id_val);
               char *endptr;
               id = strtoll(id_str, &endptr, 10);
               if (*endptr != '\0' || id <= 0) {
                   yyjson_doc_free(doc);
                   return OK(build_mail_error(ctx, "Invalid message ID"));
               }
           } else {
               yyjson_doc_free(doc);
               return OK(build_mail_error(ctx, "Invalid id type"));
           }

           result = exec_mail_read(ctx, repl, agent, id);
       }
       else if (strcmp(action, "send") == 0) {
           // Get to parameter
           yyjson_val *to_val = yyjson_obj_get(root, "to");
           if (to_val == NULL || !yyjson_is_str(to_val)) {
               yyjson_doc_free(doc);
               return OK(build_mail_error(ctx, "Missing required parameter: to"));
           }
           const char *to = yyjson_get_str(to_val);

           // Get body parameter
           yyjson_val *body_val = yyjson_obj_get(root, "body");
           if (body_val == NULL || !yyjson_is_str(body_val)) {
               yyjson_doc_free(doc);
               return OK(build_mail_error(ctx, "Missing required parameter: body"));
           }
           const char *body = yyjson_get_str(body_val);

           // Validate body is not empty
           if (strlen(body) == 0) {
               yyjson_doc_free(doc);
               return OK(build_mail_error(ctx, "Message body cannot be empty"));
           }

           result = exec_mail_send(ctx, repl, to, body);
       }
       else {
           char error_msg[64];
           snprintf(error_msg, sizeof(error_msg), "Unknown action: %s", action);
           yyjson_doc_free(doc);
           return OK(build_mail_error(ctx, error_msg));
       }

       yyjson_doc_free(doc);
       return OK(result);
   }
   ```

6. Update `ik_tool_dispatch()` in `src/tool.c`:
   ```c
   // In ik_tool_dispatch(), add mail tool routing:
   // ... existing tool dispatches ...
   if (strcmp(tool_name, "mail") == 0) {
       return ik_tool_exec_mail(ctx, repl, arguments);
   }
   // ... unknown tool handling ...
   ```

7. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (none for helper functions)
   - Sibling headers (`mail/inbox.h`, `mail/msg.h`, `mail/send.h`)
   - Project headers (`agent.h`, `db/mail.h`, `panic.h`, `wrapper.h`)
   - System headers (`<assert.h>`, `<inttypes.h>`, `<string.h>`, `<time.h>`)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments

4. Consider factoring out common JSON building patterns

5. Run `make lint` - verify clean

6. Run `make coverage` - verify 100% coverage on new code

7. Run `make check-valgrind` - verify no memory leaks

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on new code
- `ik_tool_exec_mail()` function implemented with:
  - JSON argument parsing
  - Action dispatch (inbox/read/send)
  - Error handling for all invalid inputs
  - Correct JSON result format for each action
- Dispatcher updated to route "mail" tool to `ik_tool_exec_mail()`
- Tests verify:
  - Invalid JSON handling
  - Missing/unknown action handling
  - Action-specific parameter validation
  - Inbox action: empty, with messages, preview truncation, ordering
  - Read action: success, not found, marks read, clears notification
  - Send action: success, validation errors, database insert
  - Database error handling
  - Memory ownership and leak prevention
  - Dispatcher integration
- Working tree is clean (all changes committed)

## Notes

### Tool Result Format vs Command Output

The mail tool returns JSON for LLM consumption, which differs from the command output format:

| Action | Tool Result (JSON) | Command Output (Text) |
|--------|--------------------|-----------------------|
| inbox | `{"messages": [...], "unread_count": N}` | `Inbox for agent 0/:\n  #5 [unread] from 1/...` |
| read | `{"id": N, "from": "...", "timestamp": "...", "body": "..."}` | `From: 1/\nTime: 2 min ago\n\nBody...` |
| send | `{"sent": true, "to": "...", "id": N}` | `Mail sent to agent 1/` |

### notification_pending Flag Behavior

- Cleared on `inbox` action (agent is actively checking mail)
- Cleared on `read` action (agent is reading a message)
- NOT cleared on `send` action (sending doesn't acknowledge receipt)
- NOT cleared on errors (agent didn't successfully interact with inbox)

### Error Message Consistency

Error messages should match the patterns established in user stories:
- "Agent 99/ not found" (from user story 42 error example)
- "Message body cannot be empty" (from user story 42 error example)
- "Message #5 not found" (from user story 41 error example)

### ISO 8601 Timestamp Format

The read action returns timestamps in ISO 8601 format: `YYYY-MM-DDTHH:MM:SSZ`

This differs from the command output which uses relative time ("2 minutes ago"). The JSON format is more suitable for LLM processing and potential UI formatting.

### Preview Length

Preview in inbox action is truncated to 50 characters with "..." suffix, matching the command output preview length from user story 30.

### Integration with High-Level Operations

The send action uses `ik_mail_send()` which handles:
- Recipient validation
- Body trimming and validation
- Database persistence
- In-memory inbox update

The read action directly manipulates inbox state but uses the same pattern as `ik_mail_read()`:
- Message lookup
- Mark as read (memory + DB)
- Clear notification flag

### Testing Strategy

Tests are organized by:
1. **Invalid input**: JSON parsing, missing/invalid action
2. **Inbox action**: Empty inbox, message fields, preview, ordering
3. **Read action**: Parameter validation, success, mark read
4. **Send action**: Parameter validation, success, inbox update
5. **Database errors**: Error propagation
6. **Memory**: Ownership, leak prevention
7. **Dispatcher**: Integration with tool routing

### Future Considerations

1. **Pagination**: Large inboxes could benefit from limit/offset parameters
2. **Filtering**: Filter by read/unread, sender, date range
3. **Sorting options**: Allow agent to request different sort orders
4. **Bulk operations**: Mark multiple messages as read

### Dependency Chain

```
mail-tool-schema.md     (defines mail tool JSON schema)
        |
        v
mail-tool-handler.md    (this task - implements tool execution)
        |
        v
mail-notification.md    (future - injects notifications on IDLE)
        |
        v
mail-e2e.md             (future - end-to-end integration tests)
```
