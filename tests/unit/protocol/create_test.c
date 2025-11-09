// Protocol message creation unit tests

#include <check.h>
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include <ctype.h>
#include "../../../src/protocol.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test that we can use ik_protocol_msg_t struct
START_TEST(test_protocol_msg_type_exists) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // This test will fail to compile until we create protocol.h with the struct
    ik_protocol_msg_t msg;
    msg.sess_id = NULL;
    msg.corr_id = NULL;
    msg.type = NULL;
    msg.payload = NULL;

    ck_assert_ptr_null(msg.sess_id);

    talloc_free(ctx);
}

END_TEST
// Test that we can call ik_protocol_msg_parse()
START_TEST(test_protocol_msg_parse_function_exists)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // This will fail to compile until we declare the function
    const char *json_str = "{\"sess_id\":\"test\",\"type\":\"user_query\",\"payload\":{}}";
    res_t res = ik_protocol_msg_parse(ctx, json_str);

    // We expect an error at this point (not implemented yet)
    // But the function must exist to compile
    ck_assert(is_err(&res) || is_ok(&res));

    talloc_free(ctx);
}

END_TEST
// Test error message constructor
START_TEST(test_protocol_create_error_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_protocol_msg_create_err(ctx,
                                           "sess123",
                                           "corr456",
                                           "server", "test error");
    ck_assert(is_ok(&res));

    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->sess_id, "sess123");
    ck_assert_str_eq(msg->corr_id, "corr456");
    ck_assert_str_eq(msg->type, "error");

    // Verify payload structure
    ck_assert_ptr_nonnull(msg->payload);
    ck_assert(json_is_object(msg->payload));

    json_t *source = json_object_get(msg->payload, "source");
    ck_assert_ptr_nonnull(source);
    ck_assert_str_eq(json_string_value(source), "server");

    json_t *message = json_object_get(msg->payload, "message");
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(json_string_value(message), "test error");

    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating message struct
START_TEST(test_protocol_create_err_oom_msg_alloc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_next_alloc();

    res_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating sess_id
START_TEST(test_protocol_create_err_oom_sess_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(1);     // Fail on 2nd allocation (sess_id)

    res_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating corr_id
START_TEST(test_protocol_create_err_oom_corr_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(2);     // Fail on 3rd allocation (corr_id)

    res_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating type
START_TEST(test_protocol_create_err_oom_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(4);     // Fail on 5th allocation (type string copy)

    res_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when creating payload json_object
START_TEST(test_protocol_create_err_oom_payload)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(5);     // Fail on 5th allocation (payload json_object)

    res_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test assistant response constructor
START_TEST(test_protocol_create_assistant_response)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create test payload
    json_t *payload = json_object();
    json_object_set_new(payload, "content", json_string("Hello there"));
    json_object_set_new(payload, "model", json_string("gpt-4o-mini"));

    res_t res = ik_protocol_msg_create_assistant_resp(ctx,
                                                      "sess789",
                                                      "corr012",
                                                      payload);
    ck_assert(is_ok(&res));

    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->sess_id, "sess789");
    ck_assert_str_eq(msg->corr_id, "corr012");
    ck_assert_str_eq(msg->type, "assistant_response");

    // Verify payload matches input
    ck_assert_ptr_eq(msg->payload, payload);

    // Payload ownership transferred to message, destructor will clean it up
    talloc_free(ctx);
}

END_TEST
// Test create_assistant_resp OOM when allocating message struct
START_TEST(test_protocol_create_assistant_resp_oom_msg_alloc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    json_t *payload = json_object();

    oom_test_fail_next_alloc();

    res_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    json_decref(payload);
    talloc_free(ctx);
}

END_TEST
// Test create_assistant_resp OOM when allocating sess_id
START_TEST(test_protocol_create_assistant_resp_oom_sess_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    json_t *payload = json_object();

    oom_test_fail_after_n_calls(1);     // Fail on 2nd allocation (sess_id)

    res_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    json_decref(payload);
    talloc_free(ctx);
}

END_TEST
// Test create_assistant_resp OOM when allocating corr_id
START_TEST(test_protocol_create_assistant_resp_oom_corr_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    json_t *payload = json_object();

    oom_test_fail_after_n_calls(2);     // Fail on 3rd allocation (corr_id)

    res_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    json_decref(payload);
    talloc_free(ctx);
}

END_TEST
// Test create_assistant_resp OOM when allocating type
START_TEST(test_protocol_create_assistant_resp_oom_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    json_t *payload = json_object();

    oom_test_fail_after_n_calls(3);     // Fail on 4th allocation (type)

    res_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    json_decref(payload);
    talloc_free(ctx);
}

END_TEST
// Test memory management - talloc cleanup
START_TEST(test_protocol_memory_cleanup)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Generate UUID
    res_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(is_ok(&res));
    char *uuid = (char *)res.ok;
    ck_assert_ptr_nonnull(uuid);

    // Parse message
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Serialize message
    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(is_ok(&res));

    // All allocations should be children of ctx
    // Single talloc_free should clean everything up
    talloc_free(ctx);

    // Test passes if no memory leaks (verified by valgrind in CI)
}

END_TEST
// Test jansson cleanup
START_TEST(test_protocol_jansson_cleanup)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Parse creates json_t objects that must be properly cleaned up
    const char *json_str =
        "{\"sess_id\":\"test123\"," "\"type\":\"user_query\"," "\"payload\":{\"nested\":{\"data\":true}}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Payload should be valid JSON object
    ck_assert(json_is_object(msg->payload));

    // Destructor will clean up jansson ref automatically

    // Clean up talloc allocations
    talloc_free(ctx);

    // No leaks if test passes (verified by valgrind)
}

END_TEST
static Suite *protocol_create_suite(void)
{
    Suite *s = suite_create("Protocol Create");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_protocol_msg_type_exists);
    tcase_add_test(tc_core, test_protocol_msg_parse_function_exists);
    tcase_add_test(tc_core, test_protocol_create_error_message);
    tcase_add_test(tc_core, test_protocol_create_err_oom_msg_alloc);
    tcase_add_test(tc_core, test_protocol_create_err_oom_sess_id);
    tcase_add_test(tc_core, test_protocol_create_err_oom_corr_id);
    tcase_add_test(tc_core, test_protocol_create_err_oom_type);
    tcase_add_test(tc_core, test_protocol_create_err_oom_payload);
    tcase_add_test(tc_core, test_protocol_create_assistant_response);
    tcase_add_test(tc_core, test_protocol_create_assistant_resp_oom_msg_alloc);
    tcase_add_test(tc_core, test_protocol_create_assistant_resp_oom_sess_id);
    tcase_add_test(tc_core, test_protocol_create_assistant_resp_oom_corr_id);
    tcase_add_test(tc_core, test_protocol_create_assistant_resp_oom_type);
    tcase_add_test(tc_core, test_protocol_memory_cleanup);
    tcase_add_test(tc_core, test_protocol_jansson_cleanup);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = protocol_create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
