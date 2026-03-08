/**
 * @file skillset_catalog_test.c
 * @brief Unit tests for the skillset catalog system
 *
 * Tests the ik_skillset_catalog_entry_t struct, catalog management on
 * ik_agent_ctx_t, system prompt block assembly, /clear, /rewind, and /fork.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_basic.h"
#include "apps/ikigai/commands_fork_helpers.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/marks.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

/* ================================================================
 * Helpers
 * ================================================================ */

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Allocate and append a catalog entry directly (helper for tests).
 */
static void add_catalog_entry(ik_agent_ctx_t *agent, const char *name,
                              const char *description, size_t load_position)
{
    ik_skillset_catalog_entry_t *entry = talloc_zero(agent, ik_skillset_catalog_entry_t);
    ck_assert_ptr_nonnull(entry);
    entry->skill_name = talloc_strdup(entry, name);
    entry->description = talloc_strdup(entry, description);
    entry->load_position = load_position;

    agent->skillset_catalog = talloc_realloc(agent, agent->skillset_catalog,
                                             ik_skillset_catalog_entry_t *,
                                             (unsigned int)(agent->skillset_catalog_count + 1));
    ck_assert_ptr_nonnull(agent->skillset_catalog);
    agent->skillset_catalog[agent->skillset_catalog_count] = entry;
    agent->skillset_catalog_count++;
}

/**
 * Create a minimal REPL context for /clear and /rewind tests.
 */
static ik_repl_ctx_t *create_minimal_repl(void *parent)
{
    ik_repl_ctx_t *repl = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = ik_test_create_config(shared);
    shared->logger = ik_logger_create(shared, ".");
    repl->shared = shared;

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = ik_scrollback_create(agent, 80);
    ck_assert_ptr_nonnull(agent->scrollback);
    repl->current = agent;

    return repl;
}

/* ================================================================
 * Tests: struct allocation and field access
 * ================================================================ */

START_TEST(test_catalog_initial_state) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_null(agent->skillset_catalog);
    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
}
END_TEST

START_TEST(test_catalog_add_single_entry) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);

    add_catalog_entry(agent, "database", "PostgreSQL schema and query patterns", 0);

    ck_assert_uint_eq(agent->skillset_catalog_count, 1);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "database");
    ck_assert_str_eq(agent->skillset_catalog[0]->description,
                     "PostgreSQL schema and query patterns");
    ck_assert_uint_eq(agent->skillset_catalog[0]->load_position, 0);
}
END_TEST

START_TEST(test_catalog_add_multiple_entries) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);

    add_catalog_entry(agent, "database", "PostgreSQL schema and query patterns", 0);
    add_catalog_entry(agent, "coverage", "90% coverage requirement and exclusion rules", 5);
    add_catalog_entry(agent, "style", "Code style conventions", 10);

    ck_assert_uint_eq(agent->skillset_catalog_count, 3);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "database");
    ck_assert_str_eq(agent->skillset_catalog[1]->skill_name, "coverage");
    ck_assert_str_eq(agent->skillset_catalog[2]->skill_name, "style");
    ck_assert_uint_eq(agent->skillset_catalog[2]->load_position, 10);
}
END_TEST

/* ================================================================
 * Tests: system prompt block assembly
 * ================================================================ */

START_TEST(test_catalog_empty_produces_no_block) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = NULL;

    ik_request_t *req = NULL;
    res_t res = ik_request_create(test_ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Only the base block (block 0) should be present — no catalog block */
    ck_assert_uint_eq(req->system_block_count, 1);
}
END_TEST

START_TEST(test_catalog_block_built_correctly) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = NULL;

    add_catalog_entry(agent, "database", "PostgreSQL schema and query patterns", 0);
    add_catalog_entry(agent, "coverage", "90% coverage requirement and exclusion rules", 0);

    ik_request_t *req = NULL;
    res_t res = ik_request_create(test_ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Block 0: base prompt. Block 1: catalog block. */
    ck_assert_uint_eq(req->system_block_count, 2);

    const char *catalog_text = req->system_blocks[1].text;
    ck_assert_ptr_nonnull(catalog_text);
    ck_assert(strstr(catalog_text, "## Available Skills") != NULL);
    ck_assert(strstr(catalog_text, "database") != NULL);
    ck_assert(strstr(catalog_text, "PostgreSQL schema and query patterns") != NULL);
    ck_assert(strstr(catalog_text, "coverage") != NULL);
    ck_assert(strstr(catalog_text, "90% coverage requirement") != NULL);
    ck_assert(req->system_blocks[1].cacheable == true);
}
END_TEST

START_TEST(test_catalog_block_after_loaded_skills) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = NULL;

    /* Add a loaded skill */
    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    skill->name = talloc_strdup(skill, "style");
    skill->content = talloc_strdup(skill, "# Style Guide\n\nUse snake_case.");
    skill->load_position = 0;
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    agent->loaded_skills[0] = skill;
    agent->loaded_skill_count = 1;

    /* Add a catalog entry */
    add_catalog_entry(agent, "database", "PostgreSQL schema and query patterns", 0);

    ik_request_t *req = NULL;
    res_t res = ik_request_create(test_ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Block 0: base prompt. Block 1: loaded skill. Block 2: catalog. */
    ck_assert_uint_eq(req->system_block_count, 3);
    ck_assert(strstr(req->system_blocks[1].text, "Style Guide") != NULL);
    ck_assert(strstr(req->system_blocks[2].text, "## Available Skills") != NULL);
    ck_assert(strstr(req->system_blocks[2].text, "database") != NULL);
}
END_TEST

/* ================================================================
 * Tests: /clear drops catalog entries
 * ================================================================ */

START_TEST(test_clear_drops_catalog_entries) {
    ik_repl_ctx_t *repl = create_minimal_repl(test_ctx);

    add_catalog_entry(repl->current, "database", "PostgreSQL schema and query patterns", 0);
    add_catalog_entry(repl->current, "coverage", "90% coverage requirement", 0);
    ck_assert_uint_eq(repl->current->skillset_catalog_count, 2);

    res_t res = ik_cmd_clear(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->skillset_catalog_count, 0);
}
END_TEST

/* ================================================================
 * Tests: /rewind trims catalog entries by load_position
 * ================================================================ */

START_TEST(test_rewind_trims_catalog_entries_past_mark) {
    ik_repl_ctx_t *repl = create_minimal_repl(test_ctx);

    /* Add two messages to the conversation */
    ik_message_t *msg1 = ik_message_create_text(test_ctx, IK_ROLE_USER, "Hello");
    res_t res = ik_agent_add_message(repl->current, msg1);
    ck_assert(is_ok(&res));

    ik_message_t *msg2 = ik_message_create_text(test_ctx, IK_ROLE_ASSISTANT, "Hi");
    res = ik_agent_add_message(repl->current, msg2);
    ck_assert(is_ok(&res));

    /* Create mark at message_count = 2 */
    res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* Add two more messages (indexes 2,3 in conversation) */
    ik_message_t *msg3 = ik_message_create_text(test_ctx, IK_ROLE_USER, "More");
    res = ik_agent_add_message(repl->current, msg3);
    ck_assert(is_ok(&res));

    ik_message_t *msg4 = ik_message_create_text(test_ctx, IK_ROLE_ASSISTANT, "Yes");
    res = ik_agent_add_message(repl->current, msg4);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->message_count, 4);

    /* Add catalog entries in position order (LIFO trimming from end requires this).
     * style(1) is before mark(2) — keep.
     * database(2) is at mark — trim (>= 2).
     * coverage(3) is after mark — trim (>= 2). */
    add_catalog_entry(repl->current, "style", "Code style conventions", 1);
    add_catalog_entry(repl->current, "database", "PostgreSQL schema and query patterns", 2);
    add_catalog_entry(repl->current, "coverage", "90% coverage requirement", 3);
    ck_assert_uint_eq(repl->current->skillset_catalog_count, 3);

    res = ik_mark_rewind_to(repl, "checkpoint");
    ck_assert(is_ok(&res));

    /* Entries with load_position >= 2 (the mark index) should be trimmed.
     * The LIFO trimming removes from the end: coverage(3), database(2).
     * style(1) remains since 1 < 2. */
    ck_assert_uint_eq(repl->current->skillset_catalog_count, 1);
    ck_assert_str_eq(repl->current->skillset_catalog[0]->skill_name, "style");
}
END_TEST

/* ================================================================
 * Tests: /fork copies catalog entries with position reset
 * ================================================================ */

START_TEST(test_fork_copies_catalog_entries) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);

    add_catalog_entry(parent, "database", "PostgreSQL schema and query patterns", 5);
    add_catalog_entry(parent, "coverage", "90% coverage requirement", 10);

    ik_commands_fork_copy_skillset_catalog(child, parent);

    ck_assert_uint_eq(child->skillset_catalog_count, 2);
    ck_assert_str_eq(child->skillset_catalog[0]->skill_name, "database");
    ck_assert_str_eq(child->skillset_catalog[0]->description,
                     "PostgreSQL schema and query patterns");
    ck_assert_uint_eq(child->skillset_catalog[0]->load_position, 0);
    ck_assert_str_eq(child->skillset_catalog[1]->skill_name, "coverage");
    ck_assert_uint_eq(child->skillset_catalog[1]->load_position, 0);
}
END_TEST

START_TEST(test_fork_copy_empty_catalog) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);

    /* Parent has no catalog entries */
    ck_assert_uint_eq(parent->skillset_catalog_count, 0);

    ik_commands_fork_copy_skillset_catalog(child, parent);

    ck_assert_uint_eq(child->skillset_catalog_count, 0);
    ck_assert_ptr_null(child->skillset_catalog);
}
END_TEST

/* ================================================================
 * Suite definition
 * ================================================================ */

static Suite *skillset_catalog_suite(void)
{
    Suite *s = suite_create("Skillset Catalog");

    TCase *tc_struct = tcase_create("Struct");
    tcase_add_checked_fixture(tc_struct, setup, teardown);
    tcase_add_test(tc_struct, test_catalog_initial_state);
    tcase_add_test(tc_struct, test_catalog_add_single_entry);
    tcase_add_test(tc_struct, test_catalog_add_multiple_entries);
    suite_add_tcase(s, tc_struct);

    TCase *tc_prompt = tcase_create("SystemPrompt");
    tcase_add_checked_fixture(tc_prompt, setup, teardown);
    tcase_add_test(tc_prompt, test_catalog_empty_produces_no_block);
    tcase_add_test(tc_prompt, test_catalog_block_built_correctly);
    tcase_add_test(tc_prompt, test_catalog_block_after_loaded_skills);
    suite_add_tcase(s, tc_prompt);

    TCase *tc_clear = tcase_create("Clear");
    tcase_set_timeout(tc_clear, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_clear, setup, teardown);
    tcase_add_test(tc_clear, test_clear_drops_catalog_entries);
    suite_add_tcase(s, tc_clear);

    TCase *tc_rewind = tcase_create("Rewind");
    tcase_set_timeout(tc_rewind, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_rewind, setup, teardown);
    tcase_add_test(tc_rewind, test_rewind_trims_catalog_entries_past_mark);
    suite_add_tcase(s, tc_rewind);

    TCase *tc_fork = tcase_create("Fork");
    tcase_add_checked_fixture(tc_fork, setup, teardown);
    tcase_add_test(tc_fork, test_fork_copies_catalog_entries);
    tcase_add_test(tc_fork, test_fork_copy_empty_catalog);
    suite_add_tcase(s, tc_fork);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = skillset_catalog_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent/skillset_catalog_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
