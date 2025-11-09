// Protocol serialization unit tests

#include <check.h>
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include <ctype.h>
#include "../../../src/protocol.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test message serialization
START_TEST(test_protocol_serialize_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create a message manually for serialization testing
    ik_protocol_msg_t *msg = talloc_zero(ctx, ik_protocol_msg_t);
    msg->sess_id = talloc_strdup(msg, "VQ6EAOKbQdSnFkRmVUQAAA");
    msg->corr_id = talloc_strdup(msg, "8fKm3pLxTdOqZ1YnHjW9Gg");
    msg->type = talloc_strdup(msg, "user_query");
    msg->payload = json_object();
    json_object_set_new(msg->payload, "test", json_string("data"));

    // Manually decref payload since we're not using the constructor functions
    json_t *payload_to_cleanup = msg->payload;

    // Serialize
    res_t res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(is_ok(&res));

    char *json_str = (char *)res.ok;
    ck_assert_ptr_nonnull(json_str);

    // Parse the serialized JSON to verify structure
    json_error_t jerr;
    json_t *root = json_loads(json_str, 0, &jerr);
    ck_assert_ptr_nonnull(root);

    json_t *sess_id = json_object_get(root, "sess_id");
    ck_assert_str_eq(json_string_value(sess_id), "VQ6EAOKbQdSnFkRmVUQAAA");

    json_t *corr_id = json_object_get(root, "corr_id");
    ck_assert_str_eq(json_string_value(corr_id), "8fKm3pLxTdOqZ1YnHjW9Gg");

    json_t *type = json_object_get(root, "type");
    ck_assert_str_eq(json_string_value(type), "user_query");

    json_t *payload = json_object_get(root, "payload");
    ck_assert(json_is_object(payload));

    json_decref(root);
    json_decref(payload_to_cleanup);
    talloc_free(ctx);
}

END_TEST
// Test round-trip (parse -> serialize -> parse)
START_TEST(test_protocol_serialize_round_trip)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *original_json =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    // Parse
    res_t res = ik_protocol_msg_parse(ctx, original_json);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg1 = (ik_protocol_msg_t *)res.ok;

    // Serialize
    res = ik_protocol_msg_serialize(ctx, msg1);
    ck_assert(is_ok(&res));
    char *serialized = (char *)res.ok;

    // Parse again
    res = ik_protocol_msg_parse(ctx, serialized);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg2 = (ik_protocol_msg_t *)res.ok;

    // Verify fields match original
    ck_assert_str_eq(msg2->sess_id, "VQ6EAOKbQdSnFkRmVUQAAA");
    ck_assert_str_eq(msg2->corr_id, "8fKm3pLxTdOqZ1YnHjW9Gg");
    ck_assert_str_eq(msg2->type, "user_query");

    talloc_free(ctx);
}

END_TEST
// Test serialize OOM when allocating result string
START_TEST(test_protocol_serialize_oom_result_alloc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Fail the allocation when copying the serialized string
    // json_object and json_dumps will succeed, but talloc_strdup will fail (3rd allocation)
    oom_test_fail_after_n_calls(3);

    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test serialize OOM when creating JSON object
START_TEST(test_protocol_serialize_oom_json_object)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Fail json_object() call
    oom_test_fail_next_alloc();

    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test serialize OOM when calling json_dumps
START_TEST(test_protocol_serialize_oom_json_dumps)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Fail after json_object succeeds, fail json_dumps
    oom_test_fail_after_n_calls(2);

    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
static Suite *protocol_serialize_suite(void)
{
    Suite *s = suite_create("Protocol Serialize");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_protocol_serialize_message);
    tcase_add_test(tc_core, test_protocol_serialize_round_trip);
    tcase_add_test(tc_core, test_protocol_serialize_oom_result_alloc);
    tcase_add_test(tc_core, test_protocol_serialize_oom_json_object);
    tcase_add_test(tc_core, test_protocol_serialize_oom_json_dumps);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = protocol_serialize_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
