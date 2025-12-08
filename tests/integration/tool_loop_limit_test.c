/**
 * @file tool_loop_limit_test.c
 * @brief End-to-end integration test for tool loop limit behavior
 *
 * Verifies tool loop limit feature works end-to-end.
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "../../src/config.h"
#include "../../src/db/connection.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/error.h"
#include "../../src/msg.h"
#include "../../src/openai/client.h"
#include "../../src/openai/tool_choice.h"
#include "../../src/tool.h"
#include "../test_utils.h"

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup
static void suite_setup(void)
{
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

// Suite-level teardown
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        if (db != NULL) {
            ik_test_db_rollback(db);
        }
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// Helper to count messages in database for a session
static int count_messages(ik_db_ctx_t *db_ctx, int64_t sid, const char *kind)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *query;
    PGresult *result;

    if (kind == NULL) {
        query = "SELECT COUNT(*) FROM messages WHERE session_id = $1";
        const char *params[] = {talloc_asprintf(ctx, "%lld", (long long)sid)};
        result = PQexecParams(db_ctx->conn, query, 1, NULL, params, NULL, NULL, 0);
    } else {
        query = "SELECT COUNT(*) FROM messages WHERE session_id = $1 AND kind = $2";
        const char *params[] = {
            talloc_asprintf(ctx, "%lld", (long long)sid),
            kind
        };
        result = PQexecParams(db_ctx->conn, query, 2, NULL, params, NULL, NULL, 0);
    }

    int count = 0;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        count = atoi(PQgetvalue(result, 0, 0));
    }

    PQclear(result);
    talloc_free(ctx);
    return count;
}

START_TEST(test_tool_result_add_limit_metadata) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *result_json = "{\"output\": \"found errors\", \"count\": 3}";
    int32_t max_tool_turns = 3;

    char *result = ik_tool_result_add_limit_metadata(ctx, result_json, max_tool_turns);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_str_eq(yyjson_get_str(output), "found errors");

    yyjson_val *limit_reached = yyjson_obj_get(root, "limit_reached");
    ck_assert(yyjson_get_bool(limit_reached) == true);

    yyjson_val *limit_message = yyjson_obj_get(root, "limit_message");
    ck_assert(strstr(yyjson_get_str(limit_message), "Tool call limit reached") != NULL);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST START_TEST(test_tool_loop_limit_end_to_end)
{
    SKIP_IF_NO_DB();

    res_t res = ik_db_message_insert(db, session_id, "user",
                                     "Keep searching for errors in every file", NULL);
    ck_assert(!res.is_err);

    const char *tool_call_id_1 = "call_grep1";
    const char *tool_name = "grep";
    const char *tool_arguments_1 = "{\"pattern\": \"error\", \"path\": \"src/main.c\"}";

    char *tool_call_data_1 = talloc_asprintf(test_ctx,
                                             "{\"id\": \"%s\", \"type\": \"function\", \"function\": {\"name\": \"%s\", \"arguments\": %s}}",
                                             tool_call_id_1,
                                             tool_name,
                                             tool_arguments_1);

    res = ik_db_message_insert(db, session_id, "tool_call", NULL, tool_call_data_1);
    ck_assert(!res.is_err);

    const char *tool_result_1 = "{\"output\": \"src/main.c:12: log_error(...)\", \"count\": 1}";
    ik_message_t *tool_result_msg_1 = ik_msg_create_tool_result(
        test_ctx,
        tool_call_id_1,
        tool_name,
        tool_result_1,
        true,
        "Found 1 match"
        );
    ck_assert_ptr_nonnull(tool_result_msg_1);

    res = ik_db_message_insert(db, session_id, "tool_result",
                               tool_result_msg_1->content,
                               tool_result_msg_1->data_json);
    ck_assert(!res.is_err);

    const char *tool_call_id_2 = "call_grep2";
    char *tool_call_data_2 = talloc_asprintf(test_ctx,
                                             "{\"id\": \"%s\", \"type\": \"function\", \"function\": {\"name\": \"%s\", \"arguments\": "
                                             "{\"pattern\": \"error\", \"path\": \"src/config.c\"}}}",
                                             tool_call_id_2,
                                             tool_name);

    res = ik_db_message_insert(db, session_id, "tool_call", NULL, tool_call_data_2);
    ck_assert(!res.is_err);

    const char *tool_result_2 = "{\"output\": \"src/config.c:45: return CONFIG_ERROR;\", \"count\": 1}";

    ik_message_t *tool_result_msg_2 = ik_msg_create_tool_result(
        test_ctx,
        tool_call_id_2,
        tool_name,
        tool_result_2,
        true,
        "Found 1 match"
        );

    res = ik_db_message_insert(db, session_id, "tool_result",
                               tool_result_msg_2->content,
                               tool_result_msg_2->data_json);
    ck_assert(!res.is_err);

    const char *tool_call_id_3 = "call_grep3";
    char *tool_call_data_3 = talloc_asprintf(test_ctx,
                                             "{\"id\": \"%s\", \"type\": \"function\", \"function\": {\"name\": \"%s\", \"arguments\": "
                                             "{\"pattern\": \"error\", \"path\": \"src/parser.c\"}}}",
                                             tool_call_id_3,
                                             tool_name);

    res = ik_db_message_insert(db, session_id, "tool_call", NULL, tool_call_data_3);
    ck_assert(!res.is_err);

    const char *tool_result_3 = "{\"output\": \"src/parser.c:78: parse_error(line, col);\", \"count\": 1}";
    int32_t max_tool_turns = 3;
    char *tool_result_3_with_limit = ik_tool_result_add_limit_metadata(
        test_ctx, tool_result_3, max_tool_turns);
    yyjson_doc *limit_doc = yyjson_read(tool_result_3_with_limit, strlen(tool_result_3_with_limit), 0);
    ck_assert_ptr_nonnull(limit_doc);
    yyjson_val *limit_root = yyjson_doc_get_root(limit_doc);
    yyjson_val *limit_reached_field = yyjson_obj_get(limit_root, "limit_reached");
    ck_assert_ptr_nonnull(limit_reached_field);
    ck_assert(yyjson_get_bool(limit_reached_field) == true);
    yyjson_doc_free(limit_doc);

    ik_message_t *tool_result_msg_3 = ik_msg_create_tool_result(
        test_ctx,
        tool_call_id_3,
        tool_name,
        tool_result_3_with_limit,
        true,
        "Found 1 match (limit reached)"
        );

    res = ik_db_message_insert(db, session_id, "tool_result",
                               tool_result_msg_3->content,
                               tool_result_msg_3->data_json);
    ck_assert(!res.is_err);

    res = ik_db_message_insert(db,
                               session_id,
                               "assistant",
                               "I searched but reached the tool call limit (3 calls). Found errors in main.c, config.c, parser.c.",
                               "{\"model\": \"gpt-4o-mini\", \"finish_reason\": \"stop\"}");
    ck_assert(!res.is_err);
    ck_assert_int_eq(count_messages(db, session_id, NULL), 8);
    ck_assert_int_eq(count_messages(db, session_id, "user"), 1);
    ck_assert_int_eq(count_messages(db, session_id, "tool_call"), 3);
    ck_assert_int_eq(count_messages(db, session_id, "tool_result"), 3);
    ck_assert_int_eq(count_messages(db, session_id, "assistant"), 1);
    const char *query =
        "SELECT data FROM messages WHERE session_id = $1 AND kind = 'tool_result' ORDER BY id DESC LIMIT 1";
    char *session_id_str = talloc_asprintf(test_ctx, "%lld", (long long)session_id);
    const char *params[] = {session_id_str};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    yyjson_doc *data_doc = yyjson_read(PQgetvalue(result, 0, 0),
                                       strlen(PQgetvalue(result, 0, 0)), 0);
    yyjson_val *output = yyjson_obj_get(yyjson_doc_get_root(data_doc), "output");
    yyjson_doc *output_doc = yyjson_read(yyjson_get_str(output), strlen(yyjson_get_str(output)), 0);
    yyjson_val *limit_flag = yyjson_obj_get(yyjson_doc_get_root(output_doc), "limit_reached");
    ck_assert(yyjson_get_bool(limit_flag) == true);
    yyjson_doc_free(output_doc);
    yyjson_doc_free(data_doc);
    PQclear(result);
}

END_TEST START_TEST(test_request_serialization_with_tool_choice)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;
    cfg->max_tool_turns = 3;

    res_t conv_res = ik_openai_conversation_create(ctx);
    ik_openai_conversation_t *conv = conv_res.ok;
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Search for errors");
    ik_openai_conversation_add_msg(conv, msg_res.ok);
    ik_openai_request_t *request = ik_openai_request_create(ctx, cfg, conv);

    ik_tool_choice_t choice_auto = ik_tool_choice_auto();
    char *json_normal = ik_openai_serialize_request(ctx, request, choice_auto);
    yyjson_doc *doc_normal = yyjson_read(json_normal, strlen(json_normal), 0);
    yyjson_val *choice_normal = yyjson_obj_get(yyjson_doc_get_root(doc_normal), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(choice_normal), "auto");
    yyjson_doc_free(doc_normal);

    ik_tool_choice_t choice_none = ik_tool_choice_none();
    char *json_limit = ik_openai_serialize_request(ctx, request, choice_none);
    yyjson_doc *doc_limit = yyjson_read(json_limit, strlen(json_limit), 0);
    yyjson_val *choice_limit = yyjson_obj_get(yyjson_doc_get_root(doc_limit), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(choice_limit), "none");
    yyjson_doc_free(doc_limit);
    talloc_free(ctx);
}

END_TEST

// ========== Test Suite ==========

static Suite *tool_loop_limit_integration_suite(void)
{
    Suite *s = suite_create("Tool Loop Limit Integration");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);
    tcase_add_test(tc_core, test_tool_result_add_limit_metadata);
    tcase_add_test(tc_core, test_tool_loop_limit_end_to_end);
    tcase_add_test(tc_core, test_request_serialization_with_tool_choice);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = tool_loop_limit_integration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
