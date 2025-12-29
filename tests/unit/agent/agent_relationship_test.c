#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/message.h"
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

    // Add messages to parent using new API
    ik_message_t *msg1 = ik_message_create_text(parent, IK_ROLE_USER, "Hello");
    res = ik_agent_add_message(parent, msg1);
    ck_assert(is_ok(&res));

    ik_message_t *msg2 = ik_message_create_text(parent, IK_ROLE_ASSISTANT, "Hi there");
    res = ik_agent_add_message(parent, msg2);
    ck_assert(is_ok(&res));

    ik_message_t *msg3 = ik_message_create_text(parent, IK_ROLE_ASSISTANT, "With data");
    res = ik_agent_add_message(parent, msg3);
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
    ck_assert_uint_eq(child->message_count, parent->message_count);

    talloc_free(ctx);
}

END_TEST

static Suite *agent_relationship_suite(void)
{
    Suite *s = suite_create("Agent Relationships");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
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
