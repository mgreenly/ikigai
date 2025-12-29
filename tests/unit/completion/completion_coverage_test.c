/**
 * @file completion_coverage_test.c
 * @brief Coverage tests for src/completion.c gaps
 */

#include "../../../src/completion.h"
#include "../../../src/commands.h"
#include "../../../src/repl.h"
#include "../../../src/marks.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *test_repl;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal repl context
    test_repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(test_repl);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(test_repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->marks = NULL;
    agent->mark_count = 0;
    test_repl->current = agent;

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    test_repl->shared = shared;

    // Initialize config
    shared->cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(shared->cfg);
    shared->cfg->openai_model = talloc_strdup(shared->cfg, "gpt-4o");
    ck_assert_ptr_nonnull(shared->cfg->openai_model);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Coverage for line 310 false branch - /model without slash
START_TEST(test_model_completion_without_slash)
{
    // "/model " should complete model names (no slash, so false branch at line 310)
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/model ");
    ck_assert_ptr_nonnull(comp);
    ck_assert(comp->count > 0);

    // Verify we got model names, not thinking levels
    bool found_model = false;
    for (size_t i = 0; i < comp->count; i++) {
        if (strstr(comp->candidates[i], "claude") != NULL ||
            strstr(comp->candidates[i], "gpt") != NULL ||
            strstr(comp->candidates[i], "gemini") != NULL) {
            found_model = true;
            break;
        }
    }
    ck_assert(found_model);

    talloc_free(comp);
}

END_TEST

// Test: Coverage for line 310 true branch - /model with slash
START_TEST(test_model_completion_with_slash)
{
    // "/model gpt-4o/" should complete thinking levels (slash present, so true branch at line 310)
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/model gpt-4o/");
    ck_assert_ptr_nonnull(comp);
    ck_assert(comp->count > 0);

    // Verify we got thinking levels
    bool found_thinking = false;
    for (size_t i = 0; i < comp->count; i++) {
        if (strcmp(comp->candidates[i], "none") == 0 ||
            strcmp(comp->candidates[i], "low") == 0 ||
            strcmp(comp->candidates[i], "med") == 0 ||
            strcmp(comp->candidates[i], "high") == 0) {
            found_thinking = true;
            break;
        }
    }
    ck_assert(found_thinking);

    talloc_free(comp);
}

END_TEST

// Test: Coverage for lines 136-141 - rewind args with labeled marks
START_TEST(test_rewind_completion_with_labeled_marks)
{
    // Create marks with labels (lines 136-141 collect these)
    ik_mark_t *mark1 = talloc_zero(test_repl, ik_mark_t);
    mark1->label = talloc_strdup(mark1, "checkpoint1");
    mark1->message_index = 0;

    ik_mark_t *mark2 = talloc_zero(test_repl, ik_mark_t);
    mark2->label = talloc_strdup(mark2, "goodstate");
    mark2->message_index = 5;

    ik_mark_t *mark3 = talloc_zero(test_repl, ik_mark_t);
    mark3->label = talloc_strdup(mark3, "before_refactor");
    mark3->message_index = 10;

    test_repl->current->marks = talloc_array(test_repl, ik_mark_t *, 3);
    test_repl->current->marks[0] = mark1;
    test_repl->current->marks[1] = mark2;
    test_repl->current->marks[2] = mark3;
    test_repl->current->mark_count = 3;

    // "/rewind " should return all labeled marks
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/rewind ");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 3);

    // Verify all mark labels are present
    bool found_cp1 = false, found_good = false, found_refactor = false;
    for (size_t i = 0; i < comp->count; i++) {
        if (strcmp(comp->candidates[i], "checkpoint1") == 0) found_cp1 = true;
        if (strcmp(comp->candidates[i], "goodstate") == 0) found_good = true;
        if (strcmp(comp->candidates[i], "before_refactor") == 0) found_refactor = true;
    }
    ck_assert(found_cp1);
    ck_assert(found_good);
    ck_assert(found_refactor);

    talloc_free(comp);
}

END_TEST

static Suite *completion_coverage_suite(void)
{
    Suite *s = suite_create("Completion Coverage");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_model_completion_without_slash);
    tcase_add_test(tc, test_model_completion_with_slash);
    tcase_add_test(tc, test_rewind_completion_with_labeled_marks);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = completion_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
