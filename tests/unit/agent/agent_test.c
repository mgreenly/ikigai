#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/uuid.h"
#include "../../../src/openai/client.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

// Helper function to check if string contains only base64url characters
static bool is_base64url(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (!isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// Test ik_agent_create() succeeds
START_TEST(test_agent_create_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal shared context (we just need a pointer for this test)
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    talloc_free(ctx);
}
END_TEST

// Test agent->uuid is non-NULL and exactly 22 characters
START_TEST(test_agent_uuid_non_null_and_22_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->uuid);
    ck_assert_uint_eq(strlen(agent->uuid), 22);

    talloc_free(ctx);
}
END_TEST

// Test agent->uuid contains only base64url characters
START_TEST(test_agent_uuid_base64url_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->uuid);
    ck_assert(is_base64url(agent->uuid, strlen(agent->uuid)));

    talloc_free(ctx);
}
END_TEST

// Test agent->name is NULL initially
START_TEST(test_agent_name_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->name);

    talloc_free(ctx);
}
END_TEST

// Test agent->parent_uuid is NULL for root agent
START_TEST(test_agent_parent_uuid_null_for_root)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->parent_uuid);

    talloc_free(ctx);
}
END_TEST

// Test agent->parent_uuid matches input when provided
START_TEST(test_agent_parent_uuid_matches_input)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    const char *parent_uuid = "test-parent-uuid-12345";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, parent_uuid, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->parent_uuid);
    ck_assert_str_eq(agent->parent_uuid, parent_uuid);

    talloc_free(ctx);
}
END_TEST

// Test agent->shared matches input
START_TEST(test_agent_shared_matches_input)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_eq(agent->shared, shared);

    talloc_free(ctx);
}
END_TEST

// Test agent->scrollback is initialized
START_TEST(test_agent_scrollback_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->scrollback);

    talloc_free(ctx);
}
END_TEST

// Test agent->layer_cake is initialized
START_TEST(test_agent_layer_cake_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->layer_cake);

    talloc_free(ctx);
}
END_TEST

// Test all layer pointers are non-NULL
START_TEST(test_agent_all_layers_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->scrollback_layer);
    ck_assert_ptr_nonnull(agent->spinner_layer);
    ck_assert_ptr_nonnull(agent->separator_layer);
    ck_assert_ptr_nonnull(agent->input_layer);
    ck_assert_ptr_nonnull(agent->completion_layer);

    talloc_free(ctx);
}
END_TEST

// Test agent->viewport_offset is 0 initially
START_TEST(test_agent_viewport_offset_zero)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_uint_eq(agent->viewport_offset, 0);

    talloc_free(ctx);
}
END_TEST

// Test agent->input_buffer is initialized
START_TEST(test_agent_input_buffer_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    talloc_free(ctx);
}
END_TEST

// Test agent->separator_visible is true initially
START_TEST(test_agent_separator_visible_true)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert(agent->separator_visible == true);

    talloc_free(ctx);
}
END_TEST

// Test agent->input_buffer_visible is true initially
START_TEST(test_agent_input_buffer_visible_true)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert(agent->input_buffer_visible == true);

    talloc_free(ctx);
}
END_TEST

// Test agent_ctx is allocated under provided parent
START_TEST(test_agent_allocated_under_parent)
{
    TALLOC_CTX *parent = talloc_new(NULL);
    ck_assert_ptr_nonnull(parent);

    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(parent, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    // Verify parent relationship
    TALLOC_CTX *actual_parent = talloc_parent(agent);
    ck_assert_ptr_eq(actual_parent, parent);

    talloc_free(parent);
}
END_TEST

// Test agent_ctx can be freed via talloc_free
START_TEST(test_agent_can_be_freed)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    // Free agent directly
    int result = talloc_free(agent);
    ck_assert_int_eq(result, 0);  // talloc_free returns 0 on success

    talloc_free(ctx);
}
END_TEST

// Test agent->conversation is initialized
START_TEST(test_agent_conversation_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->conversation);

    talloc_free(ctx);
}
END_TEST

// Test agent->marks is NULL and mark_count is 0 initially
START_TEST(test_agent_marks_and_count_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->marks);
    ck_assert_uint_eq(agent->mark_count, 0);
    talloc_free(ctx);
}
END_TEST

// Test agent->state is IK_AGENT_STATE_IDLE initially
START_TEST(test_agent_state_idle_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);

    talloc_free(ctx);
}
END_TEST

// Test agent->multi is created initially
START_TEST(test_agent_multi_created_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->multi);

    talloc_free(ctx);
}
END_TEST

// Test agent->curl_still_running is 0 initially
START_TEST(test_agent_curl_still_running_zero_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->curl_still_running, 0);

    talloc_free(ctx);
}
END_TEST

// Test agent response-related fields are NULL initially
START_TEST(test_agent_response_fields_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->assistant_response);
    ck_assert_ptr_null(agent->streaming_line_buffer);
    ck_assert_ptr_null(agent->http_error_message);
    ck_assert_ptr_null(agent->response_model);
    ck_assert_ptr_null(agent->response_finish_reason);
    talloc_free(ctx);
}
END_TEST

// Test agent->response_completion_tokens is 0 initially
START_TEST(test_agent_response_completion_tokens_zero_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->response_completion_tokens, 0);

    talloc_free(ctx);
}
END_TEST

// Test agent tool thread fields initialized correctly
START_TEST(test_agent_tool_fields_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->pending_tool_call);
    ck_assert(agent->tool_thread_running == false);
    ck_assert(agent->tool_thread_complete == false);
    ck_assert_int_eq(agent->tool_iteration_count, 0);
    talloc_free(ctx);
}
END_TEST

// Test agent->spinner_state is properly initialized
START_TEST(test_agent_spinner_state_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_uint_eq(agent->spinner_state.frame_index, 0);
    ck_assert(agent->spinner_state.visible == false);

    talloc_free(ctx);
}
END_TEST

// Test agent->completion is NULL initially
START_TEST(test_agent_completion_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->completion);

    talloc_free(ctx);
}
END_TEST

// Test mutex is initialized and can be locked/unlocked
START_TEST(test_agent_tool_thread_mutex_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    // Test that we can lock and unlock the mutex
    int lock_result = pthread_mutex_lock(&agent->tool_thread_mutex);
    ck_assert_int_eq(lock_result, 0);

    int unlock_result = pthread_mutex_unlock(&agent->tool_thread_mutex);
    ck_assert_int_eq(unlock_result, 0);

    talloc_free(ctx);
}
END_TEST

// Test ik_generate_uuid() returns valid 22-char base64url string
START_TEST(test_generate_uuid_returns_valid_string)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char *uuid = ik_generate_uuid(ctx);
    ck_assert_ptr_nonnull(uuid);
    ck_assert_uint_eq(strlen(uuid), 22);
    ck_assert(is_base64url(uuid, 22));

    talloc_free(ctx);
}
END_TEST

// Test that multiple UUIDs are different (with seeded random)
START_TEST(test_generate_uuid_produces_different_uuids)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Seed random number generator for reproducibility
    srand((unsigned int)time(NULL));

    char *uuid1 = ik_generate_uuid(ctx);
    char *uuid2 = ik_generate_uuid(ctx);
    char *uuid3 = ik_generate_uuid(ctx);

    ck_assert_ptr_nonnull(uuid1);
    ck_assert_ptr_nonnull(uuid2);
    ck_assert_ptr_nonnull(uuid3);

    // All should be different (with very high probability)
    ck_assert_str_ne(uuid1, uuid2);
    ck_assert_str_ne(uuid2, uuid3);
    ck_assert_str_ne(uuid1, uuid3);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_copy_conversation succeeds with messages
START_TEST(test_agent_copy_conversation)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    // Create parent agent
    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(parent);

    // Add messages to parent - some with data_json, some without
    res_t msg_res = ik_openai_msg_create(parent->conversation, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    res = ik_openai_conversation_add_msg(parent->conversation, msg_res.ok);
    ck_assert(is_ok(&res));

    // Message without data_json
    msg_res = ik_openai_msg_create(parent->conversation, "assistant", "Hi there");
    ck_assert(is_ok(&msg_res));
    res = ik_openai_conversation_add_msg(parent->conversation, msg_res.ok);
    ck_assert(is_ok(&res));

    // Message with data_json
    msg_res = ik_openai_msg_create(parent->conversation, "assistant", "With data");
    ck_assert(is_ok(&msg_res));
    ik_msg_t *msg_with_data = msg_res.ok;
    msg_with_data->data_json = talloc_strdup(msg_with_data, "{\"test\": true}");
    res = ik_openai_conversation_add_msg(parent->conversation, msg_with_data);
    ck_assert(is_ok(&res));

    // Create child agent
    ik_agent_ctx_t *child = NULL;
    res = ik_agent_create(ctx, shared, parent->uuid, &child);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child);

    // Copy conversation (child, parent)
    res = ik_agent_copy_conversation(child, parent);
    ck_assert(is_ok(&res));

    // Verify child has messages
    ck_assert_ptr_nonnull(child->conversation);
    ck_assert_uint_eq(child->conversation->message_count, parent->conversation->message_count);

    talloc_free(ctx);
}
END_TEST

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

// Test agent->repl backpointer is NULL initially (no repl context yet)
START_TEST(test_agent_repl_backpointer_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->repl);

    talloc_free(ctx);
}
END_TEST

static Suite *agent_suite(void)
{
    Suite *s = suite_create("Agent Context");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_agent_create_success);
    tcase_add_test(tc_core, test_agent_uuid_non_null_and_22_chars);
    tcase_add_test(tc_core, test_agent_uuid_base64url_chars);
    tcase_add_test(tc_core, test_agent_name_null_initially);
    tcase_add_test(tc_core, test_agent_parent_uuid_null_for_root);
    tcase_add_test(tc_core, test_agent_parent_uuid_matches_input);
    tcase_add_test(tc_core, test_agent_shared_matches_input);
    tcase_add_test(tc_core, test_agent_scrollback_initialized);
    tcase_add_test(tc_core, test_agent_layer_cake_initialized);
    tcase_add_test(tc_core, test_agent_all_layers_initialized);
    tcase_add_test(tc_core, test_agent_viewport_offset_zero);
    tcase_add_test(tc_core, test_agent_input_buffer_initialized);
    tcase_add_test(tc_core, test_agent_separator_visible_true);
    tcase_add_test(tc_core, test_agent_input_buffer_visible_true);
    tcase_add_test(tc_core, test_agent_conversation_initialized);
    tcase_add_test(tc_core, test_agent_marks_and_count_initially);
    tcase_add_test(tc_core, test_agent_allocated_under_parent);
    tcase_add_test(tc_core, test_agent_can_be_freed);
    tcase_add_test(tc_core, test_agent_state_idle_initially);
    tcase_add_test(tc_core, test_agent_multi_created_initially);
    tcase_add_test(tc_core, test_agent_curl_still_running_zero_initially);
    tcase_add_test(tc_core, test_agent_response_fields_null_initially);
    tcase_add_test(tc_core, test_agent_response_completion_tokens_zero_initially);
    tcase_add_test(tc_core, test_agent_tool_fields_initialized);
    tcase_add_test(tc_core, test_agent_spinner_state_initialized);
    tcase_add_test(tc_core, test_agent_completion_null_initially);
    tcase_add_test(tc_core, test_agent_tool_thread_mutex_initialized);
    tcase_add_test(tc_core, test_generate_uuid_returns_valid_string);
    tcase_add_test(tc_core, test_generate_uuid_produces_different_uuids);
    tcase_add_test(tc_core, test_agent_copy_conversation);
    tcase_add_test(tc_core, test_agent_create_sets_created_at);
    tcase_add_test(tc_core, test_agent_repl_backpointer_null_initially);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
