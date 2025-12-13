#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
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

// Test agent->marks is NULL initially
START_TEST(test_agent_marks_null_initially)
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

    talloc_free(ctx);
}
END_TEST

// Test agent->mark_count is 0 initially
START_TEST(test_agent_mark_count_zero_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
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

// Test agent->assistant_response is NULL initially
START_TEST(test_agent_assistant_response_null_initially)
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

    talloc_free(ctx);
}
END_TEST

// Test agent->streaming_line_buffer is NULL initially
START_TEST(test_agent_streaming_line_buffer_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->streaming_line_buffer);

    talloc_free(ctx);
}
END_TEST

// Test agent->http_error_message is NULL initially
START_TEST(test_agent_http_error_message_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->http_error_message);

    talloc_free(ctx);
}
END_TEST

// Test agent->response_model is NULL initially
START_TEST(test_agent_response_model_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->response_model);

    talloc_free(ctx);
}
END_TEST

// Test agent->response_finish_reason is NULL initially
START_TEST(test_agent_response_finish_reason_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
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

// Test ik_agent_generate_uuid() returns valid 22-char base64url string
START_TEST(test_generate_uuid_returns_valid_string)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char *uuid = ik_agent_generate_uuid(ctx);
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

    char *uuid1 = ik_agent_generate_uuid(ctx);
    char *uuid2 = ik_agent_generate_uuid(ctx);
    char *uuid3 = ik_agent_generate_uuid(ctx);

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
    tcase_add_test(tc_core, test_agent_marks_null_initially);
    tcase_add_test(tc_core, test_agent_mark_count_zero_initially);
    tcase_add_test(tc_core, test_agent_allocated_under_parent);
    tcase_add_test(tc_core, test_agent_can_be_freed);
    tcase_add_test(tc_core, test_agent_state_idle_initially);
    tcase_add_test(tc_core, test_agent_multi_created_initially);
    tcase_add_test(tc_core, test_agent_curl_still_running_zero_initially);
    tcase_add_test(tc_core, test_agent_assistant_response_null_initially);
    tcase_add_test(tc_core, test_agent_streaming_line_buffer_null_initially);
    tcase_add_test(tc_core, test_agent_http_error_message_null_initially);
    tcase_add_test(tc_core, test_agent_response_model_null_initially);
    tcase_add_test(tc_core, test_agent_response_finish_reason_null_initially);
    tcase_add_test(tc_core, test_agent_response_completion_tokens_zero_initially);
    tcase_add_test(tc_core, test_generate_uuid_returns_valid_string);
    tcase_add_test(tc_core, test_generate_uuid_produces_different_uuids);
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
