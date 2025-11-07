// Protocol integration tests - full message flow end-to-end

#include <check.h>
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include "../../src/protocol.h"
#include "../../src/error.h"
#include "../test_utils.h"

// Test complete message flow: generate IDs, create message, serialize, parse
START_TEST(test_protocol_full_message_flow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // 1. Generate session ID
    ik_result_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_ok(&res));
    char *sess_id = (char *)res.ok;
    ck_assert_int_eq((int)strlen(sess_id), 22);

    // 2. Generate correlation ID
    res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_ok(&res));
    char *corr_id = (char *)res.ok;
    ck_assert_int_eq((int)strlen(corr_id), 22);

    // 3. Create user query message manually
    ik_protocol_msg_t *msg1 = talloc_zero(ctx, ik_protocol_msg_t);
    msg1->sess_id = sess_id;
    msg1->corr_id = corr_id;
    msg1->type = talloc_strdup(msg1, "user_query");
    msg1->payload = json_object();
    json_object_set_new(msg1->payload, "model", json_string("gpt-4o-mini"));
    json_object_set_new(msg1->payload, "temperature", json_real(0.7));

    // 4. Serialize the message
    res = ik_protocol_msg_serialize(ctx, msg1);
    ck_assert(ik_is_ok(&res));
    char *json_str = (char *)res.ok;
    ck_assert_ptr_nonnull(json_str);

    // 5. Parse the serialized message
    res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg2 = (ik_protocol_msg_t *)res.ok;

    // 6. Verify all fields match
    ck_assert_str_eq(msg2->sess_id, sess_id);
    ck_assert_str_eq(msg2->corr_id, corr_id);
    ck_assert_str_eq(msg2->type, "user_query");
    ck_assert(json_is_object(msg2->payload));

    // 7. Create assistant response using constructor
    json_t *resp_payload = json_object();
    json_object_set_new(resp_payload, "content", json_string("Hello!"));

    res = ik_protocol_msg_create_assistant_resp(ctx, sess_id, corr_id, resp_payload);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg3 = (ik_protocol_msg_t *)res.ok;

    // 8. Serialize and parse assistant response
    res = ik_protocol_msg_serialize(ctx, msg3);
    ck_assert(ik_is_ok(&res));
    char *resp_json = (char *)res.ok;

    res = ik_protocol_msg_parse(ctx, resp_json);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg4 = (ik_protocol_msg_t *)res.ok;
    ck_assert_str_eq(msg4->type, "assistant_response");

    // Cleanup - msg1 was manually created without destructor
    json_decref(msg1->payload);
    // msg2, msg3, and msg4 have destructors that will clean up their payloads
    // resp_payload was passed to create_assistant_resp which took ownership
    talloc_free(ctx);
}

END_TEST
// Test error handling flow
START_TEST(test_protocol_error_handling_flow)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Generate IDs
    ik_result_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_ok(&res));
    char *sess_id = (char *)res.ok;

    res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_ok(&res));
    char *corr_id = (char *)res.ok;

    // Create error message
    res = ik_protocol_msg_create_err(ctx, sess_id, corr_id, "openai", "Authentication failed");
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *err_msg = (ik_protocol_msg_t *)res.ok;

    // Verify error structure
    ck_assert_str_eq(err_msg->type, "error");
    json_t *source = json_object_get(err_msg->payload, "source");
    ck_assert_str_eq(json_string_value(source), "openai");

    // Serialize and parse error
    res = ik_protocol_msg_serialize(ctx, err_msg);
    ck_assert(ik_is_ok(&res));
    char *err_json = (char *)res.ok;

    res = ik_protocol_msg_parse(ctx, err_json);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *parsed_err = (ik_protocol_msg_t *)res.ok;

    // Verify parsed error matches
    ck_assert_str_eq(parsed_err->type, "error");
    json_t *parsed_source = json_object_get(parsed_err->payload, "source");
    ck_assert_str_eq(json_string_value(parsed_source), "openai");

    // Cleanup - destructors will clean up payloads automatically
    talloc_free(ctx);
}

END_TEST static Suite *protocol_integration_suite(void)
{
    Suite *s = suite_create("ProtocolIntegration");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_protocol_full_message_flow);
    tcase_add_test(tc_core, test_protocol_error_handling_flow);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = protocol_integration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
