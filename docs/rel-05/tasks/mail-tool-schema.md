# Task: Build JSON Schema for Mail Tool

## Target
Phase 3: Inter-Agent Mailboxes - Step 10 (Tool schema for LLM access)

Supports User Stories:
- 35 (agent sends mail via tool) - Agent uses `mail` tool with `action: send`
- 36 (agent checks inbox via tool) - Agent uses `mail` tool with `action: inbox`
- 37 (agent reads message via tool) - Agent uses `mail` tool with `action: read`

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/quality.md

### Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Agent Interface section - mail tool JSON schema)
- docs/memory.md (talloc ownership patterns)
- docs/naming.md (ik_ prefix conventions)
- docs/return_values.md (yyjson return patterns)

### Pre-read Source (patterns)
- src/tool.h (existing schema builder declarations, ik_tool_add_string_param helper)
- src/tool.c (existing schema implementations - glob, file_read, grep, file_write, bash)
- src/openai/client.c (yyjson JSON building patterns)
- src/config.c (yyjson nested object/array construction)

### Pre-read Tests (patterns)
- tests/unit/tool/tool_test.c (schema verification patterns - verify_schema_basics, verify_string_param, verify_required)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `/mail` command registered and working (mail-cmd-register.md complete)
- Subcommand dispatch implemented for send/inbox/read
- `ik_tool_build_all()` exists and returns array of 5 tools
- `ik_tool_add_string_param()` helper exists in src/tool.c
- Test helpers exist: `verify_schema_basics()`, `verify_string_param()`, `verify_required()`

## Task
Create `ik_tool_build_mail_schema()` function that builds the JSON schema for the mail tool. This enables LLM agents to send and receive messages programmatically using the function calling interface.

**Target Schema (from backlog):**
```json
{
  "type": "function",
  "function": {
    "name": "mail",
    "description": "Send and receive messages to/from other agents",
    "parameters": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "enum": ["inbox", "read", "send"],
          "description": "Operation to perform"
        },
        "to": {
          "type": "string",
          "description": "Recipient agent ID (required for send)"
        },
        "body": {
          "type": "string",
          "description": "Message body (required for send)"
        },
        "id": {
          "type": "integer",
          "description": "Message ID (required for read)"
        }
      },
      "required": ["action"]
    }
  }
}
```

**Key Implementation Notes:**

1. **Enum Constraint for `action`:**
   - Unlike string parameters, `action` requires an `enum` array
   - Must create `yyjson_mut_arr` with values ["inbox", "read", "send"]
   - This constrains the LLM to valid action values

2. **Integer Type for `id`:**
   - Unlike other tools, `id` parameter has `"type": "integer"`
   - Cannot use `ik_tool_add_string_param()` helper
   - Requires custom parameter construction

3. **Conditional Requirements:**
   - Only `action` is in the `required` array
   - Other parameters are conditionally required based on action value
   - LLM understands conditional requirements from descriptions

4. **Integration with `ik_tool_build_all()`:**
   - Add mail schema to the tools array
   - Array will have 6 tools after this task
   - Update array size verification in tests

**New Helper Function (Optional but Recommended):**

Consider adding `ik_tool_add_enum_param()` helper for reusability:
```c
void ik_tool_add_enum_param(yyjson_mut_doc *doc,
                            yyjson_mut_val *properties,
                            const char *name,
                            const char *description,
                            const char *enum_values[],
                            size_t enum_count);
```

If a helper is added, also add `ik_tool_add_integer_param()`:
```c
void ik_tool_add_integer_param(yyjson_mut_doc *doc,
                               yyjson_mut_val *properties,
                               const char *name,
                               const char *description);
```

## TDD Cycle

### Red
1. Update `src/tool.h` - Add declarations:
   ```c
   /**
    * Helper function to add an enum parameter to properties object.
    *
    * Creates a parameter with type "string", description, and enum constraint.
    * The parameter is added to the properties object under the given name.
    *
    * @param doc The yyjson mutable document
    * @param properties The properties object to add to
    * @param name Parameter name
    * @param description Parameter description
    * @param enum_values Array of valid enum string values
    * @param enum_count Number of elements in enum_values
    */
   void ik_tool_add_enum_param(yyjson_mut_doc *doc,
                               yyjson_mut_val *properties,
                               const char *name,
                               const char *description,
                               const char *enum_values[],
                               size_t enum_count);

   /**
    * Helper function to add an integer parameter to properties object.
    *
    * Creates a parameter with type "integer" and description.
    * The parameter is added to the properties object under the given name.
    *
    * @param doc The yyjson mutable document
    * @param properties The properties object to add to
    * @param name Parameter name
    * @param description Parameter description
    */
   void ik_tool_add_integer_param(yyjson_mut_doc *doc,
                                  yyjson_mut_val *properties,
                                  const char *name,
                                  const char *description);

   /**
    * Build JSON schema for the mail tool.
    *
    * Creates a tool schema object following OpenAI's function calling format.
    * The schema includes the tool name "mail", description, and parameter
    * specifications with:
    * - "action" as required (enum: inbox, read, send)
    * - "to" as optional string (required for send)
    * - "body" as optional string (required for send)
    * - "id" as optional integer (required for read)
    *
    * @param doc The yyjson mutable document to build the schema in
    * @return Pointer to the schema object (owned by doc)
    */
   yyjson_mut_val *ik_tool_build_mail_schema(yyjson_mut_doc *doc);
   ```

2. Update `tests/unit/tool/tool_test.c` - Add helper verification tests:
   ```c
   // Helper: Verify enum parameter exists with correct values
   static void verify_enum_param(yyjson_mut_val *properties,
                                 const char *param_name,
                                 const char *expected_values[],
                                 size_t expected_count)
   {
       yyjson_mut_val *param = yyjson_mut_obj_get(properties, param_name);
       ck_assert_ptr_nonnull(param);

       yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
       ck_assert_ptr_nonnull(type);
       ck_assert_str_eq(yyjson_mut_get_str(type), "string");

       yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
       ck_assert_ptr_nonnull(description);

       yyjson_mut_val *enum_arr = yyjson_mut_obj_get(param, "enum");
       ck_assert_ptr_nonnull(enum_arr);
       ck_assert(yyjson_mut_is_arr(enum_arr));
       ck_assert_uint_eq(yyjson_mut_arr_size(enum_arr), expected_count);

       for (size_t i = 0; i < expected_count; i++) {
           yyjson_mut_val *item = yyjson_mut_arr_get(enum_arr, i);
           ck_assert_ptr_nonnull(item);
           ck_assert_str_eq(yyjson_mut_get_str(item), expected_values[i]);
       }
   }

   // Helper: Verify integer parameter exists
   static void verify_integer_param(yyjson_mut_val *properties,
                                    const char *param_name)
   {
       yyjson_mut_val *param = yyjson_mut_obj_get(properties, param_name);
       ck_assert_ptr_nonnull(param);

       yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
       ck_assert_ptr_nonnull(type);
       ck_assert_str_eq(yyjson_mut_get_str(type), "integer");

       yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
       ck_assert_ptr_nonnull(description);
   }
   ```

3. Add tests for `ik_tool_add_enum_param()`:
   ```c
   // Test: ik_tool_add_enum_param adds parameter with enum constraint
   START_TEST(test_tool_add_enum_param) {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *properties = yyjson_mut_obj(doc);
       ck_assert_ptr_nonnull(properties);

       const char *actions[] = {"inbox", "read", "send"};
       ik_tool_add_enum_param(doc, properties, "action",
                              "Operation to perform", actions, 3);

       // Verify parameter was added
       yyjson_mut_val *param = yyjson_mut_obj_get(properties, "action");
       ck_assert_ptr_nonnull(param);

       // Verify type is string
       yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
       ck_assert_ptr_nonnull(type);
       ck_assert_str_eq(yyjson_mut_get_str(type), "string");

       // Verify description
       yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
       ck_assert_ptr_nonnull(description);
       ck_assert_str_eq(yyjson_mut_get_str(description), "Operation to perform");

       // Verify enum array
       yyjson_mut_val *enum_arr = yyjson_mut_obj_get(param, "enum");
       ck_assert_ptr_nonnull(enum_arr);
       ck_assert(yyjson_mut_is_arr(enum_arr));
       ck_assert_uint_eq(yyjson_mut_arr_size(enum_arr), 3);

       ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_arr_get(enum_arr, 0)), "inbox");
       ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_arr_get(enum_arr, 1)), "read");
       ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_arr_get(enum_arr, 2)), "send");

       yyjson_mut_doc_free(doc);
   }
   END_TEST
   ```

4. Add tests for `ik_tool_add_integer_param()`:
   ```c
   // Test: ik_tool_add_integer_param adds parameter correctly
   START_TEST(test_tool_add_integer_param) {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *properties = yyjson_mut_obj(doc);
       ck_assert_ptr_nonnull(properties);

       ik_tool_add_integer_param(doc, properties, "id", "Message ID");

       // Verify parameter was added
       yyjson_mut_val *param = yyjson_mut_obj_get(properties, "id");
       ck_assert_ptr_nonnull(param);

       // Verify type is integer
       yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
       ck_assert_ptr_nonnull(type);
       ck_assert_str_eq(yyjson_mut_get_str(type), "integer");

       // Verify description
       yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
       ck_assert_ptr_nonnull(description);
       ck_assert_str_eq(yyjson_mut_get_str(description), "Message ID");

       yyjson_mut_doc_free(doc);
   }
   END_TEST
   ```

5. Add tests for `ik_tool_build_mail_schema()`:
   ```c
   // Test: ik_tool_build_mail_schema returns non-NULL and has correct structure
   START_TEST(test_tool_build_mail_schema_structure)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *schema = ik_tool_build_mail_schema(doc);
       verify_schema_basics(schema, "mail");

       // Verify description mentions agents/messages
       yyjson_mut_val *function = yyjson_mut_obj_get(schema, "function");
       yyjson_mut_val *description = yyjson_mut_obj_get(function, "description");
       ck_assert_ptr_nonnull(description);
       const char *desc_str = yyjson_mut_get_str(description);
       ck_assert(strstr(desc_str, "agents") != NULL || strstr(desc_str, "messages") != NULL);

       yyjson_mut_doc_free(doc);
   }
   END_TEST

   // Test: mail schema has action parameter with enum constraint
   START_TEST(test_tool_build_mail_schema_action_enum)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *schema = ik_tool_build_mail_schema(doc);
       yyjson_mut_val *parameters = get_parameters(schema);
       yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
       ck_assert_ptr_nonnull(properties);

       const char *expected_actions[] = {"inbox", "read", "send"};
       verify_enum_param(properties, "action", expected_actions, 3);

       yyjson_mut_doc_free(doc);
   }
   END_TEST

   // Test: mail schema has to parameter (string)
   START_TEST(test_tool_build_mail_schema_to_param)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *schema = ik_tool_build_mail_schema(doc);
       yyjson_mut_val *parameters = get_parameters(schema);
       yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
       ck_assert_ptr_nonnull(properties);

       verify_string_param(properties, "to");

       // Verify description mentions recipient or agent
       yyjson_mut_val *to_param = yyjson_mut_obj_get(properties, "to");
       yyjson_mut_val *description = yyjson_mut_obj_get(to_param, "description");
       const char *desc_str = yyjson_mut_get_str(description);
       ck_assert(strstr(desc_str, "agent") != NULL || strstr(desc_str, "Recipient") != NULL);

       yyjson_mut_doc_free(doc);
   }
   END_TEST

   // Test: mail schema has body parameter (string)
   START_TEST(test_tool_build_mail_schema_body_param)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *schema = ik_tool_build_mail_schema(doc);
       yyjson_mut_val *parameters = get_parameters(schema);
       yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
       ck_assert_ptr_nonnull(properties);

       verify_string_param(properties, "body");

       yyjson_mut_doc_free(doc);
   }
   END_TEST

   // Test: mail schema has id parameter (integer)
   START_TEST(test_tool_build_mail_schema_id_param)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *schema = ik_tool_build_mail_schema(doc);
       yyjson_mut_val *parameters = get_parameters(schema);
       yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
       ck_assert_ptr_nonnull(properties);

       verify_integer_param(properties, "id");

       yyjson_mut_doc_free(doc);
   }
   END_TEST

   // Test: mail schema only requires action
   START_TEST(test_tool_build_mail_schema_required)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *schema = ik_tool_build_mail_schema(doc);
       yyjson_mut_val *parameters = get_parameters(schema);

       const char *required_params[] = {"action"};
       verify_required(parameters, required_params, 1);

       yyjson_mut_doc_free(doc);
   }
   END_TEST
   ```

6. Add test for updated `ik_tool_build_all()`:
   ```c
   // Test: ik_tool_build_all returns array with all 6 tools (including mail)
   START_TEST(test_tool_build_all_includes_mail)
   {
       yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
       ck_assert_ptr_nonnull(doc);

       yyjson_mut_val *arr = ik_tool_build_all(doc);

       // Verify returns non-NULL
       ck_assert_ptr_nonnull(arr);

       // Verify is array
       ck_assert(yyjson_mut_is_arr(arr));

       // Verify has exactly 6 elements
       ck_assert_uint_eq(yyjson_mut_arr_size(arr), 6);

       // Verify sixth element has "function.name": "mail"
       yyjson_mut_val *sixth = yyjson_mut_arr_get(arr, 5);
       ck_assert_ptr_nonnull(sixth);
       yyjson_mut_val *function = yyjson_mut_obj_get(sixth, "function");
       ck_assert_ptr_nonnull(function);
       yyjson_mut_val *name = yyjson_mut_obj_get(function, "name");
       ck_assert_ptr_nonnull(name);
       ck_assert_str_eq(yyjson_mut_get_str(name), "mail");

       yyjson_mut_doc_free(doc);
   }
   END_TEST
   ```

7. Add stubs to `src/tool.c`:
   ```c
   void ik_tool_add_enum_param(yyjson_mut_doc *doc,
                               yyjson_mut_val *properties,
                               const char *name,
                               const char *description,
                               const char *enum_values[],
                               size_t enum_count)
   {
       (void)doc;
       (void)properties;
       (void)name;
       (void)description;
       (void)enum_values;
       (void)enum_count;
       // Stub - to be implemented
   }

   void ik_tool_add_integer_param(yyjson_mut_doc *doc,
                                  yyjson_mut_val *properties,
                                  const char *name,
                                  const char *description)
   {
       (void)doc;
       (void)properties;
       (void)name;
       (void)description;
       // Stub - to be implemented
   }

   yyjson_mut_val *ik_tool_build_mail_schema(yyjson_mut_doc *doc)
   {
       (void)doc;
       return NULL;  // Stub - to be implemented
   }
   ```

8. Update test suite registration in `tests/unit/tool/tool_test.c`:
   ```c
   // Add new test cases to suite
   TCase *tc_enum_helper = tcase_create("Enum Helper");
   tcase_add_checked_fixture(tc_enum_helper, setup, teardown);
   tcase_add_test(tc_enum_helper, test_tool_add_enum_param);
   suite_add_tcase(s, tc_enum_helper);

   TCase *tc_int_helper = tcase_create("Integer Helper");
   tcase_add_checked_fixture(tc_int_helper, setup, teardown);
   tcase_add_test(tc_int_helper, test_tool_add_integer_param);
   suite_add_tcase(s, tc_int_helper);

   TCase *tc_mail = tcase_create("Mail Schema");
   tcase_add_checked_fixture(tc_mail, setup, teardown);
   tcase_add_test(tc_mail, test_tool_build_mail_schema_structure);
   tcase_add_test(tc_mail, test_tool_build_mail_schema_action_enum);
   tcase_add_test(tc_mail, test_tool_build_mail_schema_to_param);
   tcase_add_test(tc_mail, test_tool_build_mail_schema_body_param);
   tcase_add_test(tc_mail, test_tool_build_mail_schema_id_param);
   tcase_add_test(tc_mail, test_tool_build_mail_schema_required);
   suite_add_tcase(s, tc_mail);
   ```

9. Update existing `test_tool_build_all` test to verify 6 tools instead of 5.

10. Run `make check` - expect assertion failures (stubs return NULL/do nothing)

### Green
1. Implement `ik_tool_add_enum_param()` in `src/tool.c`:
   ```c
   void ik_tool_add_enum_param(yyjson_mut_doc *doc,
                               yyjson_mut_val *properties,
                               const char *name,
                               const char *description,
                               const char *enum_values[],
                               size_t enum_count)
   {
       assert(doc != NULL); // LCOV_EXCL_BR_LINE
       assert(properties != NULL); // LCOV_EXCL_BR_LINE
       assert(name != NULL); // LCOV_EXCL_BR_LINE
       assert(description != NULL); // LCOV_EXCL_BR_LINE
       assert(enum_values != NULL); // LCOV_EXCL_BR_LINE
       assert(enum_count > 0); // LCOV_EXCL_BR_LINE

       yyjson_mut_val *param = yyjson_mut_obj(doc);
       if (param == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       if (!yyjson_mut_obj_add_str(doc, param, "type", "string")) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add type field to parameter"); // LCOV_EXCL_LINE
       }

       if (!yyjson_mut_obj_add_str(doc, param, "description", description)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add description field to parameter"); // LCOV_EXCL_LINE
       }

       // Build enum array
       yyjson_mut_val *enum_arr = yyjson_mut_arr(doc);
       if (enum_arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       for (size_t i = 0; i < enum_count; i++) {
           if (!yyjson_mut_arr_add_str(doc, enum_arr, enum_values[i])) { // LCOV_EXCL_BR_LINE
               PANIC("Failed to add enum value"); // LCOV_EXCL_LINE
           }
       }

       if (!yyjson_mut_obj_add_val(doc, param, "enum", enum_arr)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add enum field to parameter"); // LCOV_EXCL_LINE
       }

       if (!yyjson_mut_obj_add_val(doc, properties, name, param)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add parameter to properties"); // LCOV_EXCL_LINE
       }
   }
   ```

2. Implement `ik_tool_add_integer_param()` in `src/tool.c`:
   ```c
   void ik_tool_add_integer_param(yyjson_mut_doc *doc,
                                  yyjson_mut_val *properties,
                                  const char *name,
                                  const char *description)
   {
       assert(doc != NULL); // LCOV_EXCL_BR_LINE
       assert(properties != NULL); // LCOV_EXCL_BR_LINE
       assert(name != NULL); // LCOV_EXCL_BR_LINE
       assert(description != NULL); // LCOV_EXCL_BR_LINE

       yyjson_mut_val *param = yyjson_mut_obj(doc);
       if (param == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       if (!yyjson_mut_obj_add_str(doc, param, "type", "integer")) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add type field to parameter"); // LCOV_EXCL_LINE
       }

       if (!yyjson_mut_obj_add_str(doc, param, "description", description)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add description field to parameter"); // LCOV_EXCL_LINE
       }

       if (!yyjson_mut_obj_add_val(doc, properties, name, param)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add parameter to properties"); // LCOV_EXCL_LINE
       }
   }
   ```

3. Implement `ik_tool_build_mail_schema()` in `src/tool.c`:
   ```c
   yyjson_mut_val *ik_tool_build_mail_schema(yyjson_mut_doc *doc)
   {
       assert(doc != NULL); // LCOV_EXCL_BR_LINE

       yyjson_mut_val *schema = yyjson_mut_obj(doc);
       if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

       yyjson_mut_val *function = yyjson_mut_obj(doc);
       if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       if (!yyjson_mut_obj_add_str(doc, function, "name", "mail")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
       if (!yyjson_mut_obj_add_str(doc, function, "description", "Send and receive messages to/from other agents")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

       yyjson_mut_val *properties = yyjson_mut_obj(doc);
       if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       // action parameter with enum constraint
       const char *actions[] = {"inbox", "read", "send"};
       ik_tool_add_enum_param(doc, properties, "action", "Operation to perform", actions, 3);

       // to parameter (string, required for send)
       ik_tool_add_string_param(doc, properties, "to", "Recipient agent ID (required for send)");

       // body parameter (string, required for send)
       ik_tool_add_string_param(doc, properties, "body", "Message body (required for send)");

       // id parameter (integer, required for read)
       ik_tool_add_integer_param(doc, properties, "id", "Message ID (required for read)");

       yyjson_mut_val *parameters = yyjson_mut_obj(doc);
       if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
       if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

       yyjson_mut_val *required = yyjson_mut_arr(doc);
       if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       if (!yyjson_mut_arr_add_str(doc, required, "action")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
       if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
       if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
       if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

       return schema;
   }
   ```

4. Update `ik_tool_build_all()` in `src/tool.c`:
   ```c
   yyjson_mut_val *ik_tool_build_all(yyjson_mut_doc *doc)
   {
       assert(doc != NULL); // LCOV_EXCL_BR_LINE

       // Create array to hold all tools
       yyjson_mut_val *arr = yyjson_mut_arr(doc);
       if (arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

       // Add all 6 tool schemas in order
       yyjson_mut_val *glob_schema = ik_tool_build_glob_schema(doc);
       if (!yyjson_mut_arr_add_val(arr, glob_schema)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add glob schema to array"); // LCOV_EXCL_LINE
       }

       yyjson_mut_val *file_read_schema = ik_tool_build_file_read_schema(doc);
       if (!yyjson_mut_arr_add_val(arr, file_read_schema)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add file_read schema to array"); // LCOV_EXCL_LINE
       }

       yyjson_mut_val *grep_schema = ik_tool_build_grep_schema(doc);
       if (!yyjson_mut_arr_add_val(arr, grep_schema)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add grep schema to array"); // LCOV_EXCL_LINE
       }

       yyjson_mut_val *file_write_schema = ik_tool_build_file_write_schema(doc);
       if (!yyjson_mut_arr_add_val(arr, file_write_schema)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add file_write schema to array"); // LCOV_EXCL_LINE
       }

       yyjson_mut_val *bash_schema = ik_tool_build_bash_schema(doc);
       if (!yyjson_mut_arr_add_val(arr, bash_schema)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add bash schema to array"); // LCOV_EXCL_LINE
       }

       yyjson_mut_val *mail_schema = ik_tool_build_mail_schema(doc);
       if (!yyjson_mut_arr_add_val(arr, mail_schema)) { // LCOV_EXCL_BR_LINE
           PANIC("Failed to add mail schema to array"); // LCOV_EXCL_LINE
       }

       return arr;
   }
   ```

5. Update `src/tool.h` docstring for `ik_tool_build_all()`:
   ```c
   // Build array containing all 6 tool schemas.
   //
   // Creates a JSON array containing all tool schemas in order:
   // 1. glob
   // 2. file_read
   // 3. grep
   // 4. file_write
   // 5. bash
   // 6. mail
   //
   // @param doc The yyjson mutable document to build the array in
   // @return Pointer to the array object (owned by doc), or NULL on error
   yyjson_mut_val *ik_tool_build_all(yyjson_mut_doc *doc);
   ```

6. Run `make check` - expect pass

### Refactor
1. Verify helper functions follow same style as `ik_tool_add_string_param()`
2. Ensure consistent error handling with PANIC() pattern
3. Verify memory ownership (all values owned by yyjson_mut_doc)
4. Consider if enum values should be defined as constants (not necessary for MVP)
5. Run `make lint` - verify clean
6. Run `make coverage` - verify 100%

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_add_enum_param()` helper function exists and tested
- `ik_tool_add_integer_param()` helper function exists and tested
- `ik_tool_build_mail_schema()` returns correct JSON structure:
  - `type: "function"`
  - `function.name: "mail"`
  - `function.description` mentions agents/messages
  - `function.parameters.properties.action` has enum constraint
  - `function.parameters.properties.to` is string type
  - `function.parameters.properties.body` is string type
  - `function.parameters.properties.id` is integer type
  - `function.parameters.required` contains only "action"
- `ik_tool_build_all()` returns array with 6 tools (mail is last)
- 100% test coverage for new code

## Successor Tasks
- mail-tool-dispatch.md - Dispatch mail tool calls to appropriate operations
- mail-tool-results.md - Format tool results as JSON responses
- mail-notification.md - Inject notifications on IDLE with unread mail
