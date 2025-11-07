// Protocol module unit tests

#include <check.h>
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include <ctype.h>
#include "../../src/protocol.h"
#include "../../src/error.h"
#include "../test_utils.h"

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
    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);

    // We expect an error at this point (not implemented yet)
    // But the function must exist to compile
    ck_assert(ik_is_err(&res) || ik_is_ok(&res));

    talloc_free(ctx);
}

END_TEST
// Test UUID generation produces 22-character string
START_TEST(test_protocol_generate_uuid)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_result_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_ok(&res));

    char *uuid = (char *)res.ok;
    ck_assert_ptr_nonnull(uuid);
    ck_assert_int_eq((int)strlen(uuid), 22);

    // Verify it's base64url (no +, /, or =)
    for (int i = 0; i < 22; i++) {
        char c = uuid[i];
        ck_assert(isalnum(c) || c == '-' || c == '_');
        ck_assert(c != '+');
        ck_assert(c != '/');
        ck_assert(c != '=');
    }

    talloc_free(ctx);
}

END_TEST
// Test UUID uniqueness
START_TEST(test_protocol_uuid_uniqueness)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

#define NUM_UUIDS 100
    char *uuids[NUM_UUIDS];

    // Generate 100 UUIDs
    for (int i = 0; i < NUM_UUIDS; i++) {
        ik_result_t res = ik_protocol_generate_uuid(ctx);
        ck_assert(ik_is_ok(&res));
        uuids[i] = (char *)res.ok;
    }

    // Verify all are different
    for (int i = 0; i < NUM_UUIDS; i++) {
        for (int j = i + 1; j < NUM_UUIDS; j++) {
            ck_assert_str_ne(uuids[i], uuids[j]);
        }
    }

    talloc_free(ctx);
}

END_TEST
// Test UUID generation OOM when allocating result buffer
START_TEST(test_protocol_generate_uuid_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail the array allocation for the base64url result
    oom_test_fail_next_alloc();

    ik_result_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test parsing valid message
START_TEST(test_protocol_parse_valid_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test missing payload
START_TEST(test_protocol_parse_missing_payload)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"}";

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test wrong type for type field (number instead of string)
START_TEST(test_protocol_parse_wrong_type_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str = "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":42," "\"payload\":{\"test\":\"data\"}}";

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

    talloc_free(ctx);
}

END_TEST
// Test wrong type for payload field (array instead of object)
START_TEST(test_protocol_parse_wrong_type_payload)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *json_str = "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\"," "\"type\":\"user_query\"," "\"payload\":[1,2,3]}";

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_PARSE);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));

    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;
    ck_assert_ptr_null(msg->corr_id);

    talloc_free(ctx);
}

END_TEST
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
    ik_result_t res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(ik_is_ok(&res));

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
// Test round-trip (parse → serialize → parse)
START_TEST(test_protocol_serialize_round_trip)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *original_json =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    // Parse
    ik_result_t res = ik_protocol_msg_parse(ctx, original_json);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg1 = (ik_protocol_msg_t *)res.ok;

    // Serialize
    res = ik_protocol_msg_serialize(ctx, msg1);
    ck_assert(ik_is_ok(&res));
    char *serialized = (char *)res.ok;

    // Parse again
    res = ik_protocol_msg_parse(ctx, serialized);
    ck_assert(ik_is_ok(&res));
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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Fail the allocation when copying the serialized string
    // json_object and json_dumps will succeed, but talloc_strdup will fail (3rd allocation)
    oom_test_fail_after_n_calls(3);

    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Fail json_object() call
    oom_test_fail_next_alloc();

    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Fail after json_object succeeds, fail json_dumps
    oom_test_fail_after_n_calls(2);

    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test error message constructor
START_TEST(test_protocol_create_error_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_result_t res = ik_protocol_msg_create_err(ctx,
                                                 "sess123",
                                                 "corr456",
                                                 "server", "test error");
    ck_assert(ik_is_ok(&res));

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

    ik_result_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating sess_id
START_TEST(test_protocol_create_err_oom_sess_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(1);     // Fail on 2nd allocation (sess_id)

    ik_result_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating corr_id
START_TEST(test_protocol_create_err_oom_corr_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(2);     // Fail on 3rd allocation (corr_id)

    ik_result_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when allocating type
START_TEST(test_protocol_create_err_oom_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(4);     // Fail on 5th allocation (type string copy)

    ik_result_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
// Test create_err OOM when creating payload json_object
START_TEST(test_protocol_create_err_oom_payload)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_after_n_calls(5);     // Fail on 5th allocation (payload json_object)

    ik_result_t res = ik_protocol_msg_create_err(ctx, "sess", "corr", "src", "msg");
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_create_assistant_resp(ctx,
                                                            "sess789",
                                                            "corr012",
                                                            payload);
    ck_assert(ik_is_ok(&res));

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

    ik_result_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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

    ik_result_t res = ik_protocol_msg_create_assistant_resp(ctx, "sess", "corr", payload);
    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_OOM);

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
    ik_result_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(ik_is_ok(&res));
    char *uuid = (char *)res.ok;
    ck_assert_ptr_nonnull(uuid);

    // Parse message
    const char *json_str =
        "{\"sess_id\":\"VQ6EAOKbQdSnFkRmVUQAAA\","
        "\"corr_id\":\"8fKm3pLxTdOqZ1YnHjW9Gg\"," "\"type\":\"user_query\"," "\"payload\":{\"test\":\"data\"}}";

    res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Serialize message
    res = ik_protocol_msg_serialize(ctx, msg);
    ck_assert(ik_is_ok(&res));

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

    ik_result_t res = ik_protocol_msg_parse(ctx, json_str);
    ck_assert(ik_is_ok(&res));
    ik_protocol_msg_t *msg = (ik_protocol_msg_t *)res.ok;

    // Payload should be valid JSON object
    ck_assert(json_is_object(msg->payload));

    // Destructor will clean up jansson ref automatically

    // Clean up talloc allocations
    talloc_free(ctx);

    // No leaks if test passes (verified by valgrind)
}

END_TEST static Suite *protocol_suite(void)
{
    Suite *s = suite_create("Protocol");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_protocol_msg_type_exists);
    tcase_add_test(tc_core, test_protocol_msg_parse_function_exists);
    tcase_add_test(tc_core, test_protocol_generate_uuid);
    tcase_add_test(tc_core, test_protocol_uuid_uniqueness);
    tcase_add_test(tc_core, test_protocol_generate_uuid_oom);
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
    tcase_add_test(tc_core, test_protocol_serialize_message);
    tcase_add_test(tc_core, test_protocol_serialize_round_trip);
    tcase_add_test(tc_core, test_protocol_serialize_oom_result_alloc);
    tcase_add_test(tc_core, test_protocol_serialize_oom_json_object);
    tcase_add_test(tc_core, test_protocol_serialize_oom_json_dumps);
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
    Suite *s = protocol_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
