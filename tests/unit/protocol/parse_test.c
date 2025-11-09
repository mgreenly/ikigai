// Protocol parsing unit tests

#include <check.h>
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include <ctype.h>
#include "../../../src/protocol.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test parsing valid message
START_TEST(test_protocol_parse_valid_message) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));

    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->sess_id, "VQ6EAOKbQdSnFkRmVUQAAA");
    ck_assert_str_eq(msg->corr_id, "8fKm3pLxTdOqZ1YnHjW9Gg");
    ck_assert_str_eq(msg->type, "user_query");
    ck_assert_ptr_nonnull(msg->payload);
    ck_assert(json_is_object(msg->payload));

    talloc_free(ctx);
}

END_TEST
// Test parsing invalid JSON
START_TEST(test_protocol_parse_invalid_json)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *json_str = "{invalid json}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test parsing valid JSON that's not an object
START_TEST(test_protocol_parse_json_not_object)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Valid JSON array, but not an object
    const char *json_str = "[1,2,3]";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test parse OOM when allocating message struct
START_TEST(test_protocol_parse_oom_msg_alloc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *json_str = "{\"sess_id\":\"test\",\"type\":\"user_query\",\"payload\":{}}";

    // Fail the talloc_zero allocation for message struct
    oom_test_fail_next_alloc();

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test OOM when allocating sess_id string
START_TEST(test_protocol_parse_oom_sess_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *json_str = "{\"sess_id\":\"test\",\"type\":\"user_query\",\"payload\":{}}";

    // Skip first allocation (msg struct), fail second (sess_id string)
    oom_test_fail_after_n_calls(2);

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test OOM when allocating corr_id string (optional field)
START_TEST(test_protocol_parse_oom_corr_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *json_str = "{\"sess_id\":\"test\",\"corr_id\":\"corr123\",\"type\":\"user_query\",\"payload\":{}}";

    // Skip msg struct and sess_id, fail corr_id
    oom_test_fail_after_n_calls(3);

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test OOM when allocating type string
START_TEST(test_protocol_parse_oom_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // No corr_id, so only 3 allocations: msg, sess_id, type
    const char *json_str = "{\"sess_id\":\"test\",\"type\":\"user_query\",\"payload\":{}}";

    // Skip msg struct and sess_id, fail type (3rd allocation)
    oom_test_fail_after_n_calls(3);

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test missing sess_id
START_TEST(test_protocol_parse_missing_sess_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test missing type
START_TEST(test_protocol_parse_missing_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test missing payload
START_TEST(test_protocol_parse_missing_payload)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test wrong type for sess_id (number instead of string)
START_TEST(test_protocol_parse_wrong_type_sess_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"sess_id\":12345,"
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test wrong type for type field (number instead of string)
START_TEST(test_protocol_parse_wrong_type_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str = "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":42," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test wrong type for payload field (array instead of object)
START_TEST(test_protocol_parse_wrong_type_payload)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str = "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":\"user_query\"," "\"payload\":[1,2,3]}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test wrong type for corr_id field (number instead of string) - should be treated as missing
START_TEST(test_protocol_parse_wrong_type_corr_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":999," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));

    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;
    ck_assert_ptr_null(msg->corr_id);   // Wrong type treated as missing

    talloc_free(ctx);
}

END_TEST
// Test corr_id is optional
START_TEST(test_protocol_parse_corr_id_optional)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(is_ok(&res));

    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;
    ck_assert_ptr_null(msg->corr_id);

    talloc_free(ctx);
}

END_TEST
static Suite *protocol_parse_suite(void)
{
    Suite *s = suite_create("Protocol Parse");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_protocol_parse_valid_message);
    tcase_add_test(tc_core, test_protocol_parse_invalid_json);
    tcase_add_test(tc_core, test_protocol_parse_json_not_object);
    tcase_add_test(tc_core, test_protocol_parse_oom_msg_alloc);
    tcase_add_test(tc_core, test_protocol_parse_oom_sess_id);
    tcase_add_test(tc_core, test_protocol_parse_oom_corr_id);
    tcase_add_test(tc_core, test_protocol_parse_oom_type);
    tcase_add_test(tc_core, test_protocol_parse_missing_sess_id);
    tcase_add_test(tc_core, test_protocol_parse_missing_type);
    tcase_add_test(tc_core, test_protocol_parse_missing_payload);
    tcase_add_test(tc_core, test_protocol_parse_wrong_type_sess_id);
    tcase_add_test(tc_core, test_protocol_parse_wrong_type_type);
    tcase_add_test(tc_core, test_protocol_parse_wrong_type_payload);
    tcase_add_test(tc_core, test_protocol_parse_wrong_type_corr_id);
    tcase_add_test(tc_core, test_protocol_parse_corr_id_optional);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = protocol_parse_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
