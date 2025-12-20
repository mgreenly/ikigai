#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Test agent->parent_uuid is NULL for root agent
START_TEST(test_agent_parent_uuid_null_for_root) {
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
    ik_msg_t *msg1 = ik_openai_msg_create(parent->conversation, "user", "Hello");
    res = ik_openai_conversation_add_msg(parent->conversation, msg1);
    ck_assert(is_ok(&res));

    // Message without data_json
    ik_msg_t *msg2 = ik_openai_msg_create(parent->conversation, "assistant", "Hi there");
    res = ik_openai_conversation_add_msg(parent->conversation, msg2);
    ck_assert(is_ok(&res));

    // Message with data_json
    ik_msg_t *msg_with_data = ik_openai_msg_create(parent->conversation, "assistant", "With data");
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

static Suite *agent_relationship_suite(void)
{
    Suite *s = suite_create("Agent Relationships");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_agent_parent_uuid_null_for_root);
    tcase_add_test(tc_core, test_agent_parent_uuid_matches_input);
    tcase_add_test(tc_core, test_agent_copy_conversation);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_relationship_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
